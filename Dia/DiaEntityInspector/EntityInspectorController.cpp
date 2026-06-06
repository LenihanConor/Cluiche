#include "DiaEntityInspector/EntityInspectorController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::EntityInspector {

void EntityInspectorController::Activate(Dia::Editor::WebUIBridge* bridge)
{
    DIA_LOG_INFO("Editor", "EntityInspectorController: Activate");
    mBridge = bridge;
}

void EntityInspectorController::Deactivate()
{
    DIA_LOG_INFO("Editor", "EntityInspectorController: Deactivate");
    mBridge = nullptr;
}

void EntityInspectorController::OnInspectPayload(const Json::Value& payload)
{
    DIA_LOG_INFO("Editor", "EntityInspectorController::OnInspectPayload bridge=%s null=%d",
        mBridge ? "ok" : "null", payload.isNull() ? 1 : 0);
    if (!mBridge || payload.isNull()) return;
    DIA_LOG_INFO("Editor", "EntityInspectorController: pushing inspect_data to UI");
    mBridge->NotifyUIDataChanged("entity_inspector.inspect_data", payload);
}

} // namespace Dia::EntityInspector
