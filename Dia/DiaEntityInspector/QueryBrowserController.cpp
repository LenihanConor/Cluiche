#include "DiaEntityInspector/QueryBrowserController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::EntityInspector {

void QueryBrowserController::Activate(Dia::Editor::WebUIBridge* bridge)
{
    DIA_LOG_INFO("Editor", "QueryBrowserController: Activate");
    mBridge = bridge;
}
void QueryBrowserController::Deactivate()
{
    DIA_LOG_INFO("Editor", "QueryBrowserController: Deactivate");
    mBridge = nullptr;
}

void QueryBrowserController::OnInspectPayload(const Json::Value& payload)
{
    if (!mBridge || payload.isNull()) return;
    if (payload.isMember("queries"))
    {
        Json::Value msg;
        msg["queries"] = payload["queries"];
        mBridge->NotifyUIDataChanged("entity_inspector.query_data", msg);
    }
}

} // namespace Dia::EntityInspector
