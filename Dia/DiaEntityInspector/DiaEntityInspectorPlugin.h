#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>
#include "DiaEntityInspector/EntityInspectorController.h"
#include "DiaEntityInspector/QueryBrowserController.h"
#include "DiaEntityInspector/MailboxMonitorController.h"
#include "DiaEntityInspector/EntityWatchListController.h"

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
            "Live runtime inspection and field editing of diaentitytemplate state",
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
    void DispatchInspectPayload(const Json::Value& payload);

    Dia::Editor::GameConnectionManager* mManager = nullptr;
    bool mWasConnected = false;

    EntityInspectorController  mInspectorController;
    QueryBrowserController     mQueryController;
    MailboxMonitorController   mMailboxController;
    EntityWatchListController  mWatchController;
};

} // namespace Dia::EntityInspector
