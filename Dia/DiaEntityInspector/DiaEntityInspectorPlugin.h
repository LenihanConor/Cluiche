#pragma once

#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>
#include "DiaEntityInspector/EntityInspectorController.h"
#include "DiaEntityInspector/QueryBrowserController.h"
#include "DiaEntityInspector/MailboxMonitorController.h"
#include "DiaEntityInspector/EntityWatchListController.h"

namespace Dia::EntityInspector {

class DiaEntityInspectorPlugin final : public Dia::Editor::LiveConnectionPluginBase
{
public:
    DiaEntityInspectorPlugin()
        : LiveConnectionPluginBase({
            "DiaEntityInspector",
            "1.0.0",
            "Live runtime inspection and field editing of diaentitytemplate state",
            "dia://plugins/entityinspector/index.html",
            Dia::Editor::LayoutMode::kDockable,
            nullptr,
            nullptr,
            false
        }, "entity_inspector")
    {}

protected:
    void OnLivePluginLoad() override;
    void OnLivePluginUnload() override;
    void OnGameConnected() override;
    void OnGameDisconnected() override;

private:
    void DispatchInspectPayload(const Json::Value& payload);

    EntityInspectorController  mInspectorController;
    QueryBrowserController     mQueryController;
    MailboxMonitorController   mMailboxController;
    EntityWatchListController  mWatchController;
};

} // namespace Dia::EntityInspector
