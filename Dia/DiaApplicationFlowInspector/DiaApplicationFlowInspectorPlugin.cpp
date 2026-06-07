#include "DiaApplicationFlowInspector/DiaApplicationFlowInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaCore/Json/external/json/json.h>
#include <string>

namespace Dia { namespace Editor {

    static const Dia::Core::StringCRC kReqLiveConnect("live.connect");
    static const Dia::Core::StringCRC kReqLiveDisconnect("live.disconnect");
    static const Dia::Core::StringCRC kReqLiveGetStatus("live.getStatus");
    static const Dia::Core::StringCRC kReqLiveTransitionTo("live.transitionTo");
    static const Dia::Core::StringCRC kReqLiveShutdown("live.shutdown");

    static const Dia::Core::StringCRC kTopicAppState("app.state");
    static const Dia::Core::StringCRC kTopicAppModules("app.modules");
    static const Dia::Core::StringCRC kTopicAppStreams("app.streams");
    static const Dia::Core::StringCRC kTopicAppTimings("app.timings");
    static const Dia::Core::StringCRC kTopicAppEvent("app.event");

    DiaApplicationFlowInspectorPlugin::DiaApplicationFlowInspectorPlugin()
        : EditorPluginBase({
            "Application Flow Inspector",
            "1.0",
            "Live runtime inspection panel for connected game instances",
            "dia://plugins/diaapplicationflowinspector/index.html",
            LayoutMode::kDockable,
            "",
            "I",
            false
        })
    {
    }

    void DiaApplicationFlowInspectorPlugin::OnPluginLoad()
    {
        mGameConnection = GetServices() ? GetServices()->GetService<GameConnectionManager>() : nullptr;

        auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricEventsTotal = reg.RegisterCounter(Dia::Core::StringCRC("inspector.events_total"));

        DIA_LOG_INFO("Inspector", "DiaApplicationFlowInspectorPlugin: loaded");

        RegisterHandler(kReqLiveConnect,      [this](const Json::Value& d) { return HandleLiveConnect(d); });
        RegisterHandler(kReqLiveDisconnect,   [this](const Json::Value& d) { return HandleLiveDisconnect(d); });
        RegisterHandler(kReqLiveGetStatus,    [this](const Json::Value& d) { return HandleLiveGetStatus(d); });
        RegisterHandler(kReqLiveTransitionTo, [this](const Json::Value& d) { return HandleLiveTransitionTo(d); });
        RegisterHandler(kReqLiveShutdown,     [this](const Json::Value& d) { return HandleLiveShutdown(d); });

        Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealthReporter);
    }

    void DiaApplicationFlowInspectorPlugin::OnPluginUnload()
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealthReporter);

        if (mGameConnection != nullptr && mGameConnection->IsConnected())
        {
            mGameConnection->Disconnect();
        }

        mGameConnection = nullptr;

        DIA_LOG_INFO("Inspector", "DiaApplicationFlowInspectorPlugin: unloaded");
    }

    void DiaApplicationFlowInspectorPlugin::OnUpdate(float deltaTime)
    {
        DIA_TRACE_ZONE("inspector.update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

        if (mIsLiveConnected)
        {
            mSecondsSinceLastData += deltaTime;
        }

        if (mGameConnection != nullptr)
        {
            mGameConnection->Update(deltaTime);
        }
    }

    Json::Value DiaApplicationFlowInspectorPlugin::HandleLiveConnect(const Json::Value& data)
    {
        DIA_TRACE_ZONE("inspector.connect", Dia::Observation::Trace::Category::kDiaApplicationFlow);
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

        DIA_LOG_INFO("Inspector", "inspector.connect host=%s port=%d", host, port);

        mGameConnection->SetConnectionCallback(
            [this, hostStr = std::string(host), port](bool connected)
            {
                Json::Value status;
                status["connected"] = connected;
                status["host"]      = hostStr;
                status["port"]      = port;
                if (GetBridge())
                    GetBridge()->NotifyUIDataChanged("live.connectionStatus", status);

                if (connected)
                {
                    mIsLiveConnected = true;
                    DIA_LOG_INFO("Inspector", "Game connection established");

                    mGameConnection->Subscribe(kTopicAppState, [this](const Json::Value& d)
                    {
                        mSecondsSinceLastData = 0.0f;
                        Dia::ApplicationFlow::Editor::LiveAppState appState;
                        if (d.isMember("stage"))
                            appState.currentStage = Dia::Core::StringCRC(d["stage"].asCString());
                        appState.isTransitioning = d.isMember("transitioning") && d["transitioning"].asBool();
                        if (d.isMember("targetStage"))
                            appState.targetStage = Dia::Core::StringCRC(d["targetStage"].asCString());
                        mLiveStore.UpdateAppState(appState);
                        if (GetBridge())
                            GetBridge()->NotifyUIDataChanged("live.state", d);
                        if (mMetricEventsTotal) mMetricEventsTotal->Inc();
                    });

                    mGameConnection->Subscribe(kTopicAppModules, [this](const Json::Value& d)
                    {
                        mSecondsSinceLastData = 0.0f;
                        if (GetBridge())
                            GetBridge()->NotifyUIDataChanged("live.modules", d);
                        if (mMetricEventsTotal) mMetricEventsTotal->Inc();
                    });

                    mGameConnection->Subscribe(kTopicAppStreams, [this](const Json::Value& d)
                    {
                        mSecondsSinceLastData = 0.0f;
                        if (GetBridge())
                            GetBridge()->NotifyUIDataChanged("live.streams", d);
                        if (mMetricEventsTotal) mMetricEventsTotal->Inc();
                    });

                    mGameConnection->Subscribe(kTopicAppTimings, [this](const Json::Value& d)
                    {
                        mSecondsSinceLastData = 0.0f;
                        if (GetBridge())
                            GetBridge()->NotifyUIDataChanged("live.timings", d);
                        if (mMetricEventsTotal) mMetricEventsTotal->Inc();
                    });

                    mGameConnection->Subscribe(kTopicAppEvent, [this](const Json::Value& d)
                    {
                        mSecondsSinceLastData = 0.0f;
                        if (GetBridge())
                            GetBridge()->NotifyUIDataChanged("live.event", d);
                        if (mMetricEventsTotal) mMetricEventsTotal->Inc();
                    });
                }
                else
                {
                    mIsLiveConnected = false;
                    mSecondsSinceLastData = 999.0f;
                    DIA_LOG_INFO("Inspector", "Game connection lost");

                    mGameConnection->Unsubscribe(kTopicAppState);
                    mGameConnection->Unsubscribe(kTopicAppModules);
                    mGameConnection->Unsubscribe(kTopicAppStreams);
                    mGameConnection->Unsubscribe(kTopicAppTimings);
                    mGameConnection->Unsubscribe(kTopicAppEvent);
                    mLiveStore.Clear();

                    if (GetBridge())
                        GetBridge()->NotifyUIDataChanged("live.state", Json::Value());
                }
            });

        mGameConnection->Connect(host, port);

        result["ok"]         = true;
        result["connecting"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowInspectorPlugin::HandleLiveDisconnect(const Json::Value& /*data*/)
    {
        Json::Value result;

        if (mGameConnection == nullptr || !mGameConnection->IsConnected())
        {
            result["ok"]    = false;
            result["error"] = "not connected";
            return result;
        }

        DIA_LOG_INFO("Inspector", "inspector.disconnect");
        mGameConnection->Disconnect();

        result["ok"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowInspectorPlugin::HandleLiveGetStatus(const Json::Value& /*data*/)
    {
        Json::Value result;
        result["ok"]         = true;
        result["connected"]  = (mGameConnection != nullptr && mGameConnection->IsConnected());
        result["liveActive"] = mLiveStore.IsActive();
        return result;
    }

    Json::Value DiaApplicationFlowInspectorPlugin::HandleLiveTransitionTo(const Json::Value& data)
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

        DIA_LOG_INFO("Inspector", "inspector.transition_to stage=%s", data["stageName"].asCString());

        Json::Value args;
        args["stage"] = data["stageName"];

        mGameConnection->SendCommandWithResponse("transition_to", args,
            [this](bool success, const Json::Value& res)
            {
                Json::Value notification;
                notification["success"] = success;
                notification["result"]  = res;
                if (GetBridge())
                    GetBridge()->NotifyUIDataChanged("live.transitionResult", notification);
            });

        result["ok"]      = true;
        result["pending"] = true;
        return result;
    }

    Json::Value DiaApplicationFlowInspectorPlugin::HandleLiveShutdown(const Json::Value& /*data*/)
    {
        Json::Value result;

        if (mGameConnection == nullptr || !mGameConnection->IsConnected())
        {
            result["ok"]    = false;
            result["error"] = "not connected";
            return result;
        }

        DIA_LOG_WARNING("Inspector", "inspector.shutdown_command");
        mGameConnection->SendCommand("shutdown", Json::Value());

        result["ok"] = true;
        return result;
    }

}} // namespace Dia::Editor

using namespace Dia::Editor;
REGISTER_EDITOR_PLUGIN(DiaApplicationFlowInspectorPlugin, "DiaApplicationFlowInspector")
