#include "DiaPipelineEditor/PipelineEditorPlugin.h"
#include "DiaPipelineEditor/PipelineLogTailer.h"
#include "DiaPipelineEditor/PipelineBuildManager.h"
#include "DiaPipelineEditor/RunHistoryStore.h"
#include "DiaPipelineEditor/Internal/PipelineTargetParser.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <cstdio>
#include <windows.h>
#include <shellapi.h>

using namespace Dia::PipelineEditor;

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
{
	mRepoRoot[0]     = '\0';
	mDiagamePath[0]  = '\0';
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
	Json::Value summary;
	summary["target"] = run.target.AsChar();
	summary["config"] = run.config.AsChar();
	summary["passCount"] = run.passCount;
	summary["failCount"] = run.failCount;
	summary["totalDurationMs"] = run.totalDurationMs;
	summary["startTimestamp"] = static_cast<double>(run.startTimestamp);
	summary["interrupted"] = run.interrupted;
	summary["runInProgress"] = mTailer->IsRunInProgress();

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
			char target[256] = {};
			if (mDiagamePath[0] != '\0')
				ExtractTarget(mDiagamePath, target, sizeof(target));

			if (target[0] == '\0')
				return MakeErrorResponse("no diagame project loaded");

			char cmdLine[1024];
			snprintf(cmdLine, sizeof(cmdLine),
				"\"%s/Dia/DiaCLI/.venv/Scripts/python.exe\" -m dia_cli launch %s",
				mRepoRoot, target);

			STARTUPINFOA si = {};
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi = {};

			BOOL ok = CreateProcessA(
				nullptr,
				cmdLine,
				nullptr, nullptr,
				FALSE,
				CREATE_NO_WINDOW,
				nullptr, nullptr,
				&si, &pi);

			if (!ok)
			{
				char errMsg[128];
				snprintf(errMsg, sizeof(errMsg), "CreateProcessA failed (error %lu)", GetLastError());
				return MakeErrorResponse(errMsg);
			}

			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);

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

REGISTER_EDITOR_PLUGIN(PipelineEditorPlugin, "DiaPipelineEditor")
