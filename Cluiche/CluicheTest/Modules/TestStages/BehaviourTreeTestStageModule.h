#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"

#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaOrder/IOrder.h>
#include <DiaOrder/IOrderQueueObserver.h>
#include <DiaOrder/OrderQueue.h>

namespace Dia::Observation::Metric { class Gauge; }

#ifdef DIA_DEBUG
#include <DiaApplicationFlow/ModuleRefV2.h>
#include "Modules/VisualDebuggerModule.h"
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <memory>
namespace Dia::BehaviourTree { class BehaviourTreeVisualDebugger; }
#endif

namespace CluicheTest {

class BehaviourTreeTestStageModule;

// ----------------------------------------------------------------
// Per-guard context passed verbatim to IOrder<> callbacks.
// ----------------------------------------------------------------
struct GuardOrderContext
{
    struct GuardAgent* guard = nullptr;
};

// ----------------------------------------------------------------
// GuardAgent — per-guard runtime state.
// Owns blackboard, BT component, and order queue.
// Not copyable (BehaviourTreeComponent is pimpl; stays in place).
// ----------------------------------------------------------------
struct GuardAgent
{
    Dia::Maths::Vector2D position;
    unsigned int         waypointIndex = 0;

    // Patrol waypoints — set from a per-guard table in OnStart.
    Dia::Maths::Vector2D waypoints[3];

    // Chase target — updated from mWandererPos each frame before BT tick.
    Dia::Maths::Vector2D chaseTarget;

    // Back-ptr to pre-allocated move order (one per guard, lives in module).
    struct GuardMoveOrder* ownMoveOrder = nullptr;
    bool orderInFlight = false;
    // Set by GuardMoveOrder::Finish; cleared by the action that observes it.
    bool orderJustCompleted = false;

    // Counts consecutive frames with target_visible=false (used to reset has_target).
    int targetLostFrames = 0;

    // Blackboard bool slots registered in OnStart:
    //   has_target      — chased BT condition
    //   target_visible  — alert BT condition
    Dia::Blackboard::Blackboard blackboard;

    Dia::BehaviourTree::BehaviourTreeComponent btComponent;
    Dia::Order::OrderQueue<GuardOrderContext>  orderQueue;

    // Back-pointer so static action callbacks reach module state.
    BehaviourTreeTestStageModule* modulePtr = nullptr;
};

// ----------------------------------------------------------------
// GuardMoveOrder — moves a guard toward a fixed target position.
// Pre-allocated per-guard; raw ptr passed to OrderQueue::Enqueue.
// ----------------------------------------------------------------
struct GuardMoveOrder : Dia::Order::IOrder<GuardOrderContext>
{
    Dia::Core::StringCRC GetOrderId() const override;
    void Start(GuardOrderContext& ctx)            override;
    bool Update(GuardOrderContext& ctx, float dt) override;
    void Finish(GuardOrderContext& ctx)           override;
    void Cancel(GuardOrderContext& ctx)           override;

    Dia::Maths::Vector2D target;
    float                speed = 2.5f;
};

// ----------------------------------------------------------------
// GuardOrderObserver — receives queue events for one guard.
// ----------------------------------------------------------------
struct GuardOrderObserver : Dia::Order::IOrderQueueObserver<GuardOrderContext>
{
    void OnQueueEmpty() override;

    GuardAgent*                   guard     = nullptr;
    BehaviourTreeTestStageModule* modulePtr = nullptr;
};

// ----------------------------------------------------------------
// BehaviourTreeTestStageModule
// ----------------------------------------------------------------
class BehaviourTreeTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs =
        Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "BehaviourTree E2E: guard patrol/alert/chase loop, shared BT asset, 3 independent guards";

    explicit BehaviourTreeTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int         GetBudgetFrames() const override { return 600; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    // ---- Static action callbacks (ActionFn signature) ----
    // actionContext is cast to GuardAgent*; access module state via guard->modulePtr.
    static Dia::BehaviourTree::NodeResult ActMoveToTarget(
        void* ctx,
        const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params);
    static Dia::BehaviourTree::NodeResult ActSetAlertEffect(
        void* ctx,
        const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params);
    static Dia::BehaviourTree::NodeResult ActSetHasTarget(
        void* ctx,
        const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params);
    static Dia::BehaviourTree::NodeResult ActMoveToWaypoint(
        void* ctx,
        const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params);

    void RegisterCheckpoints();

    // ---- Constants ----
    static constexpr unsigned int kGuardCount       = 3;
    static constexpr float kDetectionRange          = 4.0f;
    static constexpr float kWandererOrbitRadius     = 5.0f;
    // Fast orbit (7.5 m/s tangential) so the wanderer outpaces the guard (3.5 m/s chase)
    // and exits detection range, triggering the bt.target_lost checkpoint.
    static constexpr float kWandererAngularSpeed    = 1.5f;
    static constexpr float kWaypointReachDist       = 0.3f;
    static constexpr int   kTargetLostThreshold     = 30;

    // ---- Shared asset (one BT definition used by all guards) ----
    Dia::BehaviourTree::BehaviourTreeAsset mSharedBTAsset;
    Dia::BehaviourTree::ActionRegistry     mSharedActionRegistry;

    // ---- Per-guard state ----
    GuardAgent         mGuards[kGuardCount];
    GuardMoveOrder     mGuardMoveOrders[kGuardCount];   // pre-allocated; raw ptrs for Enqueue
    GuardOrderObserver mGuardObservers[kGuardCount];

    // ---- Wanderer (deterministic player proxy) ----
    Dia::Maths::Vector2D mWandererPos;
    float                mWandererAngle = 0.0f;

    // ---- Checkpoint flags ----
    bool mCheckPatrolStarted        = false;
    bool mCheckChaseTriggered       = false;
    bool mCheckTargetLost           = false;
    bool mCheckParallelFired        = false;
    bool mCheckMultiStateDivergence = false;

    // ---- Metrics (registered in OnStart, emitted each frame in OnUpdate) ----
    Dia::Observation::Metric::Gauge* mMetricTotalChases        = nullptr;
    Dia::Observation::Metric::Gauge* mMetricPatrolWaypoints    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricDecoratorCycles    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricTotalFrames        = nullptr;

    unsigned int mTotalChases          = 0;
    unsigned int mTotalPatrolWaypoints = 0;
    unsigned int mDecoratorCycles      = 0;
    unsigned int mFrameCount           = 0;

#ifdef DIA_DEBUG
    friend class BTDebugLayer;

    class BTDebugLayer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit BTDebugLayer(const BehaviourTreeTestStageModule* module) : mModule(module) {}
        Dia::Core::StringCRC GetLayerName() const override
        {
            return Dia::Core::StringCRC("CluicheTest.BehaviourTree");
        }
        void Draw(Dia::Core::IDebugDraw& draw) override;
    private:
        const BehaviourTreeTestStageModule* mModule = nullptr;
    };

    BTDebugLayer                                                             mBtDebugLayer{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<Dia::BehaviourTree::BehaviourTreeVisualDebugger>        mBtDebugDomain;
    bool mDomainRegistered = false;
#endif
};

} // namespace CluicheTest
