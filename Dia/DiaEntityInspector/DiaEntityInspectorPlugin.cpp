#include "DiaEntityInspector/DiaEntityInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEntity/DebugDataTypes.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

using namespace Dia::EntityInspector;

REGISTER_EDITOR_PLUGIN(DiaEntityInspectorPlugin, "DiaEntityInspector")

namespace Dia::EntityInspector {

void DiaEntityInspectorPlugin::OnLivePluginLoad()
{
    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: OnLivePluginLoad");

    RegisterGameTopic(
        Dia::Entity::DebugDataType::kEntityInspect,
        [this](const Json::Value& payload) { DispatchInspectPayload(payload); });

    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: OnLivePluginLoad complete");
}

void DiaEntityInspectorPlugin::OnLivePluginUnload()
{
    mInspectorController.Deactivate();
    mQueryController.Deactivate();
    mMailboxController.Deactivate();
    mWatchController.Deactivate();
}

void DiaEntityInspectorPlugin::OnGameConnected()
{
    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: Connected to game");
    mInspectorController.Activate(GetBridge());
    mQueryController.Activate(GetBridge());
    mMailboxController.Activate(GetBridge());
    mWatchController.Activate(GetBridge());
    mWatchController.RegisterHandlers();
    mWatchController.OnConnectionStateChanged(true);
}

void DiaEntityInspectorPlugin::OnGameDisconnected()
{
    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: Disconnected from game");
    mWatchController.OnConnectionStateChanged(false);
    mWatchController.UnregisterHandlers();
    mInspectorController.Deactivate();
    mQueryController.Deactivate();
    mMailboxController.Deactivate();
    mWatchController.Deactivate();
}

void DiaEntityInspectorPlugin::DispatchInspectPayload(const Json::Value& payload)
{
    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: entity.inspect received — null=%d keys: entity=%d components=%d entities=%d",
        payload.isNull() ? 1 : 0,
        payload.isMember("entity") ? 1 : 0,
        payload.isMember("components") ? 1 : 0,
        payload.isMember("entities") ? 1 : 0);

    mInspectorController.OnInspectPayload(payload);
    mQueryController.OnInspectPayload(payload);
    mMailboxController.OnInspectPayload(payload);
    mWatchController.OnInspectPayload(payload);
}

} // namespace Dia::EntityInspector
