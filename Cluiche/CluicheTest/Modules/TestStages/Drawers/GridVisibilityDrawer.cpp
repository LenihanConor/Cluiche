#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/GridVisibilityDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGridVisibility/VisibilityState.h>
#include <DiaPathfinding/CellCoord.h>
#include <cmath>
#include <cstdio>

namespace CluicheTest {

// ---------------------------------------------------------------------------
// Visibility group IDs — must match GridVisibilityTestStageModule registrations
// ---------------------------------------------------------------------------
static const Dia::Core::StringCRC kGroupRed("Red");
static const Dia::Core::StringCRC kGroupBlue("Blue");

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
GridVisibilityDrawer::GridVisibilityDrawer(
    const Dia::GridVisibility::GridVisibilitySystem<Dia::Pathfinding::SquarePathGrid>& visSystem,
    const Dia::Pathfinding::SquarePathGrid& grid,
    float cellSize,
    const Dia::Maths::Vector2D& gridOrigin,
    const Dia::Maths::Vector2D* entityPositions,
    const float* enemyFlashTimers,
    const float& stageTime,
    const Dia::Debug::DebugLayerManager& /*layerManager*/)
    : mVisSystem(visSystem)
    , mGrid(grid)
    , mCellSize(cellSize)
    , mGridOrigin(gridOrigin)
    , mEntityPositions(entityPositions)
    , mEnemyFlashTimers(enemyFlashTimers)
    , mStageTime(stageTime)
{}

// ---------------------------------------------------------------------------
// GetLayerName
// ---------------------------------------------------------------------------
Dia::Core::StringCRC GridVisibilityDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("GridVisibilityTest");
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void GridVisibilityDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    using VS   = Dia::GridVisibility::VisibilityState;
    using V2   = Dia::Maths::Vector2D;
    using RGBA = Dia::Core::RGBA;

    const RGBA kNoOutline(0, 0, 0, 0);

    const int W = mGrid.GetWidth();
    const int H = mGrid.GetHeight();

    // -------------------------------------------------------------------------
    // 1. Grid cell pass
    //    For each cell: determine colour from both group states, then draw a
    //    filled rect. Outline is transparent (no visible border between cells).
    // -------------------------------------------------------------------------
    for (int col = 0; col < W; ++col)
    {
        for (int row = 0; row < H; ++row)
        {
            const V2 tl(mGridOrigin.X() +  col      * mCellSize,
                        mGridOrigin.Y() +  row      * mCellSize);
            const V2 br(mGridOrigin.X() + (col + 1) * mCellSize,
                        mGridOrigin.Y() + (row + 1) * mCellSize);

            if (!mGrid.IsPassable({col, row}))
            {
                draw.RequestDrawRect(tl, br, kNoOutline, RGBA(25, 25, 30, 255));
                continue;
            }

            const VS redState  = mVisSystem.GetCellState({col, row}, kGroupRed);
            const VS blueState = mVisSystem.GetCellState({col, row}, kGroupBlue);

            const bool redVisible   = (redState  == VS::Visible);
            const bool redRevealed  = (redState  == VS::Revealed);
            const bool blueVisible  = (blueState == VS::Visible);
            const bool blueRevealed = (blueState == VS::Revealed);

            RGBA fillColour;

            if (redVisible && blueVisible)
            {
                fillColour = RGBA(130, 55, 140, 215);
            }
            else if (redVisible)
            {
                // Red=Visible, Blue<=Revealed
                fillColour = RGBA(160, 60, 50, 215);
            }
            else if (blueVisible)
            {
                // Blue=Visible, Red<=Revealed
                fillColour = RGBA(50, 90, 160, 215);
            }
            else if (redRevealed && blueRevealed)
            {
                fillColour = RGBA(40, 22, 40, 255);
            }
            else if (redRevealed)
            {
                // Red=Revealed, Blue=Unexplored (Hidden)
                fillColour = RGBA(55, 22, 22, 255);
            }
            else if (blueRevealed)
            {
                // Blue=Revealed, Red=Unexplored (Hidden)
                fillColour = RGBA(22, 22, 55, 255);
            }
            else
            {
                // Both Unexplored (Hidden)
                fillColour = RGBA(10, 10, 15, 255);
            }

            draw.RequestDrawRect(tl, br, kNoOutline, fillColour);
        }
    }

    // -------------------------------------------------------------------------
    // 2. Entity pass
    // -------------------------------------------------------------------------

    // [0] Red patrol
    draw.RequestDraw(mEntityPositions[0], 10.0f,
                     RGBA(220, 80, 80, 255), RGBA(220, 80, 80, 255));

    // [1] Blue patrol
    draw.RequestDraw(mEntityPositions[1], 10.0f,
                     RGBA(80, 130, 220, 255), RGBA(80, 130, 220, 255));

    // [2..6] Enemies E0..E4
    for (int i = 0; i < kEnemyCount; ++i)
    {
        const V2& pos = mEntityPositions[2 + i];

        if (mEnemyFlashTimers[i] > 0.0f)
        {
            // Spotted: gold with pulsing radius
            const float radius = 8.0f + 4.0f * sinf(mStageTime * 6.0f);
            draw.RequestDraw(pos, radius,
                             RGBA(220, 180, 60, 255), RGBA(220, 180, 60, 255));
        }
        else
        {
            // Default grey
            draw.RequestDraw(pos, 8.0f,
                             RGBA(120, 120, 130, 200), RGBA(120, 120, 130, 200));
        }
    }

    // -------------------------------------------------------------------------
    // 3. HUD pass — visible-cell counts per group
    // -------------------------------------------------------------------------
    int redVis  = 0;
    int blueVis = 0;

    for (int col = 0; col < W; ++col)
    {
        for (int row = 0; row < H; ++row)
        {
            const Dia::Pathfinding::CellCoord cell{col, row};
            if (mVisSystem.GetCellState(cell, kGroupRed)  == VS::Visible) ++redVis;
            if (mVisSystem.GetCellState(cell, kGroupBlue) == VS::Visible) ++blueVis;
        }
    }

    char redBuf[32];
    char blueBuf[32];
    snprintf(redBuf,  sizeof(redBuf),  "Red:  %d vis", redVis);
    snprintf(blueBuf, sizeof(blueBuf), "Blue: %d vis", blueVis);

    draw.RequestDrawText(V2(-355.0f, -260.0f), redBuf,  12.0f, RGBA(220, 80, 80, 255));
    draw.RequestDrawText(V2( 200.0f, -260.0f), blueBuf, 12.0f, RGBA(80, 130, 220, 255));
}

} // namespace CluicheTest
#endif // DIA_DEBUG
