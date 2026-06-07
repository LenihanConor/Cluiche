#include "DiaApplicationFlowInspector/DiaApplicationFlowInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaObservation/Log/DiaLog.h>
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

        DIA_LOG_INFO("Inspector", "DiaApplicationFlowInspectorPlugin: loaded");

        RegisterHandler(kReqLiveConnect,      [this](const Json::Value& d) { return HandleLiveConnect(d); });
        RegisterHandler(kReqLiveDisconnect,   [this](const Json::Value& d) { return HandleLiveDisconnect(d); });
        RegisterHandler(kReqLiveGetStatus,    [this](const Json::Value& d) { return HandleLiveGetStatus(d); });
        RegisterHandler(kReqLiveTransitionTo, [this](const Json::Value& d) { return HandleLiveTransitionTo(d); });
        RegisterHandler(kReqLiveShutdown,     [this](const Json::Value& d) { return HandleLiveShutdown(d); });
    }

    void DiaApplicationFlowInspectorPlugin::OnPluginUnload()
    {
        if (mGameConnection != nullptr && mGameConnection->IsConnected())
        {
            mGameConnection->Disconnect();
        }

        mGameConnection = nullptr;

        DIA_LOG_INFO("Inspector", "DiaApplicationFlowInspectorPlugin: unloaded");
    }

    void DiaApplicationFlowInspectorPlugin::OnUpdate(float deltaTime)
    {
        if (mGameConnection != nullptr)
        {
            mGameConnection->Update(deltaTime);
        }
    }

    Json::Value DiaApplicationFlowInspectorPlugin::HandleLiveConnect(const Json::Value& data)
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

        DIA_LOG_INFO("Inspector", "Connecting to game at %s:%d", host, port);

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
                        Dia::ApplicationFlow::Editor::LiveAppState appState;
                        if (d.isMember("stage"))
                            appState.currentStage = Dia::Core::StringCRC(d["stage"].asCString());
                        appState.isTransitioning = d.isMember("transitioning") && d["transitioning"].asBool();
                        if (d.isMember("targetStage"))
                            appState.targetStage = Dia::Core::StringCRC(d["targetStage"].asCString());
                        mLiveStore.UpdateAppState(appState);
                        if (GetBridge())
                            GetBridge()->NotifyUIDataChanged("live.state", d);
                    });

                    mGameConnection->Subscribe(kTopicAppModules, [this](const Json::Value& d)
                    {
                        if (GetBridge())
                            GetBridge()->NotifyUIDataChanged("live.modules", d);
                    });

                    mGameConnection->Subscribe(kTopicAppStreams, [this](const Json::Value& d)
                    {
                        if (GetBridge())
                            GetBridge()->NotifyUIDataChanged("live.streams", d);
                    });
                }
                else
                {
                    mIsLiveConnected = false;
                    DIA_LOG_INFO("Inspector", "Game connection lost");

                    mGameConnection->Unsubscribe(kTopicAppState);
                    mGameConnection->Unsubscribe(kTopicAppModules);
                    mGameConnection->Unsubscribe(kTopicAppStreams);
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

        DIA_LOG_INFO("Inspector", "Disconnecting from game");
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

        mGameConnection->SendCommand("shutdown", Json::Value());

        result["ok"] = true;
        return result;
    }

}} // namespace Dia::Editor

using namespace Dia::Editor;
REGISTER_EDITOR_PLUGIN(DiaApplicationFlowInspectorPlugin, "DiaApplicationFlowInspector")
