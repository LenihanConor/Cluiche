#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/AIDecisionTestDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

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

} // namespace CluicheTest

#endif // DIA_DEBUG
