#include "HTNVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaHTN/HTNPlannerComponent.h>
#include <DiaHTN/HTNPlan.h>

namespace Dia::HTN
{

HTNVisualDebugger::HTNVisualDebugger(const HTNPlannerComponent& component)
    : mComponent(component)
{}

Dia::Core::StringCRC HTNVisualDebugger::GetDomainId()    const { return Dia::Core::StringCRC("htn"); }
const char* HTNVisualDebugger::GetDisplayName()          const { return "HTN"; }
const char* HTNVisualDebugger::GetDescription()          const { return "HTN planner \xe2\x80\x94 active plan, task cursor, divergence and replan state"; }
Dia::Core::StringCRC HTNVisualDebugger::GetGroup()       const { return Dia::Core::StringCRC("AIBehavior"); }
Dia::Core::RGBA HTNVisualDebugger::GetAccentColour()     const { return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior; }

void HTNVisualDebugger::GetJSONState(Json::Value& out)
{
    // Stub — implemented in Task 3
    out["drawers"] = Json::Value(Json::arrayValue);
    out["stats"]   = Json::Value(Json::objectValue);
}

void HTNVisualDebugger::OnCommand(Dia::Core::StringCRC /*cmd*/, const Json::Value& /*args*/)
{
    // Stub — implemented in Task 4
}

} // namespace Dia::HTN

#endif // DIA_DEBUG
