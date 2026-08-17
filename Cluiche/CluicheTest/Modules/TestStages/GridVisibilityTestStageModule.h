#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaGridVisibility/GridVisibilitySystem.h>
#include <DiaGridVisibility/IVisibilityChangeObserver.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <memory>

#ifdef DIA_DEBUG
#include <DiaApplicationFlow/ModuleRefV2.h>
#include "Modules/VisualDebuggerModule.h"
#include "Modules/TestStages/Drawers/GridVisibilityDrawer.h"
#include <DiaGridVisibilityVisualDebugger/GridVisibilityDebugDomain.h>
#endif

namespace Dia::Observation::Metric { class Gauge; }

namespace CluicheTest {

class GridVisibilityTestStageModule
    : public TestStageModuleBase
    , public Dia::GridVisibility::IVisibilityChangeObserver
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "Two-group fog-of-war demo: Red/Blue patrol sight-sources on a 24x18 grid with shadowcasting, Revealed persistence, and enemy flash on detection.";
    static constexpr unsigned int kMinDisplayFrames = 300;

    explicit GridVisibilityTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~GridVisibilityTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 1200; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

    // IVisibilityChangeObserver — fires enemy flash timer on first detection
    void OnEntityVisibilityChanged(Dia::Entity::Entity entity,
                                   Dia::GridVisibility::VisibilityGroupId groupId,
                                   bool isNowVisible) override;

private:
    void BuildGrid();
    void SpawnEntities();
    void UpdatePatrols(float dt);
    void UpdateSpatialPositions();
    void UpdateFlashTimers(float dt);
    void UpdateMetrics();
    void CheckCheckpoints();
    void RegisterCheckpoints(Dia::Automation::AutomationService* service);
    void RegisterMetrics();

    // Grid geometry constants
    static constexpr float kCellSize         = 30.0f;
    static constexpr int   kGridWidth        = 24;
    static constexpr int   kGridHeight       = 18;
    static constexpr float kGridOriginX      = -360.0f;  // world-space left edge
    static constexpr float kGridOriginY      = -270.0f;  // world-space top edge
    static constexpr float kPatrolSpeed      = 120.0f;          // world units/sec
    static constexpr float kSightRadiusWorld = 6.0f * kCellSize; // 180 world units
    static constexpr float kArrivalThreshold = 2.0f;
    static constexpr int   kWaypointCount    = 4;
    static constexpr int   kPatrolCount      = 2;
    static constexpr int   kEnemyCount       = 5;

    // Pathfinding grid — value member; owns passability flags
    Dia::Pathfinding::SquarePathGrid mGrid;

    // Entity domain and spatial index
    Dia::Entity::Domain mDomain;
    std::unique_ptr<Dia::EntitySpatial::EntitySpatialModule> mSpatialModule;

    // Visibility system — templated on the concrete grid type
    std::unique_ptr<Dia::GridVisibility::GridVisibilitySystem<Dia::Pathfinding::SquarePathGrid>> mVisSystem;

    // Entities
    Dia::Entity::Entity mPatrolEntities[kPatrolCount];
    Dia::Entity::Entity mEnemyEntities[kEnemyCount];

    // Patrol simulation state
    Dia::Maths::Vector2D mPatrolPos[kPatrolCount];
    int  mPatrolWpIdx[kPatrolCount];      // index of the NEXT target waypoint (1..3, wraps to 0)
    bool mPatrolPassedWp1[kPatrolCount];  // true once wp1 has been visited this lap
    int  mLoopCount[kPatrolCount];        // increments each time a full lap is completed

    // Drawer-accessible state (const refs passed to GridVisibilityDrawer in Task 4).
    // [0] = Red patrol, [1] = Blue patrol, [2..6] = E0..E4
    Dia::Maths::Vector2D mEntityWorldPos[7];
    float                mEnemyFlashTimers[5]; // seconds remaining; E0..E4
    float                mStageTime;           // accumulated seconds since OnStart

    // Checkpoint flags
    bool mRedLoopComplete   = false;
    bool mBlueLoopComplete  = false;
    bool mBothLoopsComplete = false;

    // Metrics (gauges registered with MetricRegistry)
    Dia::Observation::Metric::Gauge* mMetricRedVisible           = nullptr;
    Dia::Observation::Metric::Gauge* mMetricBlueVisible          = nullptr;
    Dia::Observation::Metric::Gauge* mMetricEnemiesSpottedByRed  = nullptr;
    Dia::Observation::Metric::Gauge* mMetricEnemiesSpottedByBlue = nullptr;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<GridVisibilityDrawer> mDrawer;
    std::unique_ptr<Dia::GridVisibilityVisualDebugger::GridVisibilityDebugDomain<Dia::Pathfinding::SquarePathGrid>> mDebugDomain;
#endif
};

} // namespace CluicheTest
