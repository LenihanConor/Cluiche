#include "DiaAIInspector/Controllers/UtilityAIController.h"
#include <DiaEditor/WebUIBridge.h>
#include <json/value.h>

namespace Dia::AIInspector {

void UtilityAIController::Init(Dia::Editor::WebUIBridge* bridge)
{
    mBridge = bridge;
}

void UtilityAIController::OnPayload(const Json::Value& payload)
{
    mBridge->NotifyUIDataChanged("ai_inspector.utility_ai", payload);
}

} // namespace Dia::AIInspector
