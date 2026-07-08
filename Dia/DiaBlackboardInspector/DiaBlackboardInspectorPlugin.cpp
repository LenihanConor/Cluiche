#include "DiaBlackboardInspector/DiaBlackboardInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>

using namespace Dia::Editor;

REGISTER_EDITOR_PLUGIN(DiaBlackboardInspectorPlugin, "DiaBlackboardInspector")

namespace Dia::Editor {

void DiaBlackboardInspectorPlugin::OnLivePluginLoad()
{
    DIA_LOG_INFO("BlackboardInspector", "DiaBlackboardInspectorPlugin: OnLivePluginLoad");

    RegisterGameTopic(
        Dia::Core::StringCRC{"blackboard.state"},
        [this](const Json::Value& payload) { OnBlackboardStateUpdate(payload); });

    DIA_LOG_INFO("BlackboardInspector", "DiaBlackboardInspectorPlugin: OnLivePluginLoad complete");
}

void DiaBlackboardInspectorPlugin::OnLivePluginUnload()
{
    DIA_LOG_INFO("BlackboardInspector", "DiaBlackboardInspectorPlugin: OnLivePluginUnload");
}

void DiaBlackboardInspectorPlugin::OnGameConnected()
{
    DIA_LOG_INFO("BlackboardInspector", "DiaBlackboardInspectorPlugin: Connected to game");
}

void DiaBlackboardInspectorPlugin::OnGameDisconnected()
{
    DIA_LOG_INFO("BlackboardInspector", "DiaBlackboardInspectorPlugin: Disconnected from game");
}

void DiaBlackboardInspectorPlugin::OnBlackboardStateUpdate(const Json::Value& payload)
{
    GetBridge()->NotifyUIDataChanged("blackboard_inspector.state", payload);
}

} // namespace Dia::Editor
