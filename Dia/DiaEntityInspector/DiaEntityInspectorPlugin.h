#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>

namespace Dia { namespace Editor {
    class GameConnectionManager;
} }

namespace Dia::EntityInspector {

class DiaEntityInspectorPlugin final : public Dia::Editor::EditorPluginBase
{
public:
    DiaEntityInspectorPlugin()
        : EditorPluginBase({
            "DiaEntityInspector",
            "1.0.0",
            "Live runtime inspection and field editing of DiaEntity state",
            "dia://plugins/entityinspector/index.html",
            Dia::Editor::LayoutMode::kDockable,
            nullptr,
            nullptr,
            false
        })
    {}

protected:
    void OnPluginLoad() override;
    void OnUpdate(float deltaTime) override;

private:
    void HandleConnectionStateChange(bool connected);

    Dia::Editor::GameConnectionManager* mManager = nullptr;
    bool mWasConnected = false;
};

} // namespace Dia::EntityInspector
