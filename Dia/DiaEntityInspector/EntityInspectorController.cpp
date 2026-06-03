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
    if (!mBridge || payload.isNull()) return;
    mBridge->NotifyUIDataChanged("entity_inspector.inspect_data", payload);
}

} // namespace Dia::EntityInspector
