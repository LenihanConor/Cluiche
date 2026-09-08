#include "DiaEconomyInspector/Controllers/EconomyInstancesController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia::EconomyInspector {

void EconomyInstancesController::Init(Dia::Editor::WebUIBridge* bridge)
{
    mBridge = bridge;
}

void EconomyInstancesController::OnPayload(const Json::Value& payload)
{
    mBridge->NotifyUIDataChanged("economy_inspector.instances", payload);
}

} // namespace Dia::EconomyInspector
