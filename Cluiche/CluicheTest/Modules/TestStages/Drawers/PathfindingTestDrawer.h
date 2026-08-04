#pragma once
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaFlowField/FlowField.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

class PathfindingTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr int kAgentCount = 3;
    static constexpr int kTrailLen   = 20;

    PathfindingTestDrawer(
        const Dia::Pathfinding::SquarePathGrid&  grid,
        const Dia::FlowField::FlowField*&        currentField,   // pointer-to-pointer (null during recompute)
        const Dia::Steering::SteeringAgent*      agents,         // array[kAgentCount]
        const Dia::Maths::Vector2D (*trails)[kTrailLen],         // trails[kAgentCount][kTrailLen]
        const int*                               trailHeads,     // trailHeads[kAgentCount]
        const bool*                              agentArrived,   // agentArrived[kAgentCount]
        const Dia::Maths::Vector2D&              goalWorld,
        const bool&                              pathComputed,
        const bool&                              flowFieldReady,
        const bool&                              recomputedAfterBlock,
        const bool&                              firstArrived,
        const bool&                              allArrived,
        const int&                               recomputeCount,
        float                                    cellSize,
        const float&                             stageTime,      // accumulated time in seconds (for goal pulse)
        const Dia::Debug::DebugLayerManager&     layerManager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    Dia::Maths::Vector2D CellCenter(int col, int row) const;

    const Dia::Pathfinding::SquarePathGrid&  mGrid;
    const Dia::FlowField::FlowField*&        mCurrentField;
    const Dia::Steering::SteeringAgent*      mAgents;
    const Dia::Maths::Vector2D (*mTrails)[kTrailLen];
    const int*                               mTrailHeads;
    const bool*                              mAgentArrived;
    const Dia::Maths::Vector2D&              mGoalWorld;
    const bool&                              mPathComputed;
    const bool&                              mFlowFieldReady;
    const bool&                              mRecomputedAfterBlock;
    const bool&                              mFirstArrived;
    const bool&                              mAllArrived;
    const int&                               mRecomputeCount;
    float                                    mCellSize;
    const float&                             mStageTime;
};

} // namespace CluicheTest
#endif // DIA_DEBUG
