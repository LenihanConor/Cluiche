#include "Modules/TestStages/BehaviourTreeTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaCore/Json/external/json/json.h>
#include <cmath>

#ifdef DIA_DEBUG
#include <DiaBehaviourTreeVisualDebugger/BehaviourTreeVisualDebugger.h>
#endif

namespace CluicheTest {

// ---------------------------------------------------------------------------
// BT JSON — shared across all 3 guards
// ---------------------------------------------------------------------------
static const char* kGuardBtJson = R"({
  "root": "root_select",
  "nodes": {
    "root_select":        { "type": "selector",  "children": ["chase_seq","alert_seq","patrol_repeat"] },
    "chase_seq":          { "type": "sequence",  "children": ["cond_has_target","chase_par"] },
    "cond_has_target":    { "type": "condition", "blackboard_key": "has_target" },
    "chase_par":          { "type": "parallel",  "policy": "require_all", "children": ["act_move_to_target","act_set_alert_effect"] },
    "act_move_to_target": { "type": "action",    "action_id": "move_to_target",   "params": [] },
    "act_set_alert_effect":{ "type": "action",   "action_id": "set_alert_effect", "params": [] },
    "alert_seq":          { "type": "sequence",  "children": ["cond_target_visible","act_set_has_target"] },
    "cond_target_visible":{ "type": "condition", "blackboard_key": "target_visible" },
    "act_set_has_target": { "type": "action",    "action_id": "set_has_target",   "params": [] },
    "patrol_repeat":      { "type": "decorator", "decorator": "repeater", "repeat_count": 0, "break_on_failure": false, "child": "act_move_to_waypoint" },
    "act_move_to_waypoint":{ "type": "action",   "action_id": "move_to_waypoint", "params": [] }
  }
})";

// ---------------------------------------------------------------------------
// Per-guard patrol waypoints and start positions
// ---------------------------------------------------------------------------
// Guards patrol near the wanderer orbit (radius 5) so they regularly enter
// detection range (4.0) as it sweeps past at 7.5 m/s.
static const Dia::Maths::Vector2D kGuardWaypoints[3][3] = {
    { {-3.f,  0.f}, {-3.f,  3.f}, {-1.f,  1.f} },
    { { 3.f,  0.f}, { 3.f,  3.f}, { 1.f,  1.f} },
    { { 0.f,  3.f}, { 1.f, -2.f}, {-1.f, -2.f} },
};

static const Dia::Maths::Vector2D kGuardStartPos[3] = {
    {-3.f,  0.f},
    { 3.f,  0.f},
    { 0.f,  3.f},
};

// ---------------------------------------------------------------------------

const Dia::Core::StringCRC BehaviourTreeTestStageModule::kTypeId("BehaviourTreeTestStageModule");

BehaviourTreeTestStageModule::BehaviourTreeTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Dia::Core::StringCRC BehaviourTreeTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("BehaviourTreeTestStage");
}

const Dia::Core::StringCRC* BehaviourTreeTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("bt.patrol_started"),
        Dia::Core::StringCRC("bt.chase_triggered"),
        Dia::Core::StringCRC("bt.target_lost"),
        Dia::Core::StringCRC("bt.parallel_fired"),
        Dia::Core::StringCRC("bt.multi_state_divergence"),
    };
    outCount = 5;
    return names;
}

// ---------------------------------------------------------------------------
// GuardMoveOrder
// ---------------------------------------------------------------------------
Dia::Core::StringCRC GuardMoveOrder::GetOrderId() const
{
    return Dia::Core::StringCRC("guard.move");
}

void GuardMoveOrder::Start(GuardOrderContext& /*ctx*/) {}

bool GuardMoveOrder::Update(GuardOrderContext& ctx, float dt)
{
    auto* guard = ctx.guard;
    float dist = guard->position.DistanceTo(target);
    if (dist < 0.3f)
        return true;
    Dia::Maths::Vector2D dir = target - guard->position;
    dir.Normalize();
    guard->position += dir * (speed * dt);
    return false;
}

void GuardMoveOrder::Finish(GuardOrderContext& ctx)
{
    ctx.guard->orderInFlight      = false;
    ctx.guard->orderJustCompleted = true;
}

void GuardMoveOrder::Cancel(GuardOrderContext& ctx)
{
    ctx.guard->orderInFlight = false;
}

// ---------------------------------------------------------------------------
// GuardOrderObserver
// ---------------------------------------------------------------------------
void GuardOrderObserver::OnQueueEmpty()
{
    // orderInFlight reset happens in Finish/Cancel
}

// ---------------------------------------------------------------------------
// Static action callbacks
// ---------------------------------------------------------------------------
Dia::BehaviourTree::NodeResult BehaviourTreeTestStageModule::ActMoveToWaypoint(
    void* ctx,
    const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& /*params*/)
{
    auto* guard = static_cast<GuardAgent*>(ctx);
    // Detect order just completed (Finish called this frame before BT tick).
    if (guard->orderJustCompleted)
    {
        guard->orderJustCompleted = false;
        guard->waypointIndex++;
        ++guard->modulePtr->mTotalPatrolWaypoints;
        ++guard->modulePtr->mDecoratorCycles;
        return Dia::BehaviourTree::NodeResult::kSuccess;
    }
    if (!guard->orderInFlight)
    {
        unsigned int wpIdx = guard->waypointIndex % 3u;
        guard->ownMoveOrder->target = guard->waypoints[wpIdx];
        guard->ownMoveOrder->speed  = 2.5f;
        guard->orderInFlight = true;
        guard->orderQueue.Enqueue(guard->ownMoveOrder);
    }
    return Dia::BehaviourTree::NodeResult::kRunning;
}

Dia::BehaviourTree::NodeResult BehaviourTreeTestStageModule::ActMoveToTarget(
    void* ctx,
    const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& /*params*/)
{
    auto* guard = static_cast<GuardAgent*>(ctx);
    // Detect order just completed (guard reached last known target position).
    if (guard->orderJustCompleted)
    {
        guard->orderJustCompleted = false;
        return Dia::BehaviourTree::NodeResult::kSuccess;
    }
    if (!guard->orderInFlight)
    {
        guard->ownMoveOrder->target = guard->chaseTarget;
        guard->ownMoveOrder->speed  = 3.5f;
        guard->orderInFlight = true;
        guard->orderQueue.Enqueue(guard->ownMoveOrder);
    }
    return Dia::BehaviourTree::NodeResult::kRunning;
}

Dia::BehaviourTree::NodeResult BehaviourTreeTestStageModule::ActSetAlertEffect(
    void* ctx,
    const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& /*params*/)
{
    auto* guard = static_cast<GuardAgent*>(ctx);
    guard->modulePtr->mCheckParallelFired = true;
    return Dia::BehaviourTree::NodeResult::kSuccess;
}

Dia::BehaviourTree::NodeResult BehaviourTreeTestStageModule::ActSetHasTarget(
    void* ctx,
    const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& /*params*/)
{
    auto* guard = static_cast<GuardAgent*>(ctx);
    guard->blackboard.Get<bool>(Dia::Core::StringCRC("has_target")) = true;
    return Dia::BehaviourTree::NodeResult::kSuccess;
}

// ---------------------------------------------------------------------------
// OnStart
// ---------------------------------------------------------------------------
void BehaviourTreeTestStageModule::OnStart(Dia::Automation::AutomationService* /*service*/)
{
    // Load shared BT asset from inline JSON
    {
        Json::Value root;
        Json::Reader().parse(kGuardBtJson, root);
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        mSharedBTAsset = Dia::BehaviourTree::BehaviourTreeAsset::LoadFromJson(root, errors);
        DIA_ASSERT(mSharedBTAsset.IsValid(), "BehaviourTreeTestStageModule: BT asset invalid");
    }

    // Register action callbacks
    mSharedActionRegistry.Register(Dia::Core::StringCRC("move_to_waypoint"), ActMoveToWaypoint);
    mSharedActionRegistry.Register(Dia::Core::StringCRC("move_to_target"),   ActMoveToTarget);
    mSharedActionRegistry.Register(Dia::Core::StringCRC("set_alert_effect"), ActSetAlertEffect);
    mSharedActionRegistry.Register(Dia::Core::StringCRC("set_has_target"),   ActSetHasTarget);

    // Init per-guard state
    for (unsigned int i = 0; i < kGuardCount; ++i)
    {
        GuardAgent& guard      = mGuards[i];
        guard.position         = kGuardStartPos[i];
        guard.waypointIndex    = 0;
        guard.waypoints[0]     = kGuardWaypoints[i][0];
        guard.waypoints[1]     = kGuardWaypoints[i][1];
        guard.waypoints[2]     = kGuardWaypoints[i][2];
        guard.chaseTarget      = {0.f, 0.f};
        guard.ownMoveOrder        = &mGuardMoveOrders[i];
        guard.orderInFlight       = false;
        guard.orderJustCompleted  = false;
        guard.targetLostFrames    = 0;
        guard.modulePtr        = this;

        guard.blackboard.Register<bool>(Dia::Core::StringCRC("has_target"))     = false;
        guard.blackboard.Register<bool>(Dia::Core::StringCRC("target_visible")) = false;

        guard.btComponent.SetAsset(&mSharedBTAsset);
        guard.btComponent.SetBlackboard(&guard.blackboard);
        guard.btComponent.SetActionRegistry(&mSharedActionRegistry);
        guard.btComponent.SetActionContext(&guard);

        mGuardObservers[i].guard     = &guard;
        mGuardObservers[i].modulePtr = this;
        guard.orderQueue.AddObserver(mGuardObservers[i]);
    }

    // Wanderer — starts at (kWandererOrbitRadius, 0), near Guard 1 at (3, 0)
    mWandererAngle = 0.0f;
    mWandererPos   = Dia::Maths::Vector2D(kWandererOrbitRadius, 0.f);

    // Checkpoint flags
    mCheckPatrolStarted        = false;
    mCheckChaseTriggered       = false;
    mCheckTargetLost           = false;
    mCheckParallelFired        = false;
    mCheckMultiStateDivergence = false;
    mTotalChases               = 0;
    mTotalPatrolWaypoints      = 0;
    mDecoratorCycles           = 0;
    mFrameCount                = 0;

    RegisterCheckpoints();

    // Metrics
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    mMetricTotalChases     = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.bt.total_chases"));
    mMetricPatrolWaypoints = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.bt.patrol_waypoints"));
    mMetricDecoratorCycles = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.bt.decorator_cycles"));
    mMetricTotalFrames     = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.bt.total_frames"));

#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
        vd->GetLayerManager().Register(&mBtDebugLayer, 10);

    for (unsigned int i = 0; i < kGuardCount; ++i)
        mGuards[i].btComponent.AddEventListener(&mNodeTrackers[i]);

    mBtDebugDomain = std::make_unique<Dia::BehaviourTree::BehaviourTreeVisualDebugger>(mGuards[0].btComponent);
    mGuards[0].btComponent.AddEventListener(mBtDebugDomain.get());
    mDomainRegistered = false;
#endif

    DIA_LOG_INFO("CluicheTest", "BehaviourTreeTestStageModule::OnStart — 3 guards initialised");
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------
void BehaviourTreeTestStageModule::OnUpdate(float deltaTime)
{
    // Advance wanderer (deterministic circular orbit)
    mWandererAngle += kWandererAngularSpeed * deltaTime;
    mWandererPos.x = std::cos(mWandererAngle) * kWandererOrbitRadius;
    mWandererPos.y = std::sin(mWandererAngle) * kWandererOrbitRadius;

    for (unsigned int i = 0; i < kGuardCount; ++i)
    {
        GuardAgent& guard = mGuards[i];
        guard.chaseTarget = mWandererPos;

        // Update blackboard bool slots
        float dist = guard.position.DistanceTo(mWandererPos);
        bool visible = (dist < kDetectionRange);
        guard.blackboard.Get<bool>(Dia::Core::StringCRC("target_visible")) = visible;

        // Reset has_target after kTargetLostThreshold frames without visibility
        bool* hasTarget = guard.blackboard.TryGet<bool>(Dia::Core::StringCRC("has_target"));
        if (hasTarget && *hasTarget && !visible)
        {
            guard.targetLostFrames++;
            if (guard.targetLostFrames > kTargetLostThreshold)
            {
                *hasTarget = false;
                guard.targetLostFrames = 0;
                guard.orderInFlight      = false;
                guard.orderJustCompleted = false;
                guard.orderQueue.Cancel();
                if (!mCheckTargetLost)
                {
                    mCheckTargetLost = true;
                    DIA_LOG_INFO("CluicheTest", "BT: target lost — guard %u returned to patrol", i);
                }
            }
        }
        else
        {
            guard.targetLostFrames = 0;
        }

        // Advance order queue then tick BT (reset on completion so the tree loops)
        {
            GuardOrderContext ctx{ &guard };
            guard.orderQueue.Update(ctx, deltaTime);
        }
        guard.btComponent.Tick(deltaTime);
        if (guard.btComponent.IsComplete())
            guard.btComponent.Reset();

        // Latch chase_triggered
        if (hasTarget && *hasTarget && !mCheckChaseTriggered)
        {
            mCheckChaseTriggered = true;
            ++mTotalChases;
            DIA_LOG_INFO("CluicheTest", "BT: first chase triggered — guard %u", i);
        }
    }

    // Latch patrol_started on first frame
    if (!mCheckPatrolStarted)
    {
        mCheckPatrolStarted = true;
        DIA_LOG_INFO("CluicheTest", "BT: patrol started");
    }

    // Latch multi_state_divergence when guards are in different branches
    if (!mCheckMultiStateDivergence)
    {
        auto getBranch = [this](unsigned int idx) -> int {
            bool* ht = mGuards[idx].blackboard.TryGet<bool>(Dia::Core::StringCRC("has_target"));
            bool* tv = mGuards[idx].blackboard.TryGet<bool>(Dia::Core::StringCRC("target_visible"));
            if (ht && *ht) return 2;   // chase
            if (tv && *tv) return 1;   // alert
            return 0;                  // patrol
        };
        if (getBranch(0) != getBranch(1) || getBranch(1) != getBranch(2))
        {
            mCheckMultiStateDivergence = true;
            DIA_LOG_INFO("CluicheTest", "BT: multi-state divergence — guards in different branches");
        }
    }

    // Update metrics
    ++mFrameCount;
    if (mMetricTotalChases)     mMetricTotalChases->Set(static_cast<double>(mTotalChases));
    if (mMetricPatrolWaypoints) mMetricPatrolWaypoints->Set(static_cast<double>(mTotalPatrolWaypoints));
    if (mMetricDecoratorCycles) mMetricDecoratorCycles->Set(static_cast<double>(mDecoratorCycles));
    if (mMetricTotalFrames)     mMetricTotalFrames->Set(static_cast<double>(mFrameCount));

#ifdef DIA_DEBUG
    if (!mDomainRegistered && mBtDebugDomain)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
        {
            vd->RegisterDomain(*mBtDebugDomain);
            mDomainRegistered = true;
        }
    }
#endif
}

// ---------------------------------------------------------------------------
// OnStop
// ---------------------------------------------------------------------------
void BehaviourTreeTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
        vd->GetLayerManager().Unregister(Dia::Core::StringCRC("CluicheTest.BehaviourTree"));

    if (mDomainRegistered && mBtDebugDomain)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->UnregisterDomain(*mBtDebugDomain);
        mDomainRegistered = false;
    }
    for (unsigned int i = 0; i < kGuardCount; ++i)
        mGuards[i].btComponent.RemoveEventListener(&mNodeTrackers[i]);

    if (mBtDebugDomain)
    {
        mGuards[0].btComponent.RemoveEventListener(mBtDebugDomain.get());
        mBtDebugDomain.reset();
    }
#endif

    if (auto* service = GetAutomationService())
        service->UnregisterCheckpoints(this);

    for (unsigned int i = 0; i < kGuardCount; ++i)
    {
        mGuards[i].orderQueue.Cancel();
        mGuards[i].btComponent.Reset();
        mGuards[i].orderQueue.RemoveObserver(mGuardObservers[i]);
        mGuards[i].blackboard.Unregister(Dia::Core::StringCRC("has_target"));
        mGuards[i].blackboard.Unregister(Dia::Core::StringCRC("target_visible"));
    }

    DIA_LOG_INFO("CluicheTest", "BehaviourTreeTestStageModule::OnStop");
}

// ---------------------------------------------------------------------------
// RegisterCheckpoints
// ---------------------------------------------------------------------------
void BehaviourTreeTestStageModule::RegisterCheckpoints()
{
    auto* service = GetAutomationService();
    if (!service) return;

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("bt.patrol_started"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckPatrolStarted, mCheckPatrolStarted ? "patrol started" : "not yet", 0.f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("bt.chase_triggered"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckChaseTriggered, mCheckChaseTriggered ? "chase triggered" : "no chase yet", 0.f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("bt.target_lost"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckTargetLost, mCheckTargetLost ? "target lost" : "still tracking", 0.f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("bt.parallel_fired"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckParallelFired, mCheckParallelFired ? "parallel fired" : "no parallel yet", 0.f };
        });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("bt.multi_state_divergence"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckMultiStateDivergence, mCheckMultiStateDivergence ? "divergence observed" : "all same state", 0.f };
        });
}

// ---------------------------------------------------------------------------
// BTDebugLayer::Draw
// ---------------------------------------------------------------------------
#ifdef DIA_DEBUG
void BehaviourTreeTestStageModule::BTDebugLayer::Draw(Dia::Core::IDebugDraw& draw)
{
    using RGBA = Dia::Core::RGBA;
    using V2   = Dia::Maths::Vector2D;

    // Default camera zoom=1 maps 1 world unit to 1 pixel.
    // Scale world-unit positions to screen pixels so the scene is legible.
    static constexpr float kS = 50.0f;

    auto s = [](const V2& v) { return V2(v.x * kS, v.y * kS); };

    // Wanderer — blue
    draw.RequestDraw(s(mModule->mWandererPos), 12.0f,
        RGBA(255, 255, 255, 255),
        RGBA(50, 150, 255, 220));

    for (unsigned int i = 0; i < kGuardCount; ++i)
    {
        const GuardAgent& guard = mModule->mGuards[i];

        bool hasTarget     = false;
        bool targetVisible = false;
        if (const bool* ht = guard.blackboard.TryGet<bool>(Dia::Core::StringCRC("has_target")))
            hasTarget = *ht;
        if (const bool* tv = guard.blackboard.TryGet<bool>(Dia::Core::StringCRC("target_visible")))
            targetVisible = *tv;

        // Colour by state: chase=red, alert=yellow, patrol=green
        RGBA fill = hasTarget        ? RGBA(220,  50,  50, 200)
                  : targetVisible    ? RGBA(220, 200,  50, 200)
                  :                    RGBA(100, 180, 100, 200);

        const V2 gPos = s(guard.position);

        // Guard body
        draw.RequestDraw(gPos, 15.0f, RGBA(255, 255, 255, 255), fill);

        // Active node label
        const char* nodeName = mModule->mNodeTrackers[i].lastEntered.AsChar();
        if (nodeName && *nodeName)
            draw.RequestDrawText(V2(gPos.x - 40.0f, gPos.y + 20.0f), nodeName, 10.0f, RGBA(255, 255, 200, 220));

        // Detection range ring
        draw.RequestDraw(gPos, kDetectionRange * kS, RGBA(200, 200, 200, 80));

        // Line + dot to current waypoint when patrolling
        if (!hasTarget && !targetVisible)
        {
            unsigned int wpIdx = guard.waypointIndex % 3u;
            const V2 wp = s(guard.waypoints[wpIdx]);
            draw.RequestDraw(gPos, wp, RGBA(150, 150, 255, 140));
            draw.RequestDraw(wp, 5.0f, RGBA(150, 150, 255, 200));
        }

        // Line to wanderer when chasing
        if (hasTarget)
            draw.RequestDraw(gPos, s(mModule->mWandererPos), RGBA(220, 50, 50, 180));
    }
}
#endif

} // namespace CluicheTest

namespace { using BehaviourTreeTestStageModule_ = CluicheTest::BehaviourTreeTestStageModule; }
DIA_MODULE(BehaviourTreeTestStageModule_);
