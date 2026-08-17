#include "Modules/TestStages/GridVisibilityTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaGridVisibility/VisibilityState.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaCore/Json/external/json/json.h>
#include <cmath>

namespace CluicheTest {

// ---------------------------------------------------------------------------
// Static data — patrol waypoints in grid-space (no origin offset).
// Cell (col, row) centre = col*30 + 15, row*30 + 15.
// ---------------------------------------------------------------------------

// Red patrol: (1,1) → (9,1) → (9,16) → (1,16) → back
static const Dia::Maths::Vector2D kRedWaypoints[4] = {
    {  1*30+15.0f,  1*30+15.0f },
    {  9*30+15.0f,  1*30+15.0f },
    {  9*30+15.0f, 16*30+15.0f },
    {  1*30+15.0f, 16*30+15.0f },
};

// Blue patrol: (14,1) → (22,1) → (22,16) → (14,16) → back
static const Dia::Maths::Vector2D kBlueWaypoints[4] = {
    { 14*30+15.0f,  1*30+15.0f },
    { 22*30+15.0f,  1*30+15.0f },
    { 22*30+15.0f, 16*30+15.0f },
    { 14*30+15.0f, 16*30+15.0f },
};

// Enemy start positions: E0(5,6), E1(8,14), E2(11,9), E3(15,3), E4(19,12)
static const Dia::Maths::Vector2D kEnemyPositions[5] = {
    {  5*30+15.0f,  6*30+15.0f },
    {  8*30+15.0f, 14*30+15.0f },
    { 11*30+15.0f,  9*30+15.0f },
    { 15*30+15.0f,  3*30+15.0f },
    { 19*30+15.0f, 12*30+15.0f },
};

// One table per patrol index → its waypoint array
static const Dia::Maths::Vector2D* const kPatrolWaypoints[2] = {
    kRedWaypoints,
    kBlueWaypoints,
};

// World-space origin of the grid's top-left corner (matches drawer)
static const Dia::Maths::Vector2D kGridOrigin(-360.0f, -270.0f);

// Visibility group IDs
static const Dia::GridVisibility::VisibilityGroupId kRedGroup("Red");
static const Dia::GridVisibility::VisibilityGroupId kBlueGroup("Blue");
static const Dia::GridVisibility::VisibilityGroupId kGroupById[2] = { kRedGroup, kBlueGroup };

// ---------------------------------------------------------------------------
// Static type ID
// ---------------------------------------------------------------------------

const Dia::Core::StringCRC GridVisibilityTestStageModule::kTypeId("GridVisibilityTestStageModule");

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

GridVisibilityTestStageModule::GridVisibilityTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
    , mGrid(kGridWidth, kGridHeight)
    , mStageTime(0.0f)
{
    for (int i = 0; i < kPatrolCount; ++i)
    {
        mPatrolPos[i]      = Dia::Maths::Vector2D(0.0f, 0.0f);
        mPatrolWpIdx[i]    = 1;
        mPatrolPassedWp1[i] = false;
        mLoopCount[i]      = 0;
    }
    for (int i = 0; i < 7; ++i)
        mEntityWorldPos[i] = Dia::Maths::Vector2D(0.0f, 0.0f);
    for (int i = 0; i < kEnemyCount; ++i)
        mEnemyFlashTimers[i] = 0.0f;
}

GridVisibilityTestStageModule::~GridVisibilityTestStageModule() = default;

// ---------------------------------------------------------------------------
// TestStageModuleBase overrides — identity
// ---------------------------------------------------------------------------

Dia::Core::StringCRC GridVisibilityTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("GridVisibilityTestStage");
}

const Dia::Core::StringCRC* GridVisibilityTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("gridvis.red_loop_complete"),
        Dia::Core::StringCRC("gridvis.blue_loop_complete"),
        Dia::Core::StringCRC("gridvis.both_loops_complete"),
    };
    outCount = 3;
    return names;
}

// ---------------------------------------------------------------------------
// RegisterCheckpoints
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::RegisterCheckpoints(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("gridvis.red_loop_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRedLoopComplete,
                     mRedLoopComplete ? "Red patrol completed first lap" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("gridvis.blue_loop_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mBlueLoopComplete,
                     mBlueLoopComplete ? "Blue patrol completed first lap" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("gridvis.both_loops_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mBothLoopsComplete,
                     mBothLoopsComplete ? "Both patrols completed first lap" : "pending", 0.0f };
        });
}

// ---------------------------------------------------------------------------
// RegisterMetrics
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::RegisterMetrics()
{
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricRedVisible)
        mMetricRedVisible = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.gridvis.red_cells_visible"));
    if (!mMetricBlueVisible)
        mMetricBlueVisible = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.gridvis.blue_cells_visible"));
    if (!mMetricEnemiesSpottedByRed)
        mMetricEnemiesSpottedByRed = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.gridvis.enemies_spotted_by_red"));
    if (!mMetricEnemiesSpottedByBlue)
        mMetricEnemiesSpottedByBlue = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.gridvis.enemies_spotted_by_blue"));
}

// ---------------------------------------------------------------------------
// BuildGrid — apply wall mask to the SquarePathGrid
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::BuildGrid()
{
    // Centre divider: cols 11-12, rows 0-6 AND rows 11-17
    for (int col = 11; col <= 12; ++col)
    {
        for (int row = 0; row <= 6; ++row)
            mGrid.SetPassable({col, row}, false);
        for (int row = 11; row <= 17; ++row)
            mGrid.SetPassable({col, row}, false);
    }

    // L-wall left-A: horizontal span cols 3-5 at row 4, plus vertical col 5 rows 4-7
    for (int col = 3; col <= 5; ++col)
        mGrid.SetPassable({col, 4}, false);
    for (int row = 4; row <= 7; ++row)
        mGrid.SetPassable({5, row}, false);

    // L-wall left-B: cols 2-4 at row 13
    for (int col = 2; col <= 4; ++col)
        mGrid.SetPassable({col, 13}, false);

    // L-wall right-A: horizontal span cols 18-20 at row 4, plus vertical col 18 rows 4-7
    for (int col = 18; col <= 20; ++col)
        mGrid.SetPassable({col, 4}, false);
    for (int row = 4; row <= 7; ++row)
        mGrid.SetPassable({18, row}, false);

    // L-wall right-B: cols 19-21 at row 13
    for (int col = 19; col <= 21; ++col)
        mGrid.SetPassable({col, 13}, false);
}

// ---------------------------------------------------------------------------
// SpawnEntities — create domain entities and queue their SpatialComponents
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::SpawnEntities()
{
    // Patrol entities: Red (group 0), Blue (group 1)
    for (int i = 0; i < kPatrolCount; ++i)
    {
        const Dia::Maths::Vector2D& startPos = kPatrolWaypoints[i][0];
        mPatrolPos[i]      = startPos;
        mEntityWorldPos[i] = Dia::Maths::Vector2D(startPos.x + kGridOriginX, startPos.y + kGridOriginY);
        mPatrolWpIdx[i]    = 1;
        mPatrolPassedWp1[i] = false;
        mLoopCount[i]      = 0;

        mPatrolEntities[i] = mDomain.CreateEntity();

        Json::Value cfg;
        cfg["position"]["x"] = startPos.x;
        cfg["position"]["y"] = startPos.y;
        cfg["radius"]        = 8.0f;
        cfg["layerMask"]     = 0x01;
        mDomain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(mPatrolEntities[i], cfg);

        mVisSystem->RegisterSightSource(mPatrolEntities[i], kGroupById[i], kSightRadiusWorld);
    }

    // Enemy entities: E0..E4 (static; not sight sources)
    for (int i = 0; i < kEnemyCount; ++i)
    {
        const Dia::Maths::Vector2D& pos = kEnemyPositions[i];
        mEntityWorldPos[2 + i] = Dia::Maths::Vector2D(pos.x + kGridOriginX, pos.y + kGridOriginY);
        mEnemyFlashTimers[i]   = 0.0f;

        mEnemyEntities[i] = mDomain.CreateEntity();

        Json::Value cfg;
        cfg["position"]["x"] = pos.x;
        cfg["position"]["y"] = pos.y;
        cfg["radius"]        = 8.0f;
        cfg["layerMask"]     = 0x02;
        mDomain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(mEnemyEntities[i], cfg);
    }
}

// ---------------------------------------------------------------------------
// OnStart
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // Reset all mutable state so re-entry is clean
    mStageTime          = 0.0f;
    mRedLoopComplete    = false;
    mBlueLoopComplete   = false;
    mBothLoopsComplete  = false;
    for (int i = 0; i < kEnemyCount; ++i)
        mEnemyFlashTimers[i] = 0.0f;

    // 1. Register SpatialComponent storage with the entity domain
    mDomain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpatial::SpatialComponent>(
        Dia::EntitySpatial::SpatialComponent::kTypeId));

    // 2. Construct spatial index covering the full grid in grid-space [0, W*30] x [0, H*30]
    Dia::EntitySpatial::EntitySpatialIndex::SquareDef def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(0.0f, 0.0f),
        Dia::Maths::Vector2D(static_cast<float>(kGridWidth)  * kCellSize,
                             static_cast<float>(kGridHeight) * kCellSize));
    def.cellSize = kCellSize;
    mSpatialModule = std::make_unique<Dia::EntitySpatial::EntitySpatialModule>(mDomain, def);

    // 3. Stamp wall mask onto the pathfinding grid
    BuildGrid();

    // 4. Construct visibility system (chunkSize=1 → vis-cell == path-cell)
    mVisSystem = std::make_unique<
        Dia::GridVisibility::GridVisibilitySystem<Dia::Pathfinding::SquarePathGrid>>(mGrid);
    mVisSystem->AddChangeListener(this);

    // 5. Spawn patrol and enemy entities
    SpawnEntities();

    // 6. Commit queued component additions and build initial spatial index
    mDomain.EndOfFrame();
    mSpatialModule->Update();

    // 7. Initial visibility pass
    mVisSystem->Update(kCellSize, *mSpatialModule, mDomain);

    // 8. Register automation checkpoints and observation metrics
    RegisterCheckpoints(service);
    RegisterMetrics();

    DIA_LOG_INFO("CluicheTest",
                 "GridVisibilityTestStageModule::OnStart — 2 patrol sight-sources, 5 enemies, 24x18 grid");
}

// ---------------------------------------------------------------------------
// UpdatePatrols
//
// Each patrol moves toward its current target waypoint at kPatrolSpeed.
// Loop counting: mLoopCount[i] increments when the patrol re-enters wp0
// after having passed wp1 (i.e., after completing a full circuit).
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::UpdatePatrols(float dt)
{
    for (int i = 0; i < kPatrolCount; ++i)
    {
        const Dia::Maths::Vector2D& target = kPatrolWaypoints[i][mPatrolWpIdx[i]];

        const float dx  = target.x - mPatrolPos[i].x;
        const float dy  = target.y - mPatrolPos[i].y;
        const float len = std::sqrtf(dx * dx + dy * dy);

        if (len <= kArrivalThreshold)
        {
            // Snap to waypoint to avoid accumulated float drift
            mPatrolPos[i] = target;

            const int arrivedIdx = mPatrolWpIdx[i];
            mPatrolWpIdx[i] = (arrivedIdx + 1) % kWaypointCount;

            // Leaving wp1 → mark that this lap has cleared the first waypoint gate
            if (arrivedIdx == 1)
                mPatrolPassedWp1[i] = true;

            // Arriving at wp0 after having cleared wp1 = one full loop completed
            if (arrivedIdx == 0 && mPatrolPassedWp1[i])
            {
                ++mLoopCount[i];
                mPatrolPassedWp1[i] = false;
            }
        }
        else
        {
            const float move = kPatrolSpeed * dt;
            if (move >= len)
                mPatrolPos[i] = target;
            else
            {
                mPatrolPos[i].x += (dx / len) * move;
                mPatrolPos[i].y += (dy / len) * move;
            }
        }

        mEntityWorldPos[i] = Dia::Maths::Vector2D(mPatrolPos[i].x + kGridOriginX, mPatrolPos[i].y + kGridOriginY);
    }
}

// ---------------------------------------------------------------------------
// UpdateSpatialPositions — flush new patrol positions into the domain
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::UpdateSpatialPositions()
{
    for (int i = 0; i < kPatrolCount; ++i)
    {
        auto* sc = mDomain.GetComponent<Dia::EntitySpatial::SpatialComponent>(mPatrolEntities[i]);
        if (sc)
        {
            sc->position = mPatrolPos[i];
            sc->MarkDirty();
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateFlashTimers — decay enemy flash timers each frame
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::UpdateFlashTimers(float dt)
{
    for (int i = 0; i < kEnemyCount; ++i)
    {
        if (mEnemyFlashTimers[i] > 0.0f)
        {
            mEnemyFlashTimers[i] -= dt;
            if (mEnemyFlashTimers[i] < 0.0f)
                mEnemyFlashTimers[i] = 0.0f;
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateMetrics
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::UpdateMetrics()
{
    // Count Visible cells for each group
    int redVisible  = 0;
    int blueVisible = 0;
    const int w = mVisSystem->GetVisWidth();
    const int h = mVisSystem->GetVisHeight();
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const Dia::Pathfinding::CellCoord cell{ x, y };
            if (mVisSystem->GetCellState(cell, kRedGroup)  == Dia::GridVisibility::VisibilityState::Visible)
                ++redVisible;
            if (mVisSystem->GetCellState(cell, kBlueGroup) == Dia::GridVisibility::VisibilityState::Visible)
                ++blueVisible;
        }
    }

    // Count enemy entities currently visible to each group
    int spottedByRed  = 0;
    int spottedByBlue = 0;
    for (int i = 0; i < kEnemyCount; ++i)
    {
        if (mVisSystem->CanSee(mEnemyEntities[i], kRedGroup))  ++spottedByRed;
        if (mVisSystem->CanSee(mEnemyEntities[i], kBlueGroup)) ++spottedByBlue;
    }

    if (mMetricRedVisible)           mMetricRedVisible->Set(static_cast<double>(redVisible));
    if (mMetricBlueVisible)          mMetricBlueVisible->Set(static_cast<double>(blueVisible));
    if (mMetricEnemiesSpottedByRed)  mMetricEnemiesSpottedByRed->Set(static_cast<double>(spottedByRed));
    if (mMetricEnemiesSpottedByBlue) mMetricEnemiesSpottedByBlue->Set(static_cast<double>(spottedByBlue));
}

// ---------------------------------------------------------------------------
// CheckCheckpoints
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::CheckCheckpoints()
{
    if (!mRedLoopComplete && mLoopCount[0] >= 1)
        mRedLoopComplete = true;

    if (!mBlueLoopComplete && mLoopCount[1] >= 1)
        mBlueLoopComplete = true;

    if (!mBothLoopsComplete && mRedLoopComplete && mBlueLoopComplete)
        mBothLoopsComplete = true;
}

// ---------------------------------------------------------------------------
// IVisibilityChangeObserver::OnEntityVisibilityChanged
//
// On first detection of an enemy entity, reset its flash timer to 0.6 s.
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::OnEntityVisibilityChanged(
    Dia::Entity::Entity entity,
    Dia::GridVisibility::VisibilityGroupId /*groupId*/,
    bool isNowVisible)
{
    if (!isNowVisible)
        return;

    for (int i = 0; i < kEnemyCount; ++i)
    {
        if (mEnemyEntities[i] == entity)
        {
            mEnemyFlashTimers[i] = 0.6f;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::OnUpdate(float deltaTime)
{
    mStageTime += deltaTime;

    UpdateFlashTimers(deltaTime);
    UpdatePatrols(deltaTime);
    UpdateSpatialPositions();

    mDomain.EndOfFrame();
    mSpatialModule->Update();
    mVisSystem->Update(kCellSize, *mSpatialModule, mDomain);

    UpdateMetrics();
    CheckCheckpoints();

#ifdef DIA_DEBUG
    if (!mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
        {
            const Dia::Core::StringCRC stageTag("GridVisibility");
            mDrawer = std::make_unique<GridVisibilityDrawer>(
                *mVisSystem, mGrid, kCellSize, kGridOrigin,
                mEntityWorldPos, mEnemyFlashTimers, mStageTime,
                vd->GetLayerManager());
            vd->GetLayerManager().Register(mDrawer.get(), 20, stageTag);
        }
    }
#endif

    if (!IsResolved() && mBothLoopsComplete && GetFrameCount() >= kMinDisplayFrames)
        ReportPassed();
}

// ---------------------------------------------------------------------------
// OnStop
// ---------------------------------------------------------------------------

void GridVisibilityTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->GetLayerManager().Unregister(mDrawer->GetLayerName());
        mDrawer.reset();
    }
#endif

    if (auto* service = GetAutomationService())
        service->UnregisterCheckpoints(this);

    if (mVisSystem)
    {
        mVisSystem->RemoveChangeListener(this);
        mVisSystem.reset();
    }
    mSpatialModule.reset();

    // Reset state so a re-entry starts deterministically
    mStageTime          = 0.0f;
    mRedLoopComplete    = false;
    mBlueLoopComplete   = false;
    mBothLoopsComplete  = false;
    for (int i = 0; i < kPatrolCount; ++i)
    {
        mLoopCount[i]       = 0;
        mPatrolPassedWp1[i] = false;
        mPatrolWpIdx[i]     = 1;
    }
    for (int i = 0; i < kEnemyCount; ++i)
        mEnemyFlashTimers[i] = 0.0f;

    DIA_LOG_INFO("CluicheTest", "GridVisibilityTestStageModule::OnStop");
}

} // namespace CluicheTest

namespace { using GridVisibilityTestStageModule_ = CluicheTest::GridVisibilityTestStageModule; }
DIA_MODULE(GridVisibilityTestStageModule_);
DIA_DESCRIBE(GridVisibilityTestStageModule_::kTypeId,
    "Two-group fog-of-war demo: Red/Blue patrol sight-sources on a 24x18 grid with shadowcasting, Revealed persistence, and enemy flash on detection.");
