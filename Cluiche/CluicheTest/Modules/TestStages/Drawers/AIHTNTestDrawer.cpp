#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/AIHTNTestDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaHTN/HTNPlan.h>

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

} // namespace CluicheTest

#endif // DIA_DEBUG
