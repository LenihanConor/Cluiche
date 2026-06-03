#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace Editor {
    class WebUIBridge;
    class GameConnectionManager;
    class IPluginLoader;
} }

namespace Dia::EntityInspector {

class EntityInspectorController;
class QueryBrowserController;
class MailboxMonitorController;
class EntityWatchListController;

class DiaEntityInspectorPlugin final : public Dia::Editor::IEditorPlugin
{
public:
    const char* GetName()        const override { return "DiaEntityInspector"; }
    const char* GetVersion()     const override { return "1.0.0"; }
    const char* GetDescription() const override { return "Live runtime inspection and field editing of DiaEntity state"; }
    const char* GetUIPath()      const override { return "dia://plugins/entityinspector/index.html"; }
    Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

    void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
    void OnUnload() override;
    void OnUpdate(float deltaTime) override;

private:
    void RegisterRequestHandlers();
    void HandleConnectionStateChange(bool connected);

    Dia::Editor::WebUIBridge*         mBridge      = nullptr;
    Dia::Editor::IPluginLoader*       mPluginLoader = nullptr;
    Dia::Editor::GameConnectionManager* mManager   = nullptr;
    bool                              mWasConnected = false;
};

} // namespace Dia::EntityInspector
