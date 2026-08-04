#include "Modules/TestStages/PathfindingTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaPathfinding/FindPath.h>
#include <DiaSteering/Behaviours.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <cmath>
#include <cstdio>

namespace CluicheTest {

const Dia::Core::StringCRC PathfindingTestStageModule::kTypeId("PathfindingTestStageModule");

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

PathfindingTestStageModule::PathfindingTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
    , mGrid(kGridWidth, kGridHeight, Dia::Pathfinding::SquareConnectivity::k8Connected)
    , mFlowAdapter(mGrid)
    , mFlowCache(mFlowAdapter, mCostProvider, kGridWidth, kGridHeight)
{}

PathfindingTestStageModule::~PathfindingTestStageModule() = default;

// ---------------------------------------------------------------------------
// TestStageModuleBase overrides — identity and checkpoint names
// ---------------------------------------------------------------------------

Dia::Core::StringCRC PathfindingTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("PathfindingTestStage");
}

const Dia::Core::StringCRC* PathfindingTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("pathfinding.path_computed"),
        Dia::Core::StringCRC("pathfinding.flow_field_ready"),
        Dia::Core::StringCRC("pathfinding.recomputed_after_block"),
        Dia::Core::StringCRC("pathfinding.first_agent_arrived"),
        Dia::Core::StringCRC("pathfinding.all_agents_arrived"),
    };
    outCount = 5;
    return names;
}

// ---------------------------------------------------------------------------
// OnStart — build grid, run initial path + flow field, register checkpoints
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    BuildGrid();
    ComputeInitialPath();
    ComputeInitialFlowField();
    RegisterCheckpoints(service);
    RegisterMetrics();

#ifdef DIA_DEBUG
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            mDrawer = std::make_unique<PathfindingTestDrawer>(
                mGrid, mCurrentField, mAgents,
                mTrails, mTrailHead, mAgentArrived,
                mGoalWorld,
                mPathComputed, mFlowFieldReady, mRecomputedAfterBlock,
                mFirstArrived, mAllArrived, mRecomputeCount,
                kCellSize, mStageTime,
                vd->GetLayerManager());
            vd->GetLayerManager().Register(mDrawer.get(), 20, Dia::Core::StringCRC("PathfindingTest"));
        }
    }
#endif

    DIA_LOG_INFO("CluicheTest", "PathfindingTestStageModule::OnStart — "
                 "path_computed=%d flow_field_ready=%d",
                 (int)mPathComputed, (int)mFlowFieldReady);
}

// ---------------------------------------------------------------------------
// BuildGrid — 20x15, set static obstacles, place agents
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::BuildGrid()
{
    // Reset all cells to passable before applying static obstacles (required for clean re-entry).
    for (int col = 0; col < kGridWidth; ++col)
        for (int row = 0; row < kGridHeight; ++row)
            mGrid.SetPassable({ col, row }, true);

    // L-wall: vertical col 3, rows 2-7
    for (int row = 2; row <= 7; ++row)
        mGrid.SetPassable({ 3, row }, false);

    // L-wall: horizontal row 7, cols 3-7
    for (int col = 3; col <= 7; ++col)
        mGrid.SetPassable({ col, 7 }, false);

    // Diagonal barrier
    mGrid.SetPassable({ 10, 3 }, false);
    mGrid.SetPassable({ 11, 4 }, false);
    mGrid.SetPassable({ 12, 5 }, false);
    mGrid.SetPassable({ 13, 6 }, false);

    // Central block: cols 8-10, rows 6-8
    for (int col = 8; col <= 10; ++col)
        for (int row = 6; row <= 8; ++row)
            mGrid.SetPassable({ col, row }, false);

    // Agent starts (cell coords) → world positions (cell-centre)
    // Agent0=(1,1), Agent1=(1,13), Agent2=(18,13)
    mAgents[0].position = Dia::Maths::Vector2D(1 * kCellSize + kCellSize * 0.5f, 1  * kCellSize + kCellSize * 0.5f);
    mAgents[1].position = Dia::Maths::Vector2D(1 * kCellSize + kCellSize * 0.5f, 13 * kCellSize + kCellSize * 0.5f);
    mAgents[2].position = Dia::Maths::Vector2D(18 * kCellSize + kCellSize * 0.5f, 13 * kCellSize + kCellSize * 0.5f);
    for (int i = 0; i < kAgentCount; ++i)
    {
        mAgents[i].velocity  = Dia::Maths::Vector2D(0.0f, 0.0f);
        mAgents[i].maxSpeed  = 80.0f;
        mAgents[i].maxForce  = 200.0f;
    }

    // Goal world position: cell (16,7)
    mGoalWorld = Dia::Maths::Vector2D(16 * kCellSize + kCellSize * 0.5f, 7 * kCellSize + kCellSize * 0.5f);
}

// ---------------------------------------------------------------------------
// ComputeInitialPath — A* from agent-0 start cell to goal cell
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::ComputeInitialPath()
{
    const Dia::Pathfinding::CellCoord from{ 1,  1 };
    const Dia::Pathfinding::CellCoord to  { 16, 7 };

    Dia::Pathfinding::PathResult result =
        Dia::Pathfinding::FindPath<Dia::Pathfinding::SquarePathGrid>(
            mGrid, from, to, mCostProvider);

    mPathComputed = result.success;

    DIA_LOG_INFO("CluicheTest", "PathfindingTestStageModule: A* path success=%d cost=%.1f cells=%d",
                 (int)result.success, result.totalCost, result.cells.Size());
}

// ---------------------------------------------------------------------------
// ComputeInitialFlowField — BFS-based flow field to goal cell
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::ComputeInitialFlowField()
{
    const Dia::Pathfinding::CellCoord goalCell{ 16, 7 };

    mCurrentField = &mFlowCache.GetOrCompute(
        Dia::Core::StringCRC("main"), goalCell);

    // mRecomputeCount is our own counter, initialised from cache recomputes
    mRecomputeCount = mFlowCache.GetRecomputeCount();

    // Flow field is ready when we have a computed entry (recompute count >= 1)
    mFlowFieldReady = (mCurrentField != nullptr && mFlowCache.GetRecomputeCount() >= 1);

    DIA_LOG_INFO("CluicheTest", "PathfindingTestStageModule: flow field ready=%d recomputes=%d",
                 (int)mFlowFieldReady, mFlowCache.GetRecomputeCount());
}

// ---------------------------------------------------------------------------
// RegisterCheckpoints
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::RegisterCheckpoints(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("pathfinding.path_computed"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPathComputed,
                     mPathComputed ? "A* path from (1,1) to (16,7) succeeded" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("pathfinding.flow_field_ready"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mFlowFieldReady,
                     mFlowFieldReady ? "FlowField computed for goal (16,7)" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("pathfinding.recomputed_after_block"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRecomputedAfterBlock,
                     mRecomputedAfterBlock ? "Flow field recomputed after dynamic obstacle" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("pathfinding.first_agent_arrived"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mFirstArrived,
                     mFirstArrived ? "At least one agent reached goal" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("pathfinding.all_agents_arrived"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mAllArrived,
                     mAllArrived ? "All 3 agents reached goal" : "pending", 0.0f };
        });
}

// ---------------------------------------------------------------------------
// RegisterMetrics
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::RegisterMetrics()
{
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricAgent0Dist)
        mMetricAgent0Dist = reg.RegisterGauge(
            Dia::Core::StringCRC("pathfinding.agent_0_dist_to_goal"));
    if (!mMetricAgent1Dist)
        mMetricAgent1Dist = reg.RegisterGauge(
            Dia::Core::StringCRC("pathfinding.agent_1_dist_to_goal"));
    if (!mMetricAgent2Dist)
        mMetricAgent2Dist = reg.RegisterGauge(
            Dia::Core::StringCRC("pathfinding.agent_2_dist_to_goal"));
    if (!mMetricRecomputeCount)
        mMetricRecomputeCount = reg.RegisterGauge(
            Dia::Core::StringCRC("pathfinding.recompute_count"));
}

// ---------------------------------------------------------------------------
// EmitMetrics — called each frame from OnUpdate
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::EmitMetrics()
{
    Dia::Observation::Metric::Gauge* gauges[kAgentCount] = {
        mMetricAgent0Dist, mMetricAgent1Dist, mMetricAgent2Dist
    };

    for (int i = 0; i < kAgentCount; ++i)
    {
        float dx   = mAgents[i].position.X() - mGoalWorld.X();
        float dy   = mAgents[i].position.Y() - mGoalWorld.Y();
        float dist = sqrtf(dx * dx + dy * dy);
        if (gauges[i])
            gauges[i]->Set(static_cast<double>(dist));
    }

    if (mMetricRecomputeCount)
        mMetricRecomputeCount->Set(static_cast<double>(mRecomputeCount));
}

// ---------------------------------------------------------------------------
// AllCheckpointsPassed
// ---------------------------------------------------------------------------

bool PathfindingTestStageModule::AllCheckpointsPassed() const
{
    return mPathComputed && mFlowFieldReady && mRecomputedAfterBlock
        && mFirstArrived && mAllArrived;
}

// ---------------------------------------------------------------------------
// UpdateAgents — steering, integration, trail, arrival
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::UpdateAgents(float dt)
{
    bool anyArrived = false;
    bool allArrived = true;

    for (int i = 0; i < kAgentCount; ++i)
    {
        if (mAgentArrived[i])
        {
            anyArrived = true;
            continue;
        }

        // Sample flow field direction at agent world position
        // SampleWorld returns the direction vector (unit vec in grid space)
        // Use this as a "flow seek target" offset from agent position
        Dia::Maths::Vector2D flowDir = mCurrentField
            ? mCurrentField->SampleWorld(mAgents[i].position, kCellSize)
            : Dia::Maths::Vector2D(0.0f, 0.0f);

        // Blend: use Arrive near goal, flow-based Seek farther away
        float dx = mGoalWorld.X() - mAgents[i].position.X();
        float dy = mGoalWorld.Y() - mAgents[i].position.Y();
        float distToGoal = sqrtf(dx * dx + dy * dy);

        Dia::Maths::Vector2D desiredVel;
        if (distToGoal < kSlowingRadius * 2.0f)
        {
            // Close enough — use Arrive for smooth deceleration
            desiredVel = Dia::Steering::Arrive(mAgents[i], mGoalWorld, kSlowingRadius);
        }
        else
        {
            // Use flow field direction scaled to max speed
            float len = sqrtf(flowDir.X() * flowDir.X() + flowDir.Y() * flowDir.Y());
            if (len > 0.0001f)
                desiredVel = Dia::Maths::Vector2D(flowDir.X() / len * kAgentMaxSpeed,
                                                   flowDir.Y() / len * kAgentMaxSpeed);
        }

        // Framerate-independent velocity blend (exponential smoothing)
        float alpha = 1.0f - expf(-kSteeringGain * dt);
        mAgents[i].velocity = Dia::Maths::Vector2D(
            mAgents[i].velocity.X() + (desiredVel.X() - mAgents[i].velocity.X()) * alpha,
            mAgents[i].velocity.Y() + (desiredVel.Y() - mAgents[i].velocity.Y()) * alpha);

        // Integrate position
        mAgents[i].position = Dia::Maths::Vector2D(
            mAgents[i].position.X() + mAgents[i].velocity.X() * dt,
            mAgents[i].position.Y() + mAgents[i].velocity.Y() * dt);

        // Record trail
        mTrails[i][mTrailHead[i] % kTrailLen] = mAgents[i].position;
        ++mTrailHead[i];

        // Arrival check
        dx = mGoalWorld.X() - mAgents[i].position.X();
        dy = mGoalWorld.Y() - mAgents[i].position.Y();
        distToGoal = sqrtf(dx * dx + dy * dy);
        if (distToGoal < kArrivalRadius)
        {
            mAgentArrived[i] = true;
            mAgents[i].velocity = Dia::Maths::Vector2D(0.0f, 0.0f);
            anyArrived = true;
            DIA_LOG_INFO("CluicheTest", "PathfindingTestStageModule: Agent %d arrived at frame %u",
                         i, GetFrameCount());
        }

        allArrived = allArrived && mAgentArrived[i];
    }

    if (anyArrived && !mFirstArrived)
        mFirstArrived = true;
    if (allArrived && !mAllArrived)
        mAllArrived = true;
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::OnUpdate(float deltaTime)
{
    // Frame 60: insert dynamic obstacle and invalidate/recompute flow field
    if (GetFrameCount() == kDynamicBlockFrame && !mRecomputedAfterBlock)
    {
        // Add row block (6,10)–(9,10) — forces agents on the bottom path to re-route
        for (int col = 6; col <= 9; ++col)
            mGrid.SetPassable({ col, 10 }, false);
        mFlowCache.Invalidate(Dia::Core::StringCRC("main"));

        // Recompute immediately
        const Dia::Pathfinding::CellCoord goalCell{ 16, 7 };
        mCurrentField = &mFlowCache.GetOrCompute(
            Dia::Core::StringCRC("main"), goalCell);

        mRecomputeCount = mFlowCache.GetRecomputeCount();
        mRecomputedAfterBlock = (mCurrentField != nullptr && mFlowCache.GetRecomputeCount() >= 2);

        DIA_LOG_INFO("CluicheTest", "PathfindingTestStageModule: dynamic obstacle at frame %u, "
                     "recomputed=%d total_recomputes=%d",
                     GetFrameCount(), (int)mRecomputedAfterBlock, mRecomputeCount);
    }

    // Update agent steering, position integration, trail, arrival
    UpdateAgents(deltaTime);

    mStageTime += deltaTime;

    // Emit metrics each frame
    EmitMetrics();

    // Check pass condition
    if (AllCheckpointsPassed() && !mAllPassed)
    {
        mAllPassed = true;
        if (!IsResolved() && GetFrameCount() >= kMinDisplayFrames)
            ReportPassed();
    }
}

// ---------------------------------------------------------------------------
// OnStop — reset all state
// ---------------------------------------------------------------------------

void PathfindingTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->GetLayerManager().Unregister(mDrawer->GetLayerName());
        mDrawer.reset();
    }
    mStageTime = 0.0f;
#endif

    mCurrentField         = nullptr;
    mFlowCache.InvalidateAll(); // ensure re-entry recomputes from fresh grid state
    mPathComputed         = false;
    mFlowFieldReady       = false;
    mRecomputedAfterBlock = false;
    mFirstArrived         = false;
    mAllArrived           = false;
    mAllPassed            = false;
    mRecomputeCount       = 0;

    for (int i = 0; i < kAgentCount; ++i)
    {
        mAgents[i].position = Dia::Maths::Vector2D(0.0f, 0.0f);
        mAgents[i].velocity = Dia::Maths::Vector2D(0.0f, 0.0f);
    }

    for (int i = 0; i < kAgentCount; ++i)
    {
        mAgentArrived[i] = false;
        mTrailHead[i]    = 0;
        for (int t = 0; t < kTrailLen; ++t)
            mTrails[i][t] = Dia::Maths::Vector2D(0.0f, 0.0f);
    }
}

} // namespace CluicheTest

namespace { using PathfindingTestStageModule_ = CluicheTest::PathfindingTestStageModule; }
DIA_MODULE(PathfindingTestStageModule_);
DIA_DESCRIBE(PathfindingTestStageModule_::kTypeId, "Full navigation stack: A* + FlowField + Steering");
