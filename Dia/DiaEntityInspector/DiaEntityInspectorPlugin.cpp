#include "DiaEntityInspector/DiaEntityInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
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
        DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: Connected to game");
    else
        DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: Disconnected from game");
}

} // namespace Dia::EntityInspector
