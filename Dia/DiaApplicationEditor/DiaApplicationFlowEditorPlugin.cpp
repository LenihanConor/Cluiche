#include "DiaApplicationEditor/DiaApplicationFlowEditorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaApplicationEditor/V2/ManifestLoader.h>
#include <DiaApplicationEditor/V2/ManifestSaver.h>
#include <DiaApplicationEditor/V2/ManifestValidator.h>
#include <DiaApplicationEditor/V2/Commands/PUCommands.h>
#include <DiaApplicationEditor/V2/Commands/ModuleCommands.h>
#include <DiaApplicationEditor/V2/Commands/StageCommands.h>
#include <DiaApplicationEditor/V2/Commands/StreamCommands.h>
#include <DiaApplicationFlow/Streams/OverflowPolicy.h>
#include <string>
#include <windows.h>

namespace Dia { namespace Editor {

    static const char* OverflowPolicyToString(Dia::ApplicationFlow::OverflowPolicy policy)
    {
        using OP = Dia::ApplicationFlow::OverflowPolicy;
        switch (policy)
        {
        case OP::kDropNewest: return "drop-newest";
        case OP::kBlock:      return "block";
        case OP::kFailLoud:   return "fail-loud";
        default:              return "drop-oldest";
        }
    }

    static Json::Value BuildManifestStateJson(
        const Dia::ApplicationFlow::Editor::ManifestEditorState& state)
    {
        Json::Value root;
        root["filePath"]    = state.filePath;
        root["isDirty"]     = state.isDirty;
        root["hasManifest"] = state.hasManifest;

        if (!state.hasManifest)
            return root;

        const auto& m = state.manifest;
        Json::Value manifest;
        manifest["version"] = m.version;

        Json::Value stages(Json::arrayValue);
        for (unsigned int i = 0; i < m.stages.Size(); ++i)
        {
            Json::Value s;
            s["name"]         = m.stages[i].name.AsChar();
            s["manifestPath"] = m.stages[i].manifestPath.AsCStr();
            stages.append(s);
        }
        manifest["stages"] = stages;

        manifest["initialStage"] = m.initialStage.AsChar();

        Json::Value autoStages(Json::arrayValue);
        for (unsigned int i = 0; i < m.autoStages.Size(); ++i)
            autoStages.append(m.autoStages[i].AsChar());
        manifest["autoStages"] = autoStages;

        Json::Value streams(Json::arrayValue);
        for (unsigned int i = 0; i < m.streams.Size(); ++i)
        {
            const auto& sd = m.streams[i];
            Json::Value s;
            s["id"]             = sd.id.AsChar();
            s["kind"]           = sd.kind.AsChar();
            s["payloadType"]    = sd.payloadType.AsChar();
            s["fromPU"]         = sd.fromPU.AsChar();
            s["toPU"]           = sd.toPU.AsChar();
            s["capacity"]       = sd.capacity;
            s["maxReaders"]     = sd.maxReaders;
            s["multiWriter"]    = sd.multiWriter;
            s["overflow"]       = OverflowPolicyToString(sd.overflowPolicy);
            s["blockTimeoutMs"] = sd.blockTimeoutMs;
            streams.append(s);
        }
        manifest["streams"] = streams;

        Json::Value pus(Json::arrayValue);
        for (unsigned int i = 0; i < m.processingUnits.Size(); ++i)
        {
            const auto& pu = m.processingUnits[i];
            Json::Value p;
            p["instanceId"]      = pu.instanceId.AsChar();
            p["frequencyHz"]     = pu.frequencyHz;
            p["dedicatedThread"] = pu.dedicatedThread;

            Json::Value modules(Json::arrayValue);
            for (unsigned int j = 0; j < pu.modules.Size(); ++j)
            {
                const auto& mod = pu.modules[j];
                Json::Value mod_j;
                mod_j["instanceId"]      = mod.instanceId.AsChar();
                mod_j["typeId"]          = mod.typeId.AsChar();
                mod_j["startTimeoutMs"]  = mod.startTimeoutMs;
                mod_j["stopTimeoutMs"]   = mod.stopTimeoutMs;

                Json::Value modStages(Json::arrayValue);
                for (unsigned int k = 0; k < mod.stages.Size(); ++k)
                    modStages.append(mod.stages[k].AsChar());
                mod_j["stages"] = modStages;

                Json::Value deps(Json::arrayValue);
                for (unsigned int k = 0; k < mod.dependencies.Size(); ++k)
                    deps.append(mod.dependencies[k].AsChar());
                mod_j["dependencies"] = deps;

                Json::Value reads(Json::arrayValue);
                for (unsigned int k = 0; k < mod.reads.Size(); ++k)
                    reads.append(mod.reads[k].AsChar());
                mod_j["reads"] = reads;

                Json::Value writes(Json::arrayValue);
                for (unsigned int k = 0; k < mod.writes.Size(); ++k)
                    writes.append(mod.writes[k].AsChar());
                mod_j["writes"] = writes;

                modules.append(mod_j);
            }
            p["modules"] = modules;
            pus.append(p);
        }
        manifest["processingUnits"] = pus;

        root["manifest"] = manifest;
        return root;
    }

    static const Dia::Core::StringCRC kReqManifestLoad("manifest.load");
    static const Dia::Core::StringCRC kReqManifestSave("manifest.save");
    static const Dia::Core::StringCRC kReqManifestGetState("manifest.getState");
    static const Dia::Core::StringCRC kReqManifestApplyCommand("manifest.applyCommand");
    static const Dia::Core::StringCRC kReqHistoryUndo("history.undo");
    static const Dia::Core::StringCRC kReqHistoryRedo("history.redo");
    static const Dia::Core::StringCRC kReqHistoryGetState("history.getState");
    static const Dia::Core::StringCRC kReqValidationRun("validation.run");
    static const Dia::Core::StringCRC kReqTypesGet("types.get");
    static const Dia::Core::StringCRC kReqTypesRefresh("types.refresh");
    static const Dia::Core::StringCRC kReqRiskCheck("risk.check");
    static const Dia::Core::StringCRC kReqRiskConfirm("risk.confirm");
    static const Dia::Core::StringCRC kReqLiveConnect("live.connect");
    static const Dia::Core::StringCRC kReqLiveDisconnect("live.disconnect");
    static const Dia::Core::StringCRC kReqLiveGetStatus("live.getStatus");
    static const Dia::Core::StringCRC kReqLiveTransitionTo("live.transitionTo");
    static const Dia::Core::StringCRC kReqLiveShutdown("live.shutdown");

    static const Dia::Core::StringCRC kTopicAppState("app.state");
    static const Dia::Core::StringCRC kTopicAppModules("app.modules");
    static const Dia::Core::StringCRC kTopicAppStreams("app.streams");

    EditorToolbarItem DiaApplicationFlowEditorPlugin::GetToolbarItem() const
    {
        EditorToolbarItem item;
        strncpy_s(item.label, sizeof(item.label), "App Flow", _TRUNCATE);
        item.iconChar[0] = 'A';
        item.iconChar[1] = '\0';
        item.pinned = true;
        return item;
    }

    void DiaApplicationFlowEditorPlugin::OnLoad(const EditorPluginContext& context)
    {
        DIA_TRACE_ZONE("AppFlowEditorOnLoad", Dia::Observation::Trace::Category::kDiaApplicationFlow);

        mBridge = context.mBridge;
        mGameConnection = context.mServices->GetService<GameConnectionManager>();
        mModel = context.mModel;

        DIA_LOG_INFO("Editor",
            "DiaApplicationFlowEditorPlugin::OnLoad: bridge=%p gameConnection=%p model=%p",
            mBridge, mGameConnection, mModel);

        if (mBridge != nullptr)
        {
            mBridge->RegisterRequestHandler(kReqManifestLoad,    [this](const Json::Value& d) { return HandleManifestLoad(d); });
            mBridge->RegisterRequestHandler(kReqManifestSave,    [this](const Json::Value& d) { return HandleManifestSave(d); });
            mBridge->RegisterRequestHandler(kReqManifestGetState,[this](const Json::Value& d) { return HandleManifestGetState(d); });
            mBridge->RegisterRequestHandler(kReqManifestApplyCommand,[this](const Json::Value& d) { return HandleManifestApplyCommand(d); });
            mBridge->RegisterRequestHandler(kReqHistoryUndo,     [this](const Json::Value& d) { return HandleHistoryUndo(d); });
            mBridge->RegisterRequestHandler(kReqHistoryRedo,     [this](const Json::Value& d) { return HandleHistoryRedo(d); });
            mBridge->RegisterRequestHandler(kReqHistoryGetState, [this](const Json::Value& d) { return HandleHistoryGetState(d); });
            mBridge->RegisterRequestHandler(kReqValidationRun,   [this](const Json::Value& d) { return HandleValidationRun(d); });
            mBridge->RegisterRequestHandler(kReqTypesGet,        [this](const Json::Value& d) { return HandleTypesGet(d); });
            mBridge->RegisterRequestHandler(kReqTypesRefresh,    [this](const Json::Value& d) { return HandleTypesRefresh(d); });
            mBridge->RegisterRequestHandler(kReqRiskCheck,       [this](const Json::Value& d) { return HandleRiskCheck(d); });
            mBridge->RegisterRequestHandler(kReqRiskConfirm,     [this](const Json::Value& d) { return HandleRiskConfirm(d); });
            mBridge->RegisterRequestHandler(kReqLiveConnect,     [this](const Json::Value& d) { return HandleLiveConnect(d); });
            mBridge->RegisterRequestHandler(kReqLiveDisconnect,  [this](const Json::Value& d) { return HandleLiveDisconnect(d); });
            mBridge->RegisterRequestHandler(kReqLiveGetStatus,   [this](const Json::Value& d) { return HandleLiveGetStatus(d); });
            mBridge->RegisterRequestHandler(kReqLiveTransitionTo,[this](const Json::Value& d) { return HandleLiveTransitionTo(d); });
            mBridge->RegisterRequestHandler(kReqLiveShutdown,    [this](const Json::Value& d) { return HandleLiveShutdown(d); });
        }

        mFileWatcher.Start();

        // Subscribe to project changes so the manifest auto-loads when the diagame project changes
        if (mModel != nullptr)
        {
            mModel->OnDiagameProjectChanged(
                [](const Dia::Editor::ProjectContext& proj, void* ud)
                {
                    auto* self = static_cast<DiaApplicationFlowEditorPlugin*>(ud);
                    DIA_LOG_INFO("Editor", "AppFlowEditor: OnDiagameProjectChanged fired, applicationManifestPath='%s'",
                        proj.applicationManifestPath[0] ? proj.applicationManifestPath : "<empty>");
                    if (proj.applicationManifestPath[0] != '\0')
                    {
                        Json::Value req;
                        req["path"] = proj.applicationManifestPath;
                        // Store path for deferred push — React may not be mounted yet
                        self->mPendingManifestPath = Dia::Core::Containers::String512(proj.applicationManifestPath);
                        self->HandleManifestLoad(req);
                    }
                }, this);

            // Auto-load if a project is already open when plugin loads
            const auto& proj = mModel->GetDiagameProject();
            DIA_LOG_INFO("Editor", "AppFlowEditor: OnLoad project check, diagamePath='%s' applicationManifestPath='%s'",
                proj.diagamePath[0] ? proj.diagamePath : "<empty>",
                proj.applicationManifestPath[0] ? proj.applicationManifestPath : "<empty>");
            if (proj.applicationManifestPath[0] != '\0')
            {
                mPendingManifestPath = Dia::Core::Containers::String512(proj.applicationManifestPath);
                Json::Value req;
                req["path"] = proj.applicationManifestPath;
                HandleManifestLoad(req);
            }
        }
        else
        {
            DIA_LOG_WARNING("Editor", "AppFlowEditor: OnLoad — mModel is null, cannot subscribe to project changes");
        }

        // Register metrics
        {
            auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
            mMetricLoadMs             = reg.RegisterGauge(Dia::Core::StringCRC("dia.editor.manifest.load_ms"));
            mMetricSaveMs             = reg.RegisterGauge(Dia::Core::StringCRC("dia.editor.manifest.save_ms"));
            mMetricValidationErrors   = reg.RegisterGauge(Dia::Core::StringCRC("dia.editor.validation.error_count"));
            mMetricValidationWarnings = reg.RegisterGauge(Dia::Core::StringCRC("dia.editor.validation.warning_count"));
            mMetricConnectionState    = reg.RegisterGauge(Dia::Core::StringCRC("dia.editor.live.connection_state"));
            mMetricCommandsTotal      = reg.RegisterCounter(Dia::Core::StringCRC("dia.editor.commands.total"));
        }

        DIA_LOG_INFO("Editor", "DiaApplicationFlowEditorPlugin: loaded");
    }

    void DiaApplicationFlowEditorPlugin::OnUnload()
    {
        mFileWatcher.Stop();

        if (mBridge != nullptr)
        {
            mBridge->UnregisterRequestHandler(kReqManifestLoad);
            mBridge->UnregisterRequestHandler(kReqManifestSave);
            mBridge->UnregisterRequestHandler(kReqManifestGetState);
            mBridge->UnregisterRequestHandler(kReqManifestApplyCommand);
            mBridge->UnregisterRequestHandler(kReqHistoryUndo);
            mBridge->UnregisterRequestHandler(kReqHistoryRedo);
            mBridge->UnregisterRequestHandler(kReqHistoryGetState);
            mBridge->UnregisterRequestHandler(kReqValidationRun);
            mBridge->UnregisterRequestHandler(kReqTypesGet);
            mBridge->UnregisterRequestHandler(kReqTypesRefresh);
            mBridge->UnregisterRequestHandler(kReqRiskCheck);
            mBridge->UnregisterRequestHandler(kReqRiskConfirm);
            mBridge->UnregisterRequestHandler(kReqLiveConnect);
            mBridge->UnregisterRequestHandler(kReqLiveDisconnect);
            mBridge->UnregisterRequestHandler(kReqLiveGetStatus);
            mBridge->UnregisterRequestHandler(kReqLiveTransitionTo);
            mBridge->UnregisterRequestHandler(kReqLiveShutdown);
            mBridge = nullptr;
        }

        if (mGameConnection != nullptr && mGameConnection->IsConnected())
        {
            mGameConnection->Disconnect();
        }

        mGameConnection = nullptr;

        DIA_LOG_INFO("Editor", "DiaApplicationFlowEditorPlugin: unloaded");
    }

    void DiaApplicationFlowEditorPlugin::OnUpdate(float deltaTime)
    {
        mFileWatcher.Update();

        if (mGameConnection != nullptr)
        {
            mGameConnection->Update(deltaTime);
        }
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleManifestLoad(const Json::Value& data)
    {
        DIA_TRACE_ZONE("ManifestLoad", Dia::Observation::Trace::Category::kDiaApplicationFlow);
        Json::Value result;

        if (!data.isMember("path") || data["path"].asString().empty())
        {
            DIA_LOG_ERROR("Editor", "AppFlowEditor: HandleManifestLoad — no path in request");
            result["ok"]    = false;
            result["error"] = "path required";
            return result;
        }

        const std::string path = data["path"].asString();
        const char* pathCStr   = path.c_str();
        DIA_LOG_INFO("Editor", "AppFlowEditor: HandleManifestLoad loading '%s'", pathCStr);

        LARGE_INTEGER loadFreq, loadStart, loadEnd;
        ::QueryPerformanceFrequency(&loadFreq);
        ::QueryPerformanceCounter(&loadStart);

        auto lr = Dia::ApplicationFlow::Editor::ManifestLoader::Load(pathCStr, mEditorState);

        ::QueryPerformanceCounter(&loadEnd);
        const double loadMs = static_cast<double>(loadEnd.QuadPart - loadStart.QuadPart)
                              * 1000.0 / static_cast<double>(loadFreq.QuadPart);
        if (mMetricLoadMs) mMetricLoadMs->Set(loadMs);

        if (lr.status != Dia::ApplicationFlow::Editor::LoadStatus::Ok)
        {
            result["ok"]    = false;
            result["error"] = lr.errorMessage;
            DIA_LOG_ERROR("Editor", "AppFlowEditor: Manifest load FAILED path='%s' status=%d error='%s'",
                pathCStr, (int)lr.status, lr.errorMessage);
            return result;
        }

        DIA_LOG_INFO("Editor", "Manifest loaded: %s", pathCStr);

        mFileWatcher.ClearAll();
        mFileWatcher.Watch(pathCStr, [this](const char* changedPath, Dia::Core::FileWatchEvent event)
        {
            if (mSuppressFileWatchDuringSave)
                return;
            if (event == Dia::Core::FileWatchEvent::Modified)
            {
                Json::Value notification;
                notification["path"] = changedPath;
                mBridge->NotifyUIDataChanged("manifest.externalChange", notification);
            }
        });

        mCommandHistory.Clear();
        mCommandHistory.SetSavePoint();

        mBridge->NotifyUIDataChanged("manifest.state", BuildManifestStateJson(mEditorState));

        result["ok"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleManifestSave(const Json::Value& /*data*/)
    {
        DIA_TRACE_ZONE("ManifestSave", Dia::Observation::Trace::Category::kDiaApplicationFlow);
        Json::Value result;

        if (!mEditorState.hasManifest)
        {
            result["ok"]    = false;
            result["error"] = "no manifest loaded";
            return result;
        }

        auto vr = Dia::ApplicationFlow::Editor::ManifestValidator::Validate(mEditorState);
        if (vr.HasErrors())
        {
            result["ok"]         = false;
            result["error"]      = "validation errors";
            result["errorCount"] = vr.ErrorCount();
            DIA_LOG_ERROR("Editor", "Save blocked: %d validation errors", vr.ErrorCount());
            return result;
        }

        LARGE_INTEGER saveFreq, saveStart, saveEnd;
        ::QueryPerformanceFrequency(&saveFreq);
        ::QueryPerformanceCounter(&saveStart);

        mSuppressFileWatchDuringSave = true;
        auto sr = Dia::ApplicationFlow::Editor::ManifestSaver::Save(mEditorState);
        mSuppressFileWatchDuringSave = false;

        ::QueryPerformanceCounter(&saveEnd);
        const double saveMs = static_cast<double>(saveEnd.QuadPart - saveStart.QuadPart)
                              * 1000.0 / static_cast<double>(saveFreq.QuadPart);
        if (mMetricSaveMs) mMetricSaveMs->Set(saveMs);

        if (sr.status != Dia::ApplicationFlow::Editor::SaveStatus::Ok)
        {
            result["ok"]    = false;
            result["error"] = sr.errorMessage;
            return result;
        }

        DIA_LOG_INFO("Editor", "Manifest saved: %s", mEditorState.filePath);

        mCommandHistory.SetSavePoint();
        mBridge->NotifyUIDataChanged("manifest.dirty", Json::Value(false));

        result["ok"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleManifestGetState(const Json::Value& /*data*/)
    {
        Json::Value result;

        DIA_LOG_INFO("Editor", "AppFlowEditor: manifest.getState called, hasManifest=%d pendingPath='%s'",
            (int)mEditorState.hasManifest,
            mPendingManifestPath.IsEmpty() ? "<none>" : mPendingManifestPath.AsCStr());

        // If we have a pending path but the manifest didn't load yet (e.g. file wasn't ready),
        // try loading it now.
        if (!mEditorState.hasManifest && !mPendingManifestPath.IsEmpty())
        {
            DIA_LOG_INFO("Editor", "AppFlowEditor: retrying deferred load of '%s'", mPendingManifestPath.AsCStr());
            Json::Value req;
            req["path"] = mPendingManifestPath.AsCStr();
            HandleManifestLoad(req);
        }

        if (!mEditorState.hasManifest)
        {
            result["ok"]    = false;
            result["error"] = "no manifest loaded";
            DIA_LOG_INFO("Editor", "AppFlowEditor: manifest.getState — still no manifest after retry");
            return result;
        }

        // Push via event AND return in response so React gets it either way
        const Json::Value stateJson = BuildManifestStateJson(mEditorState);
        if (mBridge) mBridge->NotifyUIDataChanged("manifest.state", stateJson);

        result["ok"]    = true;
        result["state"] = stateJson;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleManifestApplyCommand(const Json::Value& data)
    {
        DIA_TRACE_ZONE("ManifestApplyCommand", Dia::Observation::Trace::Category::kDiaApplicationFlow);
        using namespace Dia::ApplicationFlow;
        using namespace Dia::ApplicationFlow::Editor;
        Json::Value result;

        if (!mEditorState.hasManifest)
        {
            result["ok"]    = false;
            result["error"] = "no manifest loaded";
            return result;
        }

        if (!data.isMember("commandType") || !data["commandType"].isString())
        {
            result["ok"]    = false;
            result["error"] = "commandType required";
            return result;
        }

        const std::string cmdType = data["commandType"].asString();
        ICommand* cmd = nullptr;

        // ----- Processing Unit commands -----
        if (cmdType == "AddPU")
        {
            const char* idStr = data.get("instanceId", "").asCString();
            const float freq  = data.get("frequencyHz", 30.0f).asFloat();
            const bool thread = data.get("dedicatedThread", false).asBool();
            cmd = new AddPUCommand(Dia::Core::StringCRC(idStr), freq, thread);
        }
        else if (cmdType == "RemovePU")
        {
            const char* idStr = data.get("instanceId", "").asCString();
            cmd = new RemovePUCommand(Dia::Core::StringCRC(idStr));
        }
        else if (cmdType == "SetPUFrequency")
        {
            const char* idStr = data.get("instanceId", "").asCString();
            const float freq  = data.get("frequencyHz", 30.0f).asFloat();
            cmd = new SetPUFrequencyCommand(Dia::Core::StringCRC(idStr), freq);
        }
        else if (cmdType == "SetPUThread")
        {
            const char* idStr = data.get("instanceId", "").asCString();
            const bool thread = data.get("dedicatedThread", false).asBool();
            cmd = new SetPUThreadCommand(Dia::Core::StringCRC(idStr), thread);
        }
        else if (cmdType == "ReorderPU")
        {
            const char* idStr = data.get("instanceId", "").asCString();
            const int newIdx  = data.get("newIndex", 0).asInt();
            cmd = new ReorderPUCommand(Dia::Core::StringCRC(idStr), newIdx);
        }
        // ----- Module commands -----
        else if (cmdType == "AddModule")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* instStr = data.get("instanceId", "").asCString();
            const char* typeStr = data.get("typeId", "").asCString();
            cmd = new AddModuleCommand(Dia::Core::StringCRC(puId),
                                       Dia::Core::StringCRC(instStr),
                                       Dia::Core::StringCRC(typeStr));
        }
        else if (cmdType == "RemoveModule")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* instStr = data.get("instanceId", "").asCString();
            cmd = new RemoveModuleCommand(Dia::Core::StringCRC(puId),
                                          Dia::Core::StringCRC(instStr));
        }
        else if (cmdType == "AddModuleDep")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const char* depStr  = data.get("dependency", "").asCString();
            cmd = new AddModuleDepCommand(Dia::Core::StringCRC(puId),
                                          Dia::Core::StringCRC(modStr),
                                          Dia::Core::StringCRC(depStr));
        }
        else if (cmdType == "RemoveModuleDep")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const char* depStr  = data.get("dependency", "").asCString();
            cmd = new RemoveModuleDepCommand(Dia::Core::StringCRC(puId),
                                             Dia::Core::StringCRC(modStr),
                                             Dia::Core::StringCRC(depStr));
        }
        else if (cmdType == "SetModuleStages")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const Json::Value& stagesArr = data["stages"];
            const unsigned int count = stagesArr.isArray() ? stagesArr.size() : 0u;
            Dia::Core::StringCRC stageIds[64];
            const unsigned int clamped = (count > 64u) ? 64u : count;
            for (unsigned int i = 0; i < clamped; ++i)
            {
                if (stagesArr[i].isString())
                    stageIds[i] = Dia::Core::StringCRC(stagesArr[i].asCString());
            }
            cmd = new SetModuleStagesCommand(Dia::Core::StringCRC(puId),
                                             Dia::Core::StringCRC(modStr),
                                             stageIds, clamped);
        }
        else if (cmdType == "SetModuleStartTimeout")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const float v       = data.get("startTimeoutMs", 0.0f).asFloat();
            cmd = new SetModuleStartTimeoutCommand(Dia::Core::StringCRC(puId),
                                                   Dia::Core::StringCRC(modStr), v);
        }
        else if (cmdType == "SetModuleStopTimeout")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const float v       = data.get("stopTimeoutMs", 0.0f).asFloat();
            cmd = new SetModuleStopTimeoutCommand(Dia::Core::StringCRC(puId),
                                                  Dia::Core::StringCRC(modStr), v);
        }
        else if (cmdType == "AddModuleRead")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const char* sStr    = data.get("streamId", "").asCString();
            cmd = new AddModuleReadCommand(Dia::Core::StringCRC(puId),
                                           Dia::Core::StringCRC(modStr),
                                           Dia::Core::StringCRC(sStr));
        }
        else if (cmdType == "RemoveModuleRead")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const char* sStr    = data.get("streamId", "").asCString();
            cmd = new RemoveModuleReadCommand(Dia::Core::StringCRC(puId),
                                              Dia::Core::StringCRC(modStr),
                                              Dia::Core::StringCRC(sStr));
        }
        else if (cmdType == "AddModuleWrite")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const char* sStr    = data.get("streamId", "").asCString();
            cmd = new AddModuleWriteCommand(Dia::Core::StringCRC(puId),
                                            Dia::Core::StringCRC(modStr),
                                            Dia::Core::StringCRC(sStr));
        }
        else if (cmdType == "RemoveModuleWrite")
        {
            const char* puId    = data.get("puId", "").asCString();
            const char* modStr  = data.get("instanceId", "").asCString();
            const char* sStr    = data.get("streamId", "").asCString();
            cmd = new RemoveModuleWriteCommand(Dia::Core::StringCRC(puId),
                                               Dia::Core::StringCRC(modStr),
                                               Dia::Core::StringCRC(sStr));
        }
        // ----- Stage commands -----
        else if (cmdType == "AddStage")
        {
            const char* nameStr = data.get("name", "").asCString();
            const char* mp      = data.get("manifestPath", "").asCString();
            cmd = new AddStageCommand(Dia::Core::StringCRC(nameStr), mp);
        }
        else if (cmdType == "RemoveStage")
        {
            const char* nameStr = data.get("name", "").asCString();
            cmd = new RemoveStageCommand(Dia::Core::StringCRC(nameStr));
        }
        else if (cmdType == "RenameStage")
        {
            const char* oldStr  = data.get("oldName", "").asCString();
            const char* newStr  = data.get("newName", "").asCString();
            cmd = new RenameStageCommand(Dia::Core::StringCRC(oldStr),
                                         Dia::Core::StringCRC(newStr));
        }
        else if (cmdType == "SetStageTrigger")
        {
            const char* nameStr = data.get("name", "").asCString();
            const bool isAuto   = data.get("isAuto", false).asBool();
            cmd = new SetStageTriggerCommand(Dia::Core::StringCRC(nameStr), isAuto);
        }
        else if (cmdType == "SetInitialStage")
        {
            const char* nameStr = data.get("name", "").asCString();
            cmd = new SetInitialStageCommand(Dia::Core::StringCRC(nameStr));
        }
        else if (cmdType == "ReorderStage")
        {
            const char* nameStr = data.get("name", "").asCString();
            const int newIdx    = data.get("newIndex", 0).asInt();
            cmd = new ReorderStageCommand(Dia::Core::StringCRC(nameStr), newIdx);
        }
        // ----- Stream commands -----
        else if (cmdType == "AddStream")
        {
            const char* idStr   = data.get("id", "").asCString();
            const char* kindStr = data.get("kind", "").asCString();
            const char* plStr   = data.get("payloadType", "").asCString();
            cmd = new AddStreamCommand(idStr,
                                       Dia::Core::StringCRC(kindStr),
                                       Dia::Core::StringCRC(plStr));
        }
        else if (cmdType == "RemoveStream")
        {
            const char* idStr = data.get("streamId", data.get("id", "").asCString()).asCString();
            cmd = new RemoveStreamCommand(Dia::Core::StringCRC(idStr));
        }
        else if (cmdType == "SetStreamKind")
        {
            const char* idStr = data.get("streamId", "").asCString();
            const char* vStr  = data.get("value", "").asCString();
            cmd = new SetStreamKindCommand(Dia::Core::StringCRC(idStr),
                                           Dia::Core::StringCRC(vStr));
        }
        else if (cmdType == "SetStreamPayload" || cmdType == "SetStreamPayloadType")
        {
            const char* idStr = data.get("streamId", "").asCString();
            const char* vStr  = data.get("value", "").asCString();
            cmd = new SetStreamPayloadCommand(Dia::Core::StringCRC(idStr),
                                              Dia::Core::StringCRC(vStr));
        }
        else if (cmdType == "SetStreamFromPU")
        {
            const char* idStr = data.get("streamId", "").asCString();
            const char* vStr  = data.get("value", "").asCString();
            cmd = new SetStreamFromPUCommand(Dia::Core::StringCRC(idStr),
                                             Dia::Core::StringCRC(vStr));
        }
        else if (cmdType == "SetStreamToPU")
        {
            const char* idStr = data.get("streamId", "").asCString();
            const char* vStr  = data.get("value", "").asCString();
            cmd = new SetStreamToPUCommand(Dia::Core::StringCRC(idStr),
                                           Dia::Core::StringCRC(vStr));
        }
        else if (cmdType == "SetStreamCapacity")
        {
            const char* idStr = data.get("streamId", "").asCString();
            const unsigned int v = data.get("value", 0u).asUInt();
            cmd = new SetStreamCapacityCommand(Dia::Core::StringCRC(idStr), v);
        }
        else if (cmdType == "SetStreamMaxReaders")
        {
            const char* idStr = data.get("streamId", "").asCString();
            const unsigned int v = data.get("value", 0u).asUInt();
            cmd = new SetStreamMaxReadersCommand(Dia::Core::StringCRC(idStr), v);
        }
        else if (cmdType == "SetStreamOverflow")
        {
            const char* idStr = data.get("streamId", "").asCString();
            const char* vStr  = data.get("value", "drop-oldest").asCString();
            const OverflowPolicy p = ParseOverflowPolicy(Dia::Core::StringCRC(vStr));
            cmd = new SetStreamOverflowCommand(Dia::Core::StringCRC(idStr), p);
        }
        else if (cmdType == "SetStreamMultiWriter")
        {
            const char* idStr = data.get("streamId", "").asCString();
            const bool v      = data.get("value", false).asBool();
            cmd = new SetStreamMultiWriterCommand(Dia::Core::StringCRC(idStr), v);
        }
        else
        {
            DIA_LOG_WARNING("Editor", "AppFlowEditor: unknown commandType '%s'", cmdType.c_str());
            result["ok"]    = false;
            result["error"] = "unknown commandType";
            return result;
        }

        if (cmd == nullptr)
        {
            result["ok"]    = false;
            result["error"] = "command construction failed";
            return result;
        }

        DIA_LOG_INFO("Editor", "AppFlowEditor: applyCommand %s", cmdType.c_str());
        if (mMetricCommandsTotal) mMetricCommandsTotal->Inc();
        mCommandHistory.Execute(cmd, mEditorState);
        mEditorState.isDirty = !mCommandHistory.IsAtSavePoint();

        if (mBridge)
            mBridge->NotifyUIDataChanged("manifest.state", BuildManifestStateJson(mEditorState));

        result["ok"]      = true;
        result["canUndo"] = mCommandHistory.CanUndo();
        result["canRedo"] = mCommandHistory.CanRedo();
        result["isDirty"] = mEditorState.isDirty;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleHistoryUndo(const Json::Value& /*data*/)
    {
        Json::Value result;

        if (!mCommandHistory.CanUndo())
        {
            result["ok"]      = false;
            result["canUndo"] = false;
            return result;
        }

        DIA_LOG_INFO("Editor", "Undo/Redo command");
        if (mMetricCommandsTotal) mMetricCommandsTotal->Inc();
        mCommandHistory.Undo(mEditorState);
        mEditorState.isDirty = !mCommandHistory.IsAtSavePoint();

        mBridge->NotifyUIDataChanged("manifest.state", BuildManifestStateJson(mEditorState));

        result["ok"]      = true;
        result["canUndo"] = mCommandHistory.CanUndo();
        result["canRedo"] = mCommandHistory.CanRedo();
        result["isDirty"] = !mCommandHistory.IsAtSavePoint();
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleHistoryRedo(const Json::Value& /*data*/)
    {
        Json::Value result;

        if (!mCommandHistory.CanRedo())
        {
            result["ok"]      = false;
            result["canRedo"] = false;
            return result;
        }

        DIA_LOG_INFO("Editor", "Undo/Redo command");
        if (mMetricCommandsTotal) mMetricCommandsTotal->Inc();
        mCommandHistory.Redo(mEditorState);
        mEditorState.isDirty = !mCommandHistory.IsAtSavePoint();

        mBridge->NotifyUIDataChanged("manifest.state", BuildManifestStateJson(mEditorState));

        result["ok"]      = true;
        result["canUndo"] = mCommandHistory.CanUndo();
        result["canRedo"] = mCommandHistory.CanRedo();
        result["isDirty"] = !mCommandHistory.IsAtSavePoint();
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleHistoryGetState(const Json::Value& /*data*/)
    {
        Json::Value result;
        result["ok"]      = true;
        result["canUndo"] = mCommandHistory.CanUndo();
        result["canRedo"] = mCommandHistory.CanRedo();
        result["count"]   = mCommandHistory.GetCount();
        result["isDirty"] = !mCommandHistory.IsAtSavePoint();
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleValidationRun(const Json::Value& /*data*/)
    {
        DIA_TRACE_ZONE("ManifestValidate", Dia::Observation::Trace::Category::kDiaApplicationFlow);
        Json::Value result;

        if (!mEditorState.hasManifest)
        {
            result["ok"]    = false;
            result["error"] = "no manifest loaded";
            return result;
        }

        auto vr = Dia::ApplicationFlow::Editor::ManifestValidator::Validate(mEditorState);

        if (mMetricValidationErrors)   mMetricValidationErrors->Set(static_cast<double>(vr.ErrorCount()));
        if (mMetricValidationWarnings) mMetricValidationWarnings->Set(static_cast<double>(vr.WarningCount()));

        DIA_LOG_INFO("Editor", "Validation: %d errors, %d warnings", vr.ErrorCount(), vr.WarningCount());

        result["ok"]           = true;
        result["errorCount"]   = vr.ErrorCount();
        result["warningCount"] = vr.WarningCount();

        Json::Value issues(Json::arrayValue);
        for (unsigned int i = 0; i < vr.issues.Size(); ++i)
        {
            const auto& issue = vr.issues[i];
            Json::Value item;
            item["ruleId"]   = static_cast<int>(issue.ruleId);
            item["severity"] = (issue.severity == Dia::ApplicationFlow::Editor::ValidationSeverity::Error)
                               ? "error" : "warning";
            item["message"]  = issue.message;
            issues.append(item);
        }
        result["issues"] = issues;

        mBridge->NotifyUIDataChanged("validation.result", result);
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleTypesGet(const Json::Value& /*data*/)
    {
        Json::Value result;
        result["ok"] = true;

        Json::Value moduleTypes(Json::arrayValue);
        const auto* mt   = mTypeDiscovery.GetModuleTypes();
        unsigned int mtc = mTypeDiscovery.GetModuleTypeCount();
        for (unsigned int i = 0; i < mtc; ++i)
        {
            Json::Value t;
            t["id"]          = mt[i].typeId.AsChar();
            t["displayName"] = mt[i].description;
            moduleTypes.append(t);
        }
        result["moduleTypes"] = moduleTypes;

        Json::Value puTypes(Json::arrayValue);
        const auto* pt   = mTypeDiscovery.GetPUTypes();
        unsigned int ptc = mTypeDiscovery.GetPUTypeCount();
        for (unsigned int i = 0; i < ptc; ++i)
        {
            Json::Value t;
            t["id"]          = pt[i].typeId.AsChar();
            t["displayName"] = pt[i].description;
            puTypes.append(t);
        }
        result["puTypes"] = puTypes;

        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleTypesRefresh(const Json::Value& data)
    {
        std::string typesPath;

        if (data.isMember("path") && !data["path"].asString().empty())
        {
            typesPath = data["path"].asString();
        }
        else if (mEditorState.hasManifest && mEditorState.filePath[0] != '\0')
        {
            // Derive types.json alongside the loaded manifest
            typesPath = mEditorState.filePath;
            const size_t sep = typesPath.find_last_of("\\/");
            if (sep != std::string::npos)
                typesPath = typesPath.substr(0, sep + 1) + "types.json";
            else
                typesPath = "types.json";
        }

        if (!typesPath.empty())
            mTypeDiscovery.LoadFromFile(typesPath.c_str());

        return HandleTypesGet(data);
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleRiskCheck(const Json::Value& data)
    {
        Json::Value result;
        result["ok"] = true;

        if (mGameConnection == nullptr || !mGameConnection->IsConnected())
        {
            result["hasRisk"] = false;
            return result;
        }

        static const char* const kRiskyCommands[] = {
            "RemovePU", "RemoveModule",
            "SetStreamCapacity", "SetStreamMaxReaders",
            "SetPUFrequency", "RemoveStage"
        };
        static const char* const kRiskyDescriptions[] = {
            "Removing a processing unit while live may crash the runtime.",
            "Removing a module while live may crash the runtime.",
            "Changing stream capacity while live will cause a runtime reconfiguration.",
            "Changing stream max readers while live will cause a runtime reconfiguration.",
            "Changing PU frequency while live will affect timing of the running simulation.",
            "Removing a stage while live may leave the runtime in an inconsistent state."
        };
        static const unsigned int kRiskyCount =
            sizeof(kRiskyCommands) / sizeof(kRiskyCommands[0]);

        const std::string cmdType = data.isMember("commandType")
                                    ? data["commandType"].asString() : "";

        for (unsigned int i = 0; i < kRiskyCount; ++i)
        {
            if (cmdType == kRiskyCommands[i])
            {
                result["hasRisk"]     = true;
                result["condition"]   = kRiskyCommands[i];
                result["description"] = kRiskyDescriptions[i];
                return result;
            }
        }

        result["hasRisk"] = false;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleRiskConfirm(const Json::Value& /*data*/)
    {
        Json::Value result;
        result["ok"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleLiveConnect(const Json::Value& data)
    {
        Json::Value result;

        if (mGameConnection == nullptr)
        {
            result["ok"]    = false;
            result["error"] = "game connection not available";
            return result;
        }

        if (mGameConnection->IsConnected())
        {
            result["ok"]    = false;
            result["error"] = "already connected";
            return result;
        }

        const char* host = data.isMember("host") ? data["host"].asCString() : "localhost";
        const int port   = data.isMember("port") ? data["port"].asInt() : 7000;

        DIA_LOG_INFO("Editor", "Connecting to game at %s:%d", host, port);

        mGameConnection->SetConnectionCallback(
            [this, hostStr = std::string(host), port](bool connected)
            {
                Json::Value status;
                status["connected"] = connected;
                status["host"]      = hostStr;
                status["port"]      = port;
                if (mBridge)
                    mBridge->NotifyUIDataChanged("live.connectionStatus", status);

                if (connected)
                {
                    mIsLiveConnected = true;
                    if (mMetricConnectionState) mMetricConnectionState->Set(1.0);
                    DIA_LOG_INFO("Editor", "Game connection established");

                    mGameConnection->Subscribe(kTopicAppState, [this](const Json::Value& d)
                    {
                        Dia::ApplicationFlow::Editor::LiveAppState appState;
                        if (d.isMember("stage"))
                            appState.currentStage = Dia::Core::StringCRC(d["stage"].asCString());
                        appState.isTransitioning = d.isMember("transitioning") && d["transitioning"].asBool();
                        if (d.isMember("targetStage"))
                            appState.targetStage = Dia::Core::StringCRC(d["targetStage"].asCString());
                        mLiveStore.UpdateAppState(appState);
                        if (mBridge)
                            mBridge->NotifyUIDataChanged("live.state", d);
                    });

                    mGameConnection->Subscribe(kTopicAppModules, [this](const Json::Value& d)
                    {
                        if (mBridge)
                            mBridge->NotifyUIDataChanged("live.modules", d);
                    });

                    mGameConnection->Subscribe(kTopicAppStreams, [this](const Json::Value& d)
                    {
                        if (mBridge)
                            mBridge->NotifyUIDataChanged("live.streams", d);
                    });
                }
                else
                {
                    mIsLiveConnected = false;
                    if (mMetricConnectionState) mMetricConnectionState->Set(0.0);
                    DIA_LOG_INFO("Editor", "Game connection lost");

                    mGameConnection->Unsubscribe(kTopicAppState);
                    mGameConnection->Unsubscribe(kTopicAppModules);
                    mGameConnection->Unsubscribe(kTopicAppStreams);
                    mLiveStore.Clear();

                    if (mBridge)
                        mBridge->NotifyUIDataChanged("live.state", Json::Value());
                }
            });

        mGameConnection->Connect(host, port);

        result["ok"]         = true;
        result["connecting"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleLiveDisconnect(const Json::Value& /*data*/)
    {
        Json::Value result;

        if (mGameConnection == nullptr || !mGameConnection->IsConnected())
        {
            result["ok"]    = false;
            result["error"] = "not connected";
            return result;
        }

        DIA_LOG_INFO("Editor", "Disconnecting from game");
        mGameConnection->Disconnect();

        result["ok"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleLiveGetStatus(const Json::Value& /*data*/)
    {
        Json::Value result;
        result["ok"]        = true;
        result["connected"] = (mGameConnection != nullptr && mGameConnection->IsConnected());
        result["liveActive"] = mLiveStore.IsActive();
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleLiveTransitionTo(const Json::Value& data)
    {
        Json::Value result;

        if (mGameConnection == nullptr || !mGameConnection->IsConnected())
        {
            result["ok"]    = false;
            result["error"] = "not connected";
            return result;
        }

        if (!data.isMember("stageName"))
        {
            result["ok"]    = false;
            result["error"] = "stageName required";
            return result;
        }

        Json::Value args;
        args["stage"] = data["stageName"];

        mGameConnection->SendCommandWithResponse("transition_to", args,
            [this](bool success, const Json::Value& res)
            {
                Json::Value notification;
                notification["success"] = success;
                notification["result"]  = res;
                if (mBridge)
                    mBridge->NotifyUIDataChanged("live.transitionResult", notification);
            });

        result["ok"]      = true;
        result["pending"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowEditorPlugin::HandleLiveShutdown(const Json::Value& /*data*/)
    {
        Json::Value result;

        if (mGameConnection == nullptr || !mGameConnection->IsConnected())
        {
            result["ok"]    = false;
            result["error"] = "not connected";
            return result;
        }

        mGameConnection->SendCommand("shutdown", Json::Value());

        result["ok"] = true;
        return result;
    }

}} // namespace Dia::Editor

using namespace Dia::Editor;
REGISTER_EDITOR_PLUGIN(DiaApplicationFlowEditorPlugin, "DiaApplicationFlowEditor")
