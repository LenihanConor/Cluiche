#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/AIHTNTestDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaHTN/HTNPlan.h>
#include <imgui.h>

namespace CluicheTest {

static const char* PhaseLabel(AIHTNTestDrawer::Phase p)
{
    switch (p)
    {
    case AIHTNTestDrawer::Phase::kFirstPlan:   return "Building first plan (health=30)";
    case AIHTNTestDrawer::Phase::kExecuting:   return "Executing: Retreat + CallForHelp";
    case AIHTNTestDrawer::Phase::kMutating:    return "Mutating blackboard (health=80)";
    case AIHTNTestDrawer::Phase::kSecondPlan:  return "Detecting divergence + replanning";
    case AIHTNTestDrawer::Phase::kAsyncSubmit: return "Submitting async plan";
    case AIHTNTestDrawer::Phase::kAsyncWait:   return "Waiting for async callback";
    case AIHTNTestDrawer::Phase::kDone:        return "Done";
    default:                                   return "Unknown";
    }
}

AIHTNTestDrawer::AIHTNTestDrawer(
    const float&                          health,
    const Phase&                          phase,
    const Dia::HTN::HTNPlannerComponent&  htnComponent,
    const int&                            retreatFireCount,
    const int&                            attackFireCount,
    const int&                            callForHelpFireCount,
    const bool&                           planBuilt,
    const bool&                           planComplete,
    const bool&                           divergedAndReplanned,
    const bool&                           ruleBridgeFired,
    const bool&                           asyncPlanCompleted,
    const bool&                           asyncPlanCorrect,
    const bool&                           allPassed,
    const Dia::Debug::DebugLayerManager&  /*layerManager*/)
    : mHealth(health)
    , mPhase(phase)
    , mHTNComponent(htnComponent)
    , mRetreatFireCount(retreatFireCount)
    , mAttackFireCount(attackFireCount)
    , mCallForHelpFireCount(callForHelpFireCount)
    , mPlanBuilt(planBuilt)
    , mPlanComplete(planComplete)
    , mDivergedAndReplanned(divergedAndReplanned)
    , mRuleBridgeFired(ruleBridgeFired)
    , mAsyncPlanCompleted(asyncPlanCompleted)
    , mAsyncPlanCorrect(asyncPlanCorrect)
    , mAllPassed(allPassed)
{}

Dia::Core::StringCRC AIHTNTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("ai.htn");
}

void AIHTNTestDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/) {}

void AIHTNTestDrawer::DrawImGui()
{
    static const ImVec4 kGreen  { 0.2f, 1.0f, 0.2f, 1.0f };
    static const ImVec4 kGrey   { 0.5f, 0.5f, 0.5f, 1.0f };
    static const ImVec4 kYellow { 1.0f, 1.0f, 0.2f, 1.0f };
    static const ImVec4 kOrange { 1.0f, 0.6f, 0.1f, 1.0f };

    // --- Blackboard ---
    ImGui::TextColored(kYellow, "Blackboard");
    ImGui::Separator();
    ImGui::Text("  self.health: %.1f", mHealth);

    ImGui::Spacing();

    // --- Phase ---
    ImGui::TextColored(kYellow, "Phase");
    ImGui::Separator();
    ImGui::TextColored(kOrange, "  %s", PhaseLabel(mPhase));

    ImGui::Spacing();

    // --- Active plan ---
    ImGui::TextColored(kYellow, "Active Plan");
    ImGui::Separator();
    const Dia::HTN::HTNPlan* plan = mHTNComponent.GetActivePlan();
    if (plan && !plan->IsEmpty())
    {
        ImGui::Text("  Tasks: %d  |  %s",
            plan->GetTaskCount(),
            plan->IsComplete() ? "COMPLETE" : "running");
        if (!plan->IsComplete())
            ImGui::Text("  Current op: %s", plan->CurrentTask().operatorId.AsChar());
    }
    else
    {
        ImGui::TextDisabled("  (no active plan)");
    }

    ImGui::Spacing();

    // --- Operator fire counts ---
    ImGui::TextColored(kYellow, "Operator Counts");
    ImGui::Separator();
    ImGui::Text("  Retreat:     %d", mRetreatFireCount);
    ImGui::Text("  CallForHelp: %d", mCallForHelpFireCount);
    ImGui::Text("  Attack:      %d", mAttackFireCount);

    ImGui::Spacing();

    // --- Checkpoints ---
    ImGui::TextColored(kYellow, "Checkpoints");
    ImGui::Separator();

    auto row = [&](const char* label, bool passed)
    {
        ImGui::TextColored(passed ? kGreen : kGrey, "  [%s]  %s", passed ? "PASS" : "    ", label);
    };

    row("Sync plan built (health=30)",          mPlanBuilt);
    row("First plan executed to completion",    mPlanComplete);
    row("Diverged + replanned (health=80)",     mDivergedAndReplanned);
    row("RuleActionBridge CallForHelp fired",   mRuleBridgeFired);
    row("Async plan callback received",         mAsyncPlanCompleted);
    row("Async plan correct (Attack only)",     mAsyncPlanCorrect);

    ImGui::Spacing();
    ImGui::Separator();

    if (mAllPassed)
        ImGui::TextColored(kGreen, "ALL PASSED");
    else
        ImGui::TextColored(kGrey, "running...");
}

} // namespace CluicheTest

#endif // DIA_DEBUG
