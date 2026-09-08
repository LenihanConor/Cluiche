#include "DiaPipelineEditor/PipelineEditorPlugin.h"
#include "DiaPipelineEditor/PipelineLogTailer.h"
#include "DiaPipelineEditor/PipelineBuildManager.h"
#include "DiaPipelineEditor/RunHistoryStore.h"
#include "DiaPipelineEditor/Internal/PipelineTargetParser.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <cstdio>
#include <windows.h>
#include <shellapi.h>

using namespace Dia::PipelineEditor;

static const char* kExePaths[] = {
	"Cluiche/bin/CluicheTest/%s/x64/CluicheTest.exe",
	"Cluiche/bin/GoogleTests/%s/x64/GoogleTests.exe",
	"Cluiche/bin/CluicheEditor/%s/x64/CluicheEditor.exe",
};
static const char* kExeTargets[] = { "cluichetest", "googletest", "cluicheeditor" };

static bool ExeExistsForTarget(const char* repoRoot, const char* target, const char* config)
{
	if (!target || target[0] == '\0' || !config || config[0] == '\0')
		return false;
	for (int i = 0; i < 3; ++i)
	{
		if (_stricmp(target, kExeTargets[i]) == 0)
		{
			char path[1024];
			snprintf(path, sizeof(path), kExePaths[i], config);
			char full[1024];
			snprintf(full, sizeof(full), "%s/%s", repoRoot, path);
			return GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES;
		}
	}
	return false;
}

static const Dia::Core::StringCRC kCmdPipelineStart("pipeline.start");
static const Dia::Core::StringCRC kCmdPipelineCancel("pipeline.cancel");
static const Dia::Core::StringCRC kCmdPipelineGetTargets("pipeline.get-targets");
static const Dia::Core::StringCRC kCmdPipelineHistory("pipeline.history");
static const Dia::Core::StringCRC kCmdPipelineGetProjectState("pipeline.get_project_state");
static const Dia::Core::StringCRC kCmdPipelineGetTargetStages("pipeline.get_target_stages");
static const Dia::Core::StringCRC kCmdPipelineLaunch("pipeline.launch");
static const Dia::Core::StringCRC kCmdPipelineOpenLogsFolder("pipeline.open-logs-folder");

PipelineEditorPlugin::PipelineEditorPlugin()
	: EditorPluginBase({
		"Pipeline Editor",
		"1.0.0",
		"Live pipeline viewer and build trigger",
		"dia://plugins/diapipelineeditor/index.html",
		Dia::Editor::LayoutMode::kDockable,
		nullptr,
		nullptr,
		true
	})
	, mTailer(nullptr)
	, mBuildManager(nullptr)
	, mHistoryStore(nullptr)
	, mLastPushedEventIndex(0)
	, mLastBuildRunning(false)
	, mLastExitCode(0)
	, mLaunchProcess(NULL)
	, mLaunchStdoutRead(NULL)
	, mLaunchStdoutFile(nullptr)
{
	mRepoRoot[0]     = '\0';
	mDiagamePath[0]  = '\0';
	mLaunchTarget[0] = '\0';
}

PipelineEditorPlugin::~PipelineEditorPlugin()
{
}

void PipelineEditorPlugin::OnPluginLoad()
{
	// Walk up from exe path to find the repo root (contains pipeline.toml)
	{
		char exePath[512];
		GetModuleFileNameA(NULL, exePath, sizeof(exePath));
		char* sep = strrchr(exePath, '\\');
		if (sep) *sep = '\0';
		strncpy_s(mRepoRoot, sizeof(mRepoRoot), exePath, _TRUNCATE);

		for (int i = 0; i < 8; ++i)
		{
			char probe[512 + 16];
			snprintf(probe, sizeof(probe), "%s\\pipeline.toml", mRepoRoot);
			DWORD attr = GetFileAttributesA(probe);
			if (attr != INVALID_FILE_ATTRIBUTES)
				break;
			char* up = strrchr(mRepoRoot, '\\');
			if (up == nullptr) break;
			*up = '\0';
		}
	}

	mTailer = new PipelineLogTailer();
	char logPath[1024];
	snprintf(logPath, sizeof(logPath), "%s/Cluiche/out/DiaCLI/logs/pipeline/last-run.ndjson", mRepoRoot);
	mTailer->Initialize(logPath);
	mTailer->RegisterObserver(this);

	mBuildManager = new PipelineBuildManager();
	mBuildManager->Initialize(mTailer, mRepoRoot);

	mHistoryStore = new RunHistoryStore();
	mHistoryStore->Initialize();

	RegisterCommands();
}

void PipelineEditorPlugin::OnPluginUnload()
{
	CleanupLaunchProcess();

	if (mHistoryStore)
	{
		mHistoryStore->Shutdown();
		delete mHistoryStore;
		mHistoryStore = nullptr;
	}

	if (mBuildManager)
	{
		mBuildManager->Shutdown();
		delete mBuildManager;
		mBuildManager = nullptr;
	}

	if (mTailer)
	{
		mTailer->UnregisterObserver(this);
		mTailer->Shutdown();
		delete mTailer;
		mTailer = nullptr;
	}
}

void PipelineEditorPlugin::ExtractTarget(const char* diagamePath, char* targetOut, size_t targetSize)
{
	if (targetSize == 0)
		return;
	targetOut[0] = '\0';

	if (diagamePath == nullptr || diagamePath[0] == '\0')
		return;

	// Find last '/' or '\'
	const char* lastSlash = nullptr;
	for (const char* p = diagamePath; *p != '\0'; ++p)
	{
		if (*p == '/' || *p == '\\')
			lastSlash = p;
	}

	const char* nameStart = (lastSlash != nullptr) ? lastSlash + 1 : diagamePath;

	// Copy up to (but not including) the first '.'
	size_t written = 0;
	for (const char* p = nameStart; *p != '\0' && *p != '.' && written + 1 < targetSize; ++p, ++written)
		targetOut[written] = *p;
	targetOut[written] = '\0';
}

void PipelineEditorPlugin::OnNavigate(const Dia::Core::StringCRC& /*instanceId*/)
{
	if (!GetBridge())
		return;

	char target[256] = {};
	if (mDiagamePath[0] != '\0')
		ExtractTarget(mDiagamePath, target, sizeof(target));

	Json::Value payload;
	payload["isValid"]     = mDiagamePath[0] != '\0';
	payload["diagamePath"] = mDiagamePath;
	payload["target"]      = target;
	payload["diagameName"] = target;
	payload["exeExists"]   = ExeExistsForTarget(mRepoRoot, target, "Debug");
	GetBridge()->NotifyUIDataChanged("pipeline.project_changed", payload);
}

void PipelineEditorPlugin::OnProjectChanged(const Dia::Editor::ProjectContext& ctx)
{
	strncpy_s(mDiagamePath, kDiagamePathLength,
	          ctx.IsValid() ? ctx.diagamePath : "", _TRUNCATE);

	if (GetBridge())
	{
		char target[256] = {};
		if (ctx.IsValid())
			ExtractTarget(ctx.diagamePath, target, sizeof(target));

		Json::Value payload;
		payload["isValid"]     = ctx.IsValid();
		payload["diagamePath"] = ctx.diagamePath;
		payload["target"]      = target;
		payload["diagameName"] = target;   // alias: JS reads diagameName
		payload["exeExists"]   = ExeExistsForTarget(mRepoRoot, target, "Debug");
		GetBridge()->NotifyUIDataChanged("pipeline.project_changed", payload);
	}
}

void PipelineEditorPlugin::OnUpdate(float /*deltaTime*/)
{
	if (mBuildManager)
	{
		mBuildManager->Update();
	}

	if (mTailer)
	{
		mTailer->Poll();
	}

	PollLaunchProcess();

	// Push build running state to UI only when it changes
	if (GetBridge() && mBuildManager)
	{
		bool running = mBuildManager->IsBuildRunning();
		int exitCode = mBuildManager->GetLastExitCode();
		if (running != mLastBuildRunning || exitCode != mLastExitCode)
		{
			mLastBuildRunning = running;
			mLastExitCode = exitCode;
			Json::Value status;
			status["buildRunning"] = running;
			status["lastExitCode"] = exitCode;
			GetBridge()->NotifyUIDataChanged("pipeline.build-status", status);
		}
	}
}

void PipelineEditorPlugin::ObserverNotification(const Dia::Core::ObserverSubject* /*subject*/, int message)
{
	if (message == PipelineLogTailer::kRunStarted)
	{
		mLastPushedEventIndex = 0;
	}

	if (message == PipelineLogTailer::kRunCompleted || message == PipelineLogTailer::kRunInterrupted)
	{
		if (mHistoryStore && mTailer)
		{
			mHistoryStore->RecordRun(mTailer->GetCurrentRunSummary());
			if (GetBridge())
			{
				Json::Value historyPayload;
				historyPayload["runs"] = mHistoryStore->ToJson();
				GetBridge()->NotifyUIDataChanged("pipeline.history", historyPayload);
			}
		}
	}

	PushEventsToUI();
}

void PipelineEditorPlugin::PushEventsToUI()
{
	if (GetBridge() == nullptr || mTailer == nullptr)
		return;

	int totalEvents = mTailer->GetEventCount();
	if (mLastPushedEventIndex >= totalEvents)
		return;

	Json::Value eventsArray(Json::arrayValue);
	for (int i = mLastPushedEventIndex; i < totalEvents; ++i)
	{
		const PipelineEvent& evt = mTailer->GetEvent(i);
		Json::Value entry;
		entry["event"] = evt.eventType.AsChar();
		entry["system"] = evt.system.AsChar();
		entry["stage"] = evt.stage.AsChar();
		entry["step"] = evt.step.AsChar();
		entry["ts"] = static_cast<double>(evt.timestampSec);
		entry["durationMs"] = evt.durationMs;
		if (evt.error)
			entry["error"] = evt.error;
		if (evt.detail)
			entry["detail"] = evt.detail;
		if (evt.level)
			entry["level"] = evt.level;
		eventsArray.append(entry);
	}
	mLastPushedEventIndex = totalEvents;

	const RunSummary& run = mTailer->GetCurrentRunSummary();
	const char* summaryConfig = run.config.AsChar()[0] ? run.config.AsChar() : "Debug";
	Json::Value summary;
	summary["target"] = run.target.AsChar();
	summary["config"] = summaryConfig;
	summary["passCount"] = run.passCount;
	summary["failCount"] = run.failCount;
	summary["totalDurationMs"] = run.totalDurationMs;
	summary["startTimestamp"] = static_cast<double>(run.startTimestamp);
	summary["interrupted"] = run.interrupted;
	summary["runInProgress"] = mTailer->IsRunInProgress();
	summary["exeExists"] = ExeExistsForTarget(mRepoRoot, run.target.AsChar(), summaryConfig);

	Json::Value payload;
	payload["events"] = eventsArray;
	payload["summary"] = summary;

	GetBridge()->NotifyUIDataChanged("pipeline.event", payload);
}

void PipelineEditorPlugin::RegisterCommands()
{
	RegisterHandler(kCmdPipelineStart,
		[this](const Json::Value& data) -> Json::Value
		{
			Json::Value result;
			const char* config = data.isMember("config") ? data["config"].asCString() : "Debug";
			const char* target = data.isMember("target") ? data["target"].asCString() : "googletest";
			const char* stages = data.isMember("stages") ? data["stages"].asCString() : nullptr;
			bool force = data.isMember("force") && data["force"].asBool();

			int err = mBuildManager->Start(config, target, stages, force);
			result["ok"] = (err == 0);
			if (err != 0)
				result["error"] = "Build already running or failed to start";
			return result;
		});

	RegisterHandler(kCmdPipelineCancel,
		[this](const Json::Value&) -> Json::Value
		{
			mBuildManager->Cancel();
			Json::Value result;
			result["ok"] = true;
			return result;
		});

	RegisterHandler(kCmdPipelineGetTargets,
		[this](const Json::Value&) -> Json::Value
		{
			char tomlPath[1024];
			snprintf(tomlPath, sizeof(tomlPath), "%s/pipeline.toml", mRepoRoot);

			Json::Value result;
			result["targets"] = Internal::ParsePipelineTargets(tomlPath);
			return result;
		});

	RegisterHandler(kCmdPipelineHistory,
		[this](const Json::Value&) -> Json::Value
		{
			Json::Value result;
			result["runs"] = mHistoryStore->ToJson();
			return result;
		});

	RegisterHandler(kCmdPipelineGetProjectState,
		[this](const Json::Value& /*data*/) -> Json::Value
		{
			char target[256] = {};
			if (mDiagamePath[0] != '\0')
				ExtractTarget(mDiagamePath, target, sizeof(target));

			Json::Value result;
			result["isValid"]     = mDiagamePath[0] != '\0';
			result["diagamePath"] = mDiagamePath;
			result["target"]      = target;
			result["diagameName"] = target;   // alias: JS reads diagameName
			result["exeExists"]   = ExeExistsForTarget(mRepoRoot, target, "Debug");
			return result;
		});

	RegisterHandler(kCmdPipelineGetTargetStages,
		[this](const Json::Value& /*data*/) -> Json::Value
		{
			char target[256] = {};
			if (mDiagamePath[0] != '\0')
				ExtractTarget(mDiagamePath, target, sizeof(target));

			char tomlPath[1024];
			snprintf(tomlPath, sizeof(tomlPath), "%s/pipeline.toml", mRepoRoot);

			Json::Value result;
			result["stages"] = Internal::ParseTargetStages(tomlPath, target);
			return result;
		});

	RegisterHandler(kCmdPipelineLaunch,
		[this](const Json::Value& /*data*/) -> Json::Value
		{
			if (mLaunchProcess != NULL)
				return MakeErrorResponse("launch already in progress");

			char target[256] = {};
			if (mDiagamePath[0] != '\0')
				ExtractTarget(mDiagamePath, target, sizeof(target));

			if (target[0] == '\0')
				return MakeErrorResponse("no diagame project loaded");

			// Open log file
			char logDir[1024];
			snprintf(logDir, sizeof(logDir), "%s/Cluiche/out/DiaCLI/logs/launch", mRepoRoot);
			CreateDirectoryA(logDir, NULL);

			char logPath[1100];
			snprintf(logPath, sizeof(logPath), "%s/last-stdout.log", logDir);
			mLaunchStdoutFile = nullptr;
			fopen_s(&mLaunchStdoutFile, logPath, "wb");
			if (!mLaunchStdoutFile)
				DIA_LOG_WARNING("PipelineEditor", "launch: could not open log file %s", logPath);

			// Pipe for stdout/stderr
			HANDLE stdoutWrite = NULL;
			SECURITY_ATTRIBUTES sa = {};
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = TRUE;
			if (!CreatePipe(&mLaunchStdoutRead, &stdoutWrite, &sa, 0))
			{
				DWORD err = GetLastError();
				DIA_LOG_WARNING("PipelineEditor", "launch: CreatePipe failed (error %lu)", err);
				if (mLaunchStdoutFile) { fclose(mLaunchStdoutFile); mLaunchStdoutFile = nullptr; }
				return MakeErrorResponse("CreatePipe failed");
			}
			SetHandleInformation(mLaunchStdoutRead, HANDLE_FLAG_INHERIT, 0);

			char cmdLine[1024];
			snprintf(cmdLine, sizeof(cmdLine),
				"\"%s/Dia/DiaCLI/.venv/Scripts/python.exe\" -m dia_cli launch %s",
				mRepoRoot, target);

			STARTUPINFOA si = {};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdOutput = stdoutWrite;
			si.hStdError  = stdoutWrite;
			si.hStdInput  = NULL;

			char workingDir[512];
			snprintf(workingDir, sizeof(workingDir), "%s/Dia/DiaCLI", mRepoRoot);

			PROCESS_INFORMATION pi = {};
			BOOL ok = CreateProcessA(nullptr, cmdLine, nullptr, nullptr,
				TRUE, CREATE_NO_WINDOW, nullptr, workingDir, &si, &pi);

			CloseHandle(stdoutWrite);

			if (!ok)
			{
				DWORD err = GetLastError();
				DIA_LOG_WARNING("PipelineEditor", "launch: CreateProcessA failed (error %lu) cmd: %s", err, cmdLine);
				CloseHandle(mLaunchStdoutRead);
				mLaunchStdoutRead = NULL;
				if (mLaunchStdoutFile) { fclose(mLaunchStdoutFile); mLaunchStdoutFile = nullptr; }
				char errMsg[128];
				snprintf(errMsg, sizeof(errMsg), "CreateProcessA failed (error %lu)", err);
				return MakeErrorResponse(errMsg);
			}

			mLaunchProcess = pi.hProcess;
			CloseHandle(pi.hThread);
			strncpy_s(mLaunchTarget, sizeof(mLaunchTarget), target, _TRUNCATE);

			DIA_LOG_INFO("PipelineEditor", "launch: started %s (pid %lu) — log: %s",
				target, pi.dwProcessId, logPath);

			return MakeSuccessResponse();
		});

	RegisterHandler(kCmdPipelineOpenLogsFolder,
		[this](const Json::Value& /*data*/) -> Json::Value
		{
			char dirPath[1024];
			snprintf(dirPath, sizeof(dirPath), "%s\\Cluiche\\out\\DiaCLI\\logs\\pipeline", mRepoRoot);

			DWORD attr = GetFileAttributesA(dirPath);
			if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
				return MakeErrorResponse("Log directory does not exist");

			ShellExecuteA(NULL, "explore", dirPath, NULL, NULL, SW_SHOWDEFAULT);
			return MakeSuccessResponse();
		});
}

void PipelineEditorPlugin::PollLaunchProcess()
{
	if (mLaunchProcess == NULL)
		return;

	DrainLaunchPipe();

	DWORD exitCode = 0;
	if (GetExitCodeProcess(mLaunchProcess, &exitCode) && exitCode != STILL_ACTIVE)
	{
		DrainLaunchPipe();

		// Flush log before reading it back
		if (mLaunchStdoutFile)
		{
			fflush(mLaunchStdoutFile);
			fclose(mLaunchStdoutFile);
			mLaunchStdoutFile = nullptr;
		}

		if (exitCode == 0)
		{
			DIA_LOG_INFO("PipelineEditor", "launch: %s exited cleanly (exit 0)", mLaunchTarget);
		}
		else
		{
			DIA_LOG_WARNING("PipelineEditor", "launch: %s exited with code %lu — see Cluiche/out/DiaCLI/logs/launch/last-stdout.log", mLaunchTarget, exitCode);
		}

		if (GetBridge())
		{
			// Read last few lines from stdout log for the toast
			char logPath[1100];
			snprintf(logPath, sizeof(logPath), "%s/Cluiche/out/DiaCLI/logs/launch/last-stdout.log", mRepoRoot);

			char lastLines[512] = {};
			FILE* f = nullptr;
			fopen_s(&f, logPath, "rb");
			if (f)
			{
				fseek(f, 0, SEEK_END);
				long sz = ftell(f);
				long readStart = sz > 480 ? sz - 480 : 0;
				fseek(f, readStart, SEEK_SET);
				size_t n = fread(lastLines, 1, sizeof(lastLines) - 1, f);
				lastLines[n] = '\0';
				fclose(f);
				// Strip leading partial line if we didn't start at 0
				char* lineStart = readStart > 0 ? strchr(lastLines, '\n') : lastLines;
				if (lineStart && lineStart != lastLines)
					memmove(lastLines, lineStart + 1, strlen(lineStart));
			}

			Json::Value status;
			status["target"]   = mLaunchTarget;
			status["exitCode"] = static_cast<int>(exitCode);
			status["output"]   = lastLines;
			GetBridge()->NotifyUIDataChanged("pipeline.launch-status", status);
		}

		// Null out file handle — already closed above
		mLaunchStdoutFile = nullptr;
		CleanupLaunchProcess();
	}
}

void PipelineEditorPlugin::DrainLaunchPipe()
{
	if (mLaunchStdoutRead == NULL)
		return;

	static const DWORD kBufSize = 4096;
	char buf[kBufSize];

	for (;;)
	{
		DWORD available = 0;
		if (!PeekNamedPipe(mLaunchStdoutRead, NULL, 0, NULL, &available, NULL))
			break;
		if (available == 0)
			break;
		DWORD toRead = available < kBufSize ? available : kBufSize;
		DWORD bytesRead = 0;
		if (!ReadFile(mLaunchStdoutRead, buf, toRead, &bytesRead, NULL) || bytesRead == 0)
			break;
		if (mLaunchStdoutFile && bytesRead > 0)
			fwrite(buf, 1, bytesRead, mLaunchStdoutFile);
	}
}

void PipelineEditorPlugin::CleanupLaunchProcess()
{
	if (mLaunchProcess != NULL) { CloseHandle(mLaunchProcess); mLaunchProcess = NULL; }
	if (mLaunchStdoutRead != NULL) { CloseHandle(mLaunchStdoutRead); mLaunchStdoutRead = NULL; }
	if (mLaunchStdoutFile != nullptr)
	{
		fflush(mLaunchStdoutFile);
		fclose(mLaunchStdoutFile);
		mLaunchStdoutFile = nullptr;
	}
	mLaunchTarget[0] = '\0';
}

REGISTER_EDITOR_PLUGIN(PipelineEditorPlugin, "DiaPipelineEditor")
