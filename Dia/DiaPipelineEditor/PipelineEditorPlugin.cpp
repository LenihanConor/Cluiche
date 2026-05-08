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

using namespace Dia::PipelineEditor;

static const Dia::Core::StringCRC kCmdPipelineStart("pipeline.start");
static const Dia::Core::StringCRC kCmdPipelineCancel("pipeline.cancel");
static const Dia::Core::StringCRC kCmdPipelineGetTargets("pipeline.get-targets");
static const Dia::Core::StringCRC kCmdPipelineHistory("pipeline.history");

PipelineEditorPlugin::PipelineEditorPlugin()
	: mTailer(nullptr)
	, mBuildManager(nullptr)
	, mHistoryStore(nullptr)
	, mBridge(nullptr)
	, mLastPushedEventIndex(0)
	, mLastBuildRunning(false)
	, mLastExitCode(0)
{
	mRepoRoot[0] = '\0';
	mPluginOutputRoot[0] = '\0';
}

PipelineEditorPlugin::~PipelineEditorPlugin()
{
}

void PipelineEditorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
{
	mBridge = context.mBridge;

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

	snprintf(mPluginOutputRoot, sizeof(mPluginOutputRoot), "%s/Cluiche/out/CluicheEditor/DiaPipelineEditor", mRepoRoot);

	SYSTEMTIME st;
	GetLocalTime(&st);
	char sessionId[64];
	snprintf(sessionId, sizeof(sessionId), "s-%04d%02d%02d-%02d%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);

	mHistoryStore = new RunHistoryStore();
	mHistoryStore->Initialize(mPluginOutputRoot, sessionId);

	RegisterCommands();
}

void PipelineEditorPlugin::OnUnload()
{
	UnregisterCommands();

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
	if (mBridge && mBuildManager)
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
			mBridge->NotifyUIDataChanged("pipeline.build-status", status);
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
			if (mBridge)
			{
				Json::Value historyPayload;
				historyPayload["runs"] = mHistoryStore->ToJson();
				mBridge->NotifyUIDataChanged("pipeline.history", historyPayload);
			}
		}
	}

	PushEventsToUI();
}

void PipelineEditorPlugin::PushEventsToUI()
{
	if (mBridge == nullptr || mTailer == nullptr)
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

	mBridge->NotifyUIDataChanged("pipeline.event", payload);
}

void PipelineEditorPlugin::RegisterCommands()
{
	if (mBridge == nullptr)
		return;

	mBridge->RegisterRequestHandler(kCmdPipelineStart,
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

	mBridge->RegisterRequestHandler(kCmdPipelineCancel,
		[this](const Json::Value&) -> Json::Value
		{
			mBuildManager->Cancel();
			Json::Value result;
			result["ok"] = true;
			return result;
		});

	mBridge->RegisterRequestHandler(kCmdPipelineGetTargets,
		[this](const Json::Value&) -> Json::Value
		{
			char tomlPath[1024];
			snprintf(tomlPath, sizeof(tomlPath), "%s/pipeline.toml", mRepoRoot);

			Json::Value result;
			result["targets"] = Internal::ParsePipelineTargets(tomlPath);
			return result;
		});

	mBridge->RegisterRequestHandler(kCmdPipelineHistory,
		[this](const Json::Value&) -> Json::Value
		{
			Json::Value result;
			result["runs"] = mHistoryStore->ToJson();
			return result;
		});
}

void PipelineEditorPlugin::UnregisterCommands()
{
	if (mBridge == nullptr)
		return;

	mBridge->UnregisterRequestHandler(kCmdPipelineStart);
	mBridge->UnregisterRequestHandler(kCmdPipelineCancel);
	mBridge->UnregisterRequestHandler(kCmdPipelineGetTargets);
	mBridge->UnregisterRequestHandler(kCmdPipelineHistory);
}

REGISTER_EDITOR_PLUGIN(PipelineEditorPlugin, "DiaPipelineEditor")
