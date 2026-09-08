#include "DiaPipelineEditor/PipelineBuildManager.h"
#include "DiaPipelineEditor/PipelineLogTailer.h"
#include <DiaObservation/Log/DiaLog.h>

#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace PipelineEditor
	{
		PipelineBuildManager::PipelineBuildManager()
			: mTailer(nullptr)
			, mProcessHandle(NULL)
			, mThreadHandle(NULL)
			, mStdoutReadHandle(NULL)
			, mStdoutFile(nullptr)
			, mBuildRunning(false)
			, mLastExitCode(0)
		{
			mRepoRoot[0] = '\0';
			mStdoutLogPath[0] = '\0';
		}

		PipelineBuildManager::~PipelineBuildManager()
		{
			Shutdown();
		}

		void PipelineBuildManager::Initialize(PipelineLogTailer* tailer, const char* repoRoot)
		{
			mTailer = tailer;
			strncpy_s(mRepoRoot, sizeof(mRepoRoot), repoRoot, _TRUNCATE);
		}

		void PipelineBuildManager::Shutdown()
		{
			if (mBuildRunning)
				Cancel();
			CleanupProcess();
			mTailer = nullptr;
		}

		int PipelineBuildManager::Start(const char* config, const char* target,
		                                const char* stages, bool force)
		{
			if (mBuildRunning)
			{
				DIA_LOG_WARNING("PipelineEditor", "Build already running — cannot start another");
				return -1;
			}

			// Build the log output path and ensure its directory exists
			snprintf(mStdoutLogPath, sizeof(mStdoutLogPath),
				"%s/Cluiche/out/DiaCLI/logs/pipeline/last-stdout.log",
				mRepoRoot);

			// Create intermediate directories if needed
			{
				char dirBuf[1024];
				// Level 1: {repoRoot}/Cluiche/out/DiaCLI/logs
				snprintf(dirBuf, sizeof(dirBuf), "%s/Cluiche/out/DiaCLI/logs", mRepoRoot);
				CreateDirectoryA(dirBuf, NULL);
				// Level 2: {repoRoot}/Cluiche/out/DiaCLI/logs/pipeline
				snprintf(dirBuf, sizeof(dirBuf), "%s/Cluiche/out/DiaCLI/logs/pipeline", mRepoRoot);
				CreateDirectoryA(dirBuf, NULL);
			}

			// Open the log file (truncate on each new build)
			mStdoutFile = nullptr;
			fopen_s(&mStdoutFile, mStdoutLogPath, "wb");
			if (!mStdoutFile)
			{
				DIA_LOG_WARNING("PipelineEditor", "Failed to open stdout log: %s", mStdoutLogPath);
				// Non-fatal: proceed without capture
			}

			// Create anonymous pipe for stdout/stderr capture
			HANDLE stdoutWriteHandle = NULL;
			SECURITY_ATTRIBUTES sa = {};
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = TRUE;  // write end is inherited by child
			sa.lpSecurityDescriptor = NULL;

			if (!CreatePipe(&mStdoutReadHandle, &stdoutWriteHandle, &sa, 0))
			{
				DWORD err = GetLastError();
				DIA_LOG_WARNING("PipelineEditor", "CreatePipe failed (error %lu)", err);
				if (mStdoutFile) { fclose(mStdoutFile); mStdoutFile = nullptr; }
				return static_cast<int>(err);
			}

			// Make the read end non-inheritable so the child doesn't get it
			SetHandleInformation(mStdoutReadHandle, HANDLE_FLAG_INHERIT, 0);

			// Build the command line
			char cmdLine[2048];
			int written = snprintf(cmdLine, sizeof(cmdLine),
				"\"%s/Dia/DiaCLI/.venv/Scripts/python.exe\" -m dia_cli pipeline --config %s --target %s",
				mRepoRoot, config ? config : "Debug", target ? target : "googletest");

			if (stages != nullptr && stages[0] != '\0')
			{
				written += snprintf(cmdLine + written, sizeof(cmdLine) - written,
					" --stage %s", stages);
			}

			if (force)
			{
				snprintf(cmdLine + written, sizeof(cmdLine) - written, " --force");
			}

			STARTUPINFOA si = {};
			si.cb = sizeof(si);
			si.dwFlags |= STARTF_USESTDHANDLES;
			si.hStdOutput = stdoutWriteHandle;
			si.hStdError  = stdoutWriteHandle;
			si.hStdInput  = NULL;

			PROCESS_INFORMATION pi = {};

			char workingDir[512];
			snprintf(workingDir, sizeof(workingDir), "%s/Dia/DiaCLI", mRepoRoot);

			BOOL ok = CreateProcessA(
				NULL,
				cmdLine,
				NULL, NULL,
				TRUE,               // inherit handles so child gets the write end
				CREATE_NO_WINDOW,
				NULL,
				workingDir,
				&si, &pi
			);

			// Close the write end in the parent — child owns it now; if we keep it
			// open ReadFile would block forever waiting for more data.
			CloseHandle(stdoutWriteHandle);

			if (!ok)
			{
				DWORD err = GetLastError();
				DIA_LOG_WARNING("PipelineEditor", "CreateProcess failed (error %lu): %s", err, cmdLine);
				CloseHandle(mStdoutReadHandle);
				mStdoutReadHandle = NULL;
				if (mStdoutFile) { fclose(mStdoutFile); mStdoutFile = nullptr; }
				return static_cast<int>(err);
			}

			mProcessHandle = pi.hProcess;
			mThreadHandle  = pi.hThread;
			mBuildRunning  = true;
			mLastExitCode  = 0;

			return 0;
		}

		void PipelineBuildManager::Cancel()
		{
			if (!mBuildRunning || mProcessHandle == NULL)
				return;

			DrainPipe();
			TerminateProcess(mProcessHandle, 1);
			WaitForSingleObject(mProcessHandle, 1000);
			CleanupProcess();
			mBuildRunning = false;
			mLastExitCode = 1;

			DIA_LOG_WARNING("PipelineEditor", "Build cancelled by user");
		}

		void PipelineBuildManager::Update()
		{
			if (!mBuildRunning || mProcessHandle == NULL)
				return;

			// Drain whatever the child has written so far (non-blocking)
			DrainPipe();

			DWORD exitCode = 0;
			if (GetExitCodeProcess(mProcessHandle, &exitCode))
			{
				if (exitCode != STILL_ACTIVE)
				{
					// Final drain before closing
					DrainPipe();

					mLastExitCode = static_cast<int>(exitCode);
					mBuildRunning = false;
					CleanupProcess();

					if (mLastExitCode != 0)
					{
						DIA_LOG_WARNING("PipelineEditor", "Build subprocess exited with code %d", mLastExitCode);
					}
				}
			}
		}

		bool PipelineBuildManager::IsBuildRunning() const
		{
			return mBuildRunning;
		}

		int PipelineBuildManager::GetLastExitCode() const
		{
			return mLastExitCode;
		}

		const char* PipelineBuildManager::GetStdoutLogPath() const
		{
			return mStdoutLogPath;
		}

		void PipelineBuildManager::DrainPipe()
		{
			if (mStdoutReadHandle == NULL)
				return;

			static const DWORD kBufSize = 4096;
			char buf[kBufSize];

			for (;;)
			{
				DWORD available = 0;
				if (!PeekNamedPipe(mStdoutReadHandle, NULL, 0, NULL, &available, NULL))
					break;  // pipe broken — child exited

				if (available == 0)
					break;  // nothing buffered right now

				DWORD toRead = available < kBufSize ? available : kBufSize;
				DWORD bytesRead = 0;
				if (!ReadFile(mStdoutReadHandle, buf, toRead, &bytesRead, NULL) || bytesRead == 0)
					break;

				if (mStdoutFile && bytesRead > 0)
					fwrite(buf, 1, bytesRead, mStdoutFile);
			}
		}

		void PipelineBuildManager::CleanupProcess()
		{
			if (mProcessHandle != NULL)
			{
				CloseHandle(mProcessHandle);
				mProcessHandle = NULL;
			}
			if (mThreadHandle != NULL)
			{
				CloseHandle(mThreadHandle);
				mThreadHandle = NULL;
			}
			if (mStdoutReadHandle != NULL)
			{
				CloseHandle(mStdoutReadHandle);
				mStdoutReadHandle = NULL;
			}
			if (mStdoutFile != nullptr)
			{
				fflush(mStdoutFile);
				fclose(mStdoutFile);
				mStdoutFile = nullptr;
			}
		}
	}
}
