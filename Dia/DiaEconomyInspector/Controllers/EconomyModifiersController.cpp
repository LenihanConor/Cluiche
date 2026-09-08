#include "DiaEconomyInspector/Controllers/EconomyModifiersController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia::EconomyInspector {

void EconomyModifiersController::Init(Dia::Editor::WebUIBridge* bridge)
{
    mBridge = bridge;
}

void EconomyModifiersController::OnPayload(const Json::Value& payload)
{
    mBridge->NotifyUIDataChanged("economy_inspector.modifiers", payload);
}

} // namespace Dia::EconomyInspector
