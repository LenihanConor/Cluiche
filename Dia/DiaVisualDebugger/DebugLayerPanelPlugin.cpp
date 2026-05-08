////////////////////////////////////////////////////////////////////////////////
// Filename: DebugLayerPanelPlugin.cpp
// Description: Implementation of DebugLayerPanelPlugin
////////////////////////////////////////////////////////////////////////////////
#include "DebugLayerPanelPlugin.h"

#ifdef DIA_DEBUG

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
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

            if (context.mServices)
                mManager = context.mServices->GetService<Dia::Editor::GameConnectionManager>();

            if (!mManager)
            {
                DIA_LOG_WARNING("Editor", "DebugLayerPanelPlugin: GameConnectionManager service not available");
                return;
            }

            mManager->Subscribe(
                Dia::Core::StringCRC("debug.layer.state"),
                [this](const Json::Value& data) {
                    HandleLayerStateData(data);
                });

            DIA_LOG_INFO("Editor", "DebugLayerPanelPlugin: Subscribed to debug.layer.state");
        }

        // --------------------------------------------------------------------
        // OnUnload
        // --------------------------------------------------------------------

        void DebugLayerPanelPlugin::OnUnload()
        {
            DIA_LOG_INFO("Editor", "DebugLayerPanelPlugin: OnUnload");
            if (mManager)
                mManager->Unsubscribe(Dia::Core::StringCRC("debug.layer.state"));
            mManager = nullptr;
            mBridge = nullptr;
        }

        // --------------------------------------------------------------------
        // OnUpdate
        // --------------------------------------------------------------------

        void DebugLayerPanelPlugin::OnUpdate(float /*deltaTime*/)
        {
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
