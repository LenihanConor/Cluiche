#include "DiaEntityInspector/DiaEntityInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

using namespace Dia::EntityInspector;

REGISTER_EDITOR_PLUGIN(DiaEntityInspectorPlugin, "DiaEntityInspector")

namespace Dia::EntityInspector {

void DiaEntityInspectorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
{
    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: OnLoad");

    mBridge       = context.mBridge;
    mPluginLoader = context.mPluginLoader;

    if (context.mServices)
        mManager = context.mServices->GetService<Dia::Editor::GameConnectionManager>();

    if (!mManager)
        DIA_LOG_WARNING("Editor", "DiaEntityInspectorPlugin: GameConnectionManager not available");

    RegisterRequestHandlers();

    if (mManager && mManager->IsConnected())
        HandleConnectionStateChange(true);

    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: OnLoad complete");
}

void DiaEntityInspectorPlugin::OnUnload()
{
    DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: OnUnload");

    if (mBridge)
    {
        mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("entity_inspector.get_connection_state"));
    }

    mManager      = nullptr;
    mBridge       = nullptr;
    mPluginLoader = nullptr;
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

void DiaEntityInspectorPlugin::RegisterRequestHandlers()
{
    if (!mBridge)
        return;

    mBridge->RegisterRequestHandler(
        Dia::Core::StringCRC("entity_inspector.get_connection_state"),
        [this](const Json::Value& /*data*/) -> Json::Value
        {
            Json::Value result;
            result["connected"] = (mManager != nullptr && mManager->IsConnected());
            return result;
        });
}

void DiaEntityInspectorPlugin::HandleConnectionStateChange(bool connected)
{
    if (mBridge)
    {
        Json::Value data;
        data["connected"] = connected;
        mBridge->NotifyUIDataChanged("entity_inspector.connection_state", data);
    }

    if (connected)
        DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: Connected to game");
    else
        DIA_LOG_INFO("Editor", "DiaEntityInspectorPlugin: Disconnected from game");
}

} // namespace Dia::EntityInspector
