#include "DiaEconomyInspector/Controllers/EconomySchemaController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia::EconomyInspector {

void EconomySchemaController::Init(Dia::Editor::WebUIBridge* bridge)
{
    mBridge = bridge;
}

void EconomySchemaController::OnPayload(const Json::Value& payload)
{
    mBridge->NotifyUIDataChanged("economy_inspector.schema", payload);
}

} // namespace Dia::EconomyInspector
