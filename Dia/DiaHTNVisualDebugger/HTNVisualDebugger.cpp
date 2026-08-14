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
    const bool planViewEnabled = mPlanViewEnabled.load();

    // drawers
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "PlanView";
        entry["enabled"] = planViewEnabled;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    // stats
    const HTNPlan* plan     = mComponent.GetActivePlan();
    const bool     hasActive = mComponent.HasActivePlan();
    const int      total    = hasActive ? plan->GetTaskCount() : 0;
    const int      current  = hasActive ? plan->GetCurrentTaskIndex() : 0;

    Json::Value stats(Json::objectValue);
    stats["current"] = current;
    stats["total"]   = total;
    out["stats"] = stats;

    // diverged — HasDiverged() requires IConditionContext which the debugger does not hold;
    // emit false as a safe default (callers may set a flag via OnCommand if needed).
    out["diverged"] = false;

    // plan array — only emitted when PlanView drawer is enabled
    if (planViewEnabled)
    {
        Json::Value planArray(Json::arrayValue);
        if (hasActive)
        {
            for (int i = 0; i < total; ++i)
            {
                Json::Value task(Json::objectValue);
                task["index"] = i + 1; // 1-based per spec
                task["name"]  = plan->GetTask(i).operatorId.AsChar();
                if (i < current)
                    task["status"] = "done";
                else if (i == current)
                    task["status"] = "current";
                else
                    task["status"] = "pending";
                planArray.append(task);
            }
        }
        out["plan"] = planArray;
    }
}

void HTNVisualDebugger::OnCommand(Dia::Core::StringCRC /*cmd*/, const Json::Value& /*args*/)
{
    // Stub — implemented in Task 4
}

} // namespace Dia::HTN

#endif // DIA_DEBUG
