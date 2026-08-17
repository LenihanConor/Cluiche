#pragma once
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGridVisibility/GridVisibilitySystem.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

// ---------------------------------------------------------------------------
// GridVisibilityDrawer
//
// Visual debug layer for GridVisibilityTestStage.
// Renders a 24x18 fog-of-war grid with two independent sight groups (Red/Blue),
// entity markers for two patrols and five enemies, and per-group visible-cell
// counts in the HUD.
// ---------------------------------------------------------------------------
class GridVisibilityDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr int kEntityCount = 7;  // [0]=RedPatrol, [1]=BluePatrol, [2..6]=E0..E4
    static constexpr int kEnemyCount  = 5;  // E0..E4

    GridVisibilityDrawer(
        const Dia::GridVisibility::GridVisibilitySystem<Dia::Pathfinding::SquarePathGrid>& visSystem,
        const Dia::Pathfinding::SquarePathGrid& grid,
        float cellSize,
        const Dia::Maths::Vector2D& gridOrigin,
        const Dia::Maths::Vector2D* entityPositions,  // [7]: [0]=RedPatrol, [1]=BluePatrol, [2..6]=E0..E4
        const float* enemyFlashTimers,                 // [5]: seconds remaining, >0 = pulsing
        const float& stageTime,
        const Dia::Debug::DebugLayerManager& layerManager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::GridVisibility::GridVisibilitySystem<Dia::Pathfinding::SquarePathGrid>& mVisSystem;
    const Dia::Pathfinding::SquarePathGrid& mGrid;
    float                           mCellSize;
    const Dia::Maths::Vector2D&     mGridOrigin;
    const Dia::Maths::Vector2D*     mEntityPositions;
    const float*                    mEnemyFlashTimers;
    const float&                    mStageTime;
};

} // namespace CluicheTest
#endif // DIA_DEBUG
