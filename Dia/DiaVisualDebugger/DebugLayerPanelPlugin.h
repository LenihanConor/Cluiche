////////////////////////////////////////////////////////////////////////////////
// Filename: DebugLayerPanelPlugin.h
// Description: Editor plugin that shows registered debug layers as a toggle
//              list and badges when primitives are dropped. Subscribes to the
//              "debug.layer.state" DATA_UPDATE topic broadcast by
//              DebugLayerManager::BroadcastLayerState().
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-editor-panel.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace Debug
    {
        ////////////////////////////////////////////////////////////////////////////////
        // DebugLayerPanelPlugin
        //
        // Dockable editor panel for the DiaVisualDebugger system.
        //
        // On load:
        //   - Initialises a GameConnectionManager pointing at ws://localhost:9002
        //   - Subscribes to the "debug.layer.state" topic
        //   - On each data update: calls mBridge->NotifyUIDataChanged to push state
        //     to the CEF front-end
        //
        // Toggle commands:
        //   The JS front-end calls SendCommand("debug.layer.enable"/"debug.layer.disable")
        //   which routes to the game's CommandRegistry via MESSAGE_TYPE_COMMAND_REQUEST.
        ////////////////////////////////////////////////////////////////////////////////
        class DebugLayerPanelPlugin : public Dia::Editor::IEditorPlugin
        {
        public:
            const char* GetName()        const override { return "Debug Layers"; }
            const char* GetVersion()     const override { return "1.0"; }
            const char* GetDescription() const override { return "Toggle debug draw layers and monitor primitive budget"; }
            const char* GetUIPath()      const override { return "dia://plugins/debug-layers/index.html"; }
            Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

            Dia::Editor::EditorToolbarItem GetToolbarItem() const override;

            void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
            void OnUnload() override;
            void OnUpdate(float deltaTime) override;

        private:
            void HandleLayerStateData(const Json::Value& data);

            Dia::Editor::WebUIBridge*        mBridge  = nullptr;
            Dia::Editor::GameConnectionManager mManager;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
