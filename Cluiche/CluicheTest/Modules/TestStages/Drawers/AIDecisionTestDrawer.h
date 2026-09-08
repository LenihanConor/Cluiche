#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

class AIDecisionTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    AIDecisionTestDrawer(
        const float&        health,
        const bool&         enemyVisible,
        const float&        enemyDistance,
        const bool&         conditionHealthLow,
        const bool&         conditionEnemyVisible,
        const bool&         rulesCallForHelpFired,
        const bool&         utilityFleeWins,
        const bool&         budgetAsyncCompleted,
        const bool&         allPassed,
        const Dia::Debug::DebugLayerManager& layerManager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const float&  mHealth;
    const bool&   mEnemyVisible;
    const float&  mEnemyDistance;
    const bool&   mConditionHealthLow;
    const bool&   mConditionEnemyVisible;
    const bool&   mRulesCallForHelpFired;
    const bool&   mUtilityFleeWins;
    const bool&   mBudgetAsyncCompleted;
    const bool&   mAllPassed;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
