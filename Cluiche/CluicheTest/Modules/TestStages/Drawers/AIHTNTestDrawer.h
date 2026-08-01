#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaHTN/HTNPlannerComponent.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

class AIHTNTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    // Phase enum mirrored from the module (int cast)
    enum class Phase { kFirstPlan, kExecuting, kMutating, kSecondPlan, kAsyncSubmit, kAsyncWait, kDone };

    AIHTNTestDrawer(
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
        const Dia::Debug::DebugLayerManager&  layerManager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const float&                          mHealth;
    const Phase&                          mPhase;
    const Dia::HTN::HTNPlannerComponent&  mHTNComponent;
    const int&                            mRetreatFireCount;
    const int&                            mAttackFireCount;
    const int&                            mCallForHelpFireCount;
    const bool&                           mPlanBuilt;
    const bool&                           mPlanComplete;
    const bool&                           mDivergedAndReplanned;
    const bool&                           mRuleBridgeFired;
    const bool&                           mAsyncPlanCompleted;
    const bool&                           mAsyncPlanCorrect;
    const bool&                           mAllPassed;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
