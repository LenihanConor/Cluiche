#include "DiaEntityInspector/DiaEntityInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaEntity/DebugDataTypes.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

using namespace Dia::EntityInspector;

REGISTER_EDITOR_PLUGIN(DiaEntityInspectorPlugin, "DiaEntityInspector")

namespace Dia::EntityInspector {

void DiaEntityInspectorPlugin::OnPluginLoad()
{
    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: OnPluginLoad");

    if (GetServices())
        mManager = GetServices()->GetService<Dia::Editor::GameConnectionManager>();

    if (!mManager)
        DIA_LOG_WARNING("Editor", "DiaEntityInspectorPlugin: GameConnectionManager not available");

    RegisterHandler(
        Dia::Core::StringCRC("entity_inspector.get_connection_state"),
        [this](const Json::Value& /*data*/) -> Json::Value
        {
            Json::Value result;
            result["connected"] = (mManager != nullptr && mManager->IsConnected());
            return result;
        });

    if (mManager)
    {
        mManager->Subscribe(
            Dia::Entity::DebugDataType::kEntityInspect,
            [this](const Json::Value& payload) { DispatchInspectPayload(payload); });
    }

    if (mManager && mManager->IsConnected())
        HandleConnectionStateChange(true);

    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: OnPluginLoad complete");
}

void DiaEntityInspectorPlugin::OnUpdate(float /*deltaTime*/)
{
    if (mManager)
    {
        bool isConnected = mManager->IsConnected();
        if (isConnected != mWasConnected)
        {
            mWasConnected = isConnected;
            HandleConnectionStateChange(isConnected);
        }
    }
}

void DiaEntityInspectorPlugin::HandleConnectionStateChange(bool connected)
{
    if (GetBridge())
    {
        Json::Value data;
        data["connected"] = connected;
        GetBridge()->NotifyUIDataChanged("entity_inspector.connection_state", data);
    }

    if (connected)
    {
        DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: Connected to game");
        mInspectorController.Activate(GetBridge());
        mQueryController.Activate(GetBridge());
        mMailboxController.Activate(GetBridge());
        mWatchController.Activate(GetBridge());
        mWatchController.RegisterHandlers();
        mWatchController.OnConnectionStateChanged(true);
    }
    else
    {
        DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: Disconnected from game");
        mWatchController.OnConnectionStateChanged(false);
        mWatchController.UnregisterHandlers();
        mInspectorController.Deactivate();
        mQueryController.Deactivate();
        mMailboxController.Deactivate();
        mWatchController.Deactivate();
    }
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
