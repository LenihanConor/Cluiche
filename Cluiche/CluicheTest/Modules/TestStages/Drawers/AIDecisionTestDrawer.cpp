#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/AIDecisionTestDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <imgui.h>

namespace CluicheTest {

AIDecisionTestDrawer::AIDecisionTestDrawer(
    const float&        health,
    const bool&         enemyVisible,
    const float&        enemyDistance,
    const bool&         conditionHealthLow,
    const bool&         conditionEnemyVisible,
    const bool&         rulesCallForHelpFired,
    const bool&         utilityFleeWins,
    const bool&         budgetAsyncCompleted,
    const bool&         allPassed,
    const Dia::Debug::DebugLayerManager& /*layerManager*/)
    : mHealth(health)
    , mEnemyVisible(enemyVisible)
    , mEnemyDistance(enemyDistance)
    , mConditionHealthLow(conditionHealthLow)
    , mConditionEnemyVisible(conditionEnemyVisible)
    , mRulesCallForHelpFired(rulesCallForHelpFired)
    , mUtilityFleeWins(utilityFleeWins)
    , mBudgetAsyncCompleted(budgetAsyncCompleted)
    , mAllPassed(allPassed)
{}

Dia::Core::StringCRC AIDecisionTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("ai.decision");
}

void AIDecisionTestDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/) {}

void AIDecisionTestDrawer::DrawImGui()
{
    static const ImVec4 kGreen  { 0.2f, 1.0f, 0.2f, 1.0f };
    static const ImVec4 kRed    { 1.0f, 0.3f, 0.3f, 1.0f };
    static const ImVec4 kGrey   { 0.5f, 0.5f, 0.5f, 1.0f };
    static const ImVec4 kYellow { 1.0f, 1.0f, 0.2f, 1.0f };

    // --- Blackboard ---
    ImGui::TextColored(kYellow, "Blackboard");
    ImGui::Separator();
    ImGui::Text("  self.health:      %.1f", mHealth);
    ImGui::Text("  enemy.visible:    %s",   mEnemyVisible ? "true" : "false");
    ImGui::Text("  enemy.distance:   %.1f", mEnemyDistance);

    ImGui::Spacing();

    // --- Checkpoint rows ---
    ImGui::TextColored(kYellow, "Checkpoints");
    ImGui::Separator();

    auto row = [&](const char* label, bool passed)
    {
        ImGui::TextColored(passed ? kGreen : kGrey, "  [%s]  %s", passed ? "PASS" : "    ", label);
    };

    row("ConditionExpr  health < 50",           mConditionHealthLow);
    row("ConditionExpr  enemy.visible == true",  mConditionEnemyVisible);
    row("RuleSet        CallForHelp fired",       mRulesCallForHelpFired);
    row("UtilityAI      Flee wins (sync)",        mUtilityFleeWins);
    row("AIBudget       EvaluateAsync callback",  mBudgetAsyncCompleted);

    ImGui::Spacing();
    ImGui::Separator();

    if (mAllPassed)
        ImGui::TextColored(kGreen, "ALL PASSED");
    else
        ImGui::TextColored(kGrey, "running...");
}

} // namespace CluicheTest

#endif // DIA_DEBUG
