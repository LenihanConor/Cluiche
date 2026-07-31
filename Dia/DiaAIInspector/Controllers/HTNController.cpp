#include "DiaAIInspector/Controllers/HTNController.h"
#include <DiaEditor/WebUIBridge.h>
#include <json/value.h>

namespace Dia::AIInspector {

void HTNController::Init(Dia::Editor::WebUIBridge* bridge)
{
    mBridge = bridge;
}

void HTNController::OnPayload(const Json::Value& payload)
{
    mBridge->NotifyUIDataChanged("ai_inspector.htn", payload);
}

} // namespace Dia::AIInspector
