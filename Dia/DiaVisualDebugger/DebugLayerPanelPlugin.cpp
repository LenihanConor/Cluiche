////////////////////////////////////////////////////////////////////////////////
// Filename: DebugLayerPanelPlugin.cpp
// Description: Implementation of DebugLayerPanelPlugin
////////////////////////////////////////////////////////////////////////////////
#include "DebugLayerPanelPlugin.h"

#ifdef DIA_DEBUG

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
    namespace Debug
    {
        // --------------------------------------------------------------------
        // GetToolbarItem
        // --------------------------------------------------------------------

        Dia::Editor::EditorToolbarItem DebugLayerPanelPlugin::GetToolbarItem() const
        {
            Dia::Editor::EditorToolbarItem item;
            strncpy_s(item.label, sizeof(item.label), "Debug Layers", _TRUNCATE);
            item.iconChar[0] = 'D';
            item.iconChar[1] = '\0';
            item.pinned = false;
            return item;
        }

        // --------------------------------------------------------------------
        // OnLoad
        // --------------------------------------------------------------------

        void DebugLayerPanelPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
        {
            DIA_LOG_INFO("Editor", "DebugLayerPanelPlugin: OnLoad");
            mBridge = context.mBridge;

            mManager.Initialize();

            // Subscribe to the debug.layer.state topic broadcast by
            // DebugLayerManager::BroadcastLayerState() via NotifySubscribers().
            mManager.Subscribe(
                Dia::Core::StringCRC("debug.layer.state"),
                [this](const Json::Value& data) {
                    HandleLayerStateData(data);
                });

            mManager.Connect("localhost", 9002);
            DIA_LOG_INFO("Editor", "DebugLayerPanelPlugin: Subscribed to debug.layer.state");
        }

        // --------------------------------------------------------------------
        // OnUnload
        // --------------------------------------------------------------------

        void DebugLayerPanelPlugin::OnUnload()
        {
            DIA_LOG_INFO("Editor", "DebugLayerPanelPlugin: OnUnload");
            mManager.Unsubscribe(Dia::Core::StringCRC("debug.layer.state"));
            mManager.Shutdown();
            mBridge = nullptr;
        }

        // --------------------------------------------------------------------
        // OnUpdate — pump the connection manager
        // --------------------------------------------------------------------

        void DebugLayerPanelPlugin::OnUpdate(float deltaTime)
        {
            mManager.Update(deltaTime);
        }

        // --------------------------------------------------------------------
        // HandleLayerStateData
        // Invoked when a "debug.layer.state" data-update arrives from the game.
        // Pushes the JSON payload to the CEF front-end via WebUIBridge.
        // --------------------------------------------------------------------

        void DebugLayerPanelPlugin::HandleLayerStateData(const Json::Value& data)
        {
            if (mBridge == nullptr) return;
            mBridge->NotifyUIDataChanged("debug.layer.state", data);
        }

    } // namespace Debug
} // namespace Dia

using namespace Dia::Debug;

REGISTER_EDITOR_PLUGIN(DebugLayerPanelPlugin, "DebugLayerPanelPlugin")

#endif // DIA_DEBUG
