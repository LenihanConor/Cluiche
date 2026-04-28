#include "DiaPipelineEditor/PipelineBuildManager.h"
#include "DiaPipelineEditor/PipelineLogTailer.h"
#include <DiaLogger/DiaLog.h>

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
			, mBuildRunning(false)
			, mLastExitCode(0)
		{
			mRepoRoot[0] = '\0';
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
			PROCESS_INFORMATION pi = {};

			char workingDir[512];
			snprintf(workingDir, sizeof(workingDir), "%s/Dia/DiaCLI", mRepoRoot);

			BOOL ok = CreateProcessA(
				NULL,
				cmdLine,
				NULL, NULL,
				FALSE,
				CREATE_NO_WINDOW,
				NULL,
				workingDir,
				&si, &pi
			);

			if (!ok)
			{
				DWORD err = GetLastError();
				DIA_LOG_WARNING("PipelineEditor", "CreateProcess failed (error %lu): %s", err, cmdLine);
				return static_cast<int>(err);
			}

			mProcessHandle = pi.hProcess;
			mThreadHandle = pi.hThread;
			mBuildRunning = true;
			mLastExitCode = 0;

			return 0;
		}

		void PipelineBuildManager::Cancel()
		{
			if (!mBuildRunning || mProcessHandle == NULL)
				return;

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

			DWORD exitCode = 0;
			if (GetExitCodeProcess(mProcessHandle, &exitCode))
			{
				if (exitCode != STILL_ACTIVE)
				{
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
		}
	}
}
