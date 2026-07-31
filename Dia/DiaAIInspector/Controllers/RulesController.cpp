#include "DiaAIInspector/Controllers/RulesController.h"
#include <DiaEditor/WebUIBridge.h>
#include <json/value.h>

namespace Dia::AIInspector {

void RulesController::Init(Dia::Editor::WebUIBridge* bridge)
{
    mBridge = bridge;
}

void RulesController::OnPayload(const Json::Value& payload)
{
    mBridge->NotifyUIDataChanged("ai_inspector.rules", payload);
}

} // namespace Dia::AIInspector
