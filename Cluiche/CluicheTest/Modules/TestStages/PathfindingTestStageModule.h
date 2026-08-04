#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/PathResult.h>
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/FlowFieldCache.h>
#include <DiaFlowField/FlowField.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/TestStages/Drawers/PathfindingTestDrawer.h"
#endif

namespace Dia::Observation::Metric { class Gauge; }

namespace CluicheTest {

class PathfindingTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Full navigation stack: A* + FlowField + Steering";
    static constexpr unsigned int kMinDisplayFrames = 300; // 10s at 30Hz

    explicit PathfindingTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~PathfindingTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 900; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void BuildGrid();
    void ComputeInitialPath();
    void ComputeInitialFlowField();
    void RegisterCheckpoints(Dia::Automation::AutomationService* service);
    void RegisterMetrics();
    void EmitMetrics();
    bool AllCheckpointsPassed() const;
    void UpdateAgents(float dt);

    // --- Grid and navigation stack ---
    // mGrid must be declared before mFlowAdapter (mFlowAdapter holds a const& to mGrid)
    Dia::Pathfinding::SquarePathGrid          mGrid;
    Dia::FlowField::SquareFlowAdapter         mFlowAdapter;
    Dia::Pathfinding::FlatCostProvider        mCostProvider;
    Dia::FlowField::FlowFieldCache<Dia::FlowField::SquareFlowAdapter> mFlowCache;

    const Dia::FlowField::FlowField*          mCurrentField = nullptr;

    // --- Agents (plain C array, no STL in members) ---
    static constexpr int kAgentCount = 3;
    Dia::Steering::SteeringAgent              mAgents[kAgentCount];

    // --- Goal world position ---
    Dia::Maths::Vector2D                      mGoalWorld;

    // Trails — ring buffer of last 20 positions per agent
    static constexpr int kTrailLen = 20;
    Dia::Maths::Vector2D mTrails[kAgentCount][kTrailLen];
    int                  mTrailHead[kAgentCount] = {};
    bool                 mAgentArrived[kAgentCount] = {};

    // --- State ---
    static constexpr float kCellSize       = 32.0f;
    static constexpr float kArrivalRadius  = 24.0f;
    static constexpr int   kGridWidth      = 20;
    static constexpr int   kGridHeight     = 15;
    static constexpr int   kDynamicBlockFrame = 60;
    static constexpr float kAgentMaxSpeed  = 80.0f;
    static constexpr float kSlowingRadius  = 64.0f;
    static constexpr float kSteeringGain   = 8.0f;   // lerp factor for velocity blending

    bool  mPathComputed        = false;
    bool  mFlowFieldReady      = false;
    bool  mRecomputedAfterBlock = false;
    bool  mFirstArrived        = false;
    bool  mAllArrived          = false;
    bool  mAllPassed           = false;
    int   mRecomputeCount      = 0;
    float mStageTime           = 0.0f;

    // --- Metrics ---
    Dia::Observation::Metric::Gauge* mMetricAgent0Dist    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricAgent1Dist    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricAgent2Dist    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricRecomputeCount = nullptr;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<PathfindingTestDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
