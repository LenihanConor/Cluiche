#include "DiaEconomyInspector/Controllers/EconomyEventsController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia::EconomyInspector {

void EconomyEventsController::Init(Dia::Editor::WebUIBridge* bridge)
{
    mBridge = bridge;
}

void EconomyEventsController::OnPayload(const Json::Value& payload)
{
    mBridge->NotifyUIDataChanged("economy_inspector.events", payload);
}

} // namespace Dia::EconomyInspector
