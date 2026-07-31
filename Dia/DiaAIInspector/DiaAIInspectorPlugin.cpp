#include "DiaAIInspector/DiaAIInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>

REGISTER_EDITOR_PLUGIN(Dia::AIInspector::DiaAIInspectorPlugin, "DiaAIInspector")

namespace Dia::AIInspector {

DiaAIInspectorPlugin::DiaAIInspectorPlugin()
    : LiveConnectionPluginBase({
        "AI Inspector", "1.0.0",
        "Live view of AI decision layer — budget, utility scores, rule fires, HTN plans",
        "dia://plugins/aiinspector/index.html",
        Dia::Editor::LayoutMode::kDockable,
        nullptr, nullptr, false
    }, "ai_inspector")
{}

void DiaAIInspectorPlugin::OnLivePluginLoad()
{
    mBudgetController.Init(GetBridge());
    mUtilityAIController.Init(GetBridge());
    mRulesController.Init(GetBridge());
    mHTNController.Init(GetBridge());

    RegisterGameTopic(
        Dia::Core::StringCRC{"ai_budget.state"},
        [this](const Json::Value& payload) { mBudgetController.OnPayload(payload); });

    RegisterGameTopic(
        Dia::Core::StringCRC{"utility_ai.state"},
        [this](const Json::Value& payload) { mUtilityAIController.OnPayload(payload); });

    RegisterGameTopic(
        Dia::Core::StringCRC{"rules.state"},
        [this](const Json::Value& payload) { mRulesController.OnPayload(payload); });

    RegisterGameTopic(
        Dia::Core::StringCRC{"htn.state"},
        [this](const Json::Value& payload) { mHTNController.OnPayload(payload); });
}

void DiaAIInspectorPlugin::OnLivePluginUnload()
{
    DIA_LOG_INFO("AIInspector", "Plugin unloaded");
}

void DiaAIInspectorPlugin::OnGameConnected()
{
    DIA_LOG_INFO("AIInspector", "Game connected");
}

void DiaAIInspectorPlugin::OnGameDisconnected()
{
    DIA_LOG_INFO("AIInspector", "Game disconnected");
}

} // namespace Dia::AIInspector
