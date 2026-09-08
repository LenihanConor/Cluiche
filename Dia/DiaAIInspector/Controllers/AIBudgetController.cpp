#include "DiaAIInspector/Controllers/AIBudgetController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <json/value.h>

namespace Dia::AIInspector {

void AIBudgetController::Init(Dia::Editor::WebUIBridge* bridge)
{
    mBridge = bridge;
}

void AIBudgetController::OnPayload(const Json::Value& payload)
{
    mBridge->NotifyUIDataChanged("ai_inspector.budget", payload);
}

} // namespace Dia::AIInspector
