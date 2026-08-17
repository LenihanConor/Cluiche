// TestGridVisibilityExhaustive.cpp — Boundary, golden, invariant, and stress tests
// for GridVisibilitySystem.
//
// Suites: GridVisibilityShadow, GridVisibilityDecay, GridVisibilityIsolation,
//         GridVisibilityChunkSize, GridVisibilityStress

#include <gtest/gtest.h>
#include <memory>

#include "DiaGridVisibility/VisibilityGroupId.h"
#include "DiaGridVisibility/VisibilityState.h"
#include "DiaGridVisibility/GridVisibilitySystem.h"
#include "DiaGridVisibility/Testing/VisibilityTestHelpers.h"

#include <algorithm>

#include "DiaPathfinding/SquarePathGrid.h"
#include "DiaPathfinding/CellCoord.h"

#include "DiaEntity/Domain.h"
#include "DiaEntity/ComponentPool.h"
#include "DiaEntitySpatial/EntitySpatialIndex.h"
#include "DiaEntitySpatial/EntitySpatialModule.h"
#include "DiaEntitySpatial/SpatialComponent.h"
#include "DiaEntitySpatial/Testing/SpatialTestHelpers.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaMaths/Vector/Vector2D.h"

using Dia::GridVisibility::GridVisibilitySystem;
using Dia::GridVisibility::VisibilityGroupId;
using Dia::GridVisibility::VisibilityState;
using Dia::GridVisibility::Testing::AssertCellVisible;
using Dia::GridVisibility::Testing::AssertCellRevealed;
using Dia::GridVisibility::Testing::AssertCellUnexplored;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::SquarePathGrid;

namespace
{
    Dia::Entity::Entity MakeEntity(uint32_t index)
    {
        return Dia::Entity::Entity(index, 1u);
    }

    // Domain + spatial module wired for a 128x128 world of 1-unit cells.
    struct ExFixtureWorld
    {
        ExFixtureWorld()
        {
            domain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpatial::SpatialComponent>(
                Dia::EntitySpatial::SpatialComponent::kTypeId));

            Dia::EntitySpatial::EntitySpatialIndex::SquareDef def;
            def.worldBounds = Dia::Geometry2D::AARect(Dia::Maths::Vector2D(0.f, 0.f),
                                                      Dia::Maths::Vector2D(128.f, 128.f));
            def.cellSize    = 4.f;

            spatial = std::make_unique<Dia::EntitySpatial::EntitySpatialModule>(domain, def);
        }

        Dia::Entity::Entity SpawnAt(float x, float y)
        {
            const Dia::Entity::Entity e =
                Dia::EntitySpatial::Testing::SpawnSpatialEntity(domain, Dia::Maths::Vector2D(x, y));
            Dia::EntitySpatial::Testing::FlushDomain(domain);
            spatial->Update();
            return e;
        }

        Dia::Entity::Domain                                      domain;
        std::unique_ptr<Dia::EntitySpatial::EntitySpatialModule> spatial;
    };
}

// ---------------------------------------------------------------------------
// Shadowcasting — wall blocking, circular radius, blocker visibility
// ---------------------------------------------------------------------------

// A wall cell directly to the east of the source is itself lit (blockers are
// visible per the recursive-octant convention), but the cell behind it is not.
TEST(GridVisibilityShadow, WallBlocker_IsItselfVisible)
{
    // Source at vis cell (4,4), wall at vis cell (5,4).
    // visCellSize = 1.0 * 1 = 1.0, so world pos (4.5, 4.5) → vis cell (4,4).
    SquarePathGrid grid(16, 16);
    grid.SetPassable(CellCoord{ 5, 4 }, false);   // wall

    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(4.5f, 4.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 5.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    // Wall cell itself must be Visible — the blocker is illuminated.
    AssertCellVisible(*sys, CellCoord{ 5, 4 }, blue);
}

TEST(GridVisibilityShadow, WallBlocker_CellBehindWallNotVisible)
{
    SquarePathGrid grid(16, 16);
    grid.SetPassable(CellCoord{ 5, 4 }, false);   // wall

    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(4.5f, 4.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 5.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    // Cell at (6,4) is directly behind the wall — must not be Visible.
    EXPECT_NE(sys->GetCellState(CellCoord{ 6, 4 }, blue), VisibilityState::Visible);
}

TEST(GridVisibilityShadow, OriginCellAlwaysVisible)
{
    // The source's own cell must be lit regardless of radius, grid size, or walls.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 0.5f);   // tiny radius — just the origin
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellVisible(*sys, CellCoord{ 8, 8 }, blue);
}

TEST(GridVisibilityShadow, CircularRadius_DiagonalCornerBeyondRadiusUnexplored)
{
    // Source at (8,8), radius=3.0.
    // Cell (11,11) is at distance sqrt(18) ≈ 4.24 > 3.0 — must NOT be lit.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellUnexplored(*sys, CellCoord{ 11, 11 }, blue);
}

TEST(GridVisibilityShadow, CircularRadius_DiagonalCellWithinRadiusVisible)
{
    // Source at (8,8), radius=3.0.
    // Cell (10,10) is at distance sqrt(8) ≈ 2.83 < 3.0 — must be lit (open grid).
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellVisible(*sys, CellCoord{ 10, 10 }, blue);
}

// ---------------------------------------------------------------------------
// Decay — Visible → Revealed transition, Revealed persists
// ---------------------------------------------------------------------------

TEST(GridVisibilityDecay, RemovingSourceDecaysCellsToRevealed)
{
    // Cells go from Visible (source present) to Revealed (source removed, Update run).
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    // Confirm origin is Visible before removal.
    ASSERT_EQ(sys->GetCellState(CellCoord{ 8, 8 }, blue), VisibilityState::Visible);

    // Remove the source; on the next Update the decay loop runs.
    sys->UnregisterSightSource(src);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellRevealed(*sys, CellCoord{ 8, 8 }, blue);
}

TEST(GridVisibilityDecay, RevealedCellDoesNotDecayFurtherToUnexplored)
{
    // Once Revealed, further Updates without coverage must not revert to Unexplored.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);
    sys->UnregisterSightSource(src);
    sys->Update(1.0f, *world.spatial, world.domain);   // now Revealed

    // A third tick with no sources must leave the cell Revealed.
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellRevealed(*sys, CellCoord{ 8, 8 }, blue);
}

TEST(GridVisibilityDecay, UnvisitedCellStaysUnexplored)
{
    // A cell completely outside the source's radius must remain Unexplored
    // even after multiple Updates.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(2.5f, 2.5f);   // vis cell (2,2)

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 1.0f);  // tiny radius
    sys->Update(1.0f, *world.spatial, world.domain);
    sys->Update(1.0f, *world.spatial, world.domain);

    // Cell (14,14) is far from the source — must never be seen.
    AssertCellUnexplored(*sys, CellCoord{ 14, 14 }, blue);
}

// ---------------------------------------------------------------------------
// Multi-group isolation — groups do not share visibility state
// ---------------------------------------------------------------------------

TEST(GridVisibilityIsolation, TwoGroupsDoNotShareCellStates)
{
    // "Blue" source illuminates (4,4); "Red" has no source.
    // Cell (4,4) must be Visible for Blue but Unexplored for Red.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity blueEntity = world.SpawnAt(4.5f, 4.5f);

    const VisibilityGroupId blue("Blue");
    const VisibilityGroupId red("Red");

    sys->RegisterSightSource(blueEntity, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellVisible(*sys, CellCoord{ 4, 4 }, blue);
    AssertCellUnexplored(*sys, CellCoord{ 4, 4 }, red);
}

TEST(GridVisibilityIsolation, TwoGroupsSeeTheirOwnAreaIndependently)
{
    // Blue source at (3,3), Red source at (12,12) on open 16x16 grid.
    // Each group's origin cell is Visible; the other group's origin is not.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity blueEntity = world.SpawnAt(3.5f, 3.5f);
    const Dia::Entity::Entity redEntity  = world.SpawnAt(12.5f, 12.5f);

    const VisibilityGroupId blue("Blue");
    const VisibilityGroupId red("Red");

    sys->RegisterSightSource(blueEntity, blue, 2.0f);
    sys->RegisterSightSource(redEntity,  red,  2.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellVisible  (*sys, CellCoord{  3,  3 }, blue);
    AssertCellUnexplored(*sys, CellCoord{ 12, 12 }, blue);

    AssertCellVisible  (*sys, CellCoord{ 12, 12 }, red);
    AssertCellUnexplored(*sys, CellCoord{  3,  3 }, red);
}

TEST(GridVisibilityIsolation, RemovingOneSourceDoesNotAffectOtherGroup)
{
    // Blue removes its source; Red's visibility must be unaffected.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity blueEntity = world.SpawnAt(3.5f, 3.5f);
    const Dia::Entity::Entity redEntity  = world.SpawnAt(12.5f, 12.5f);

    const VisibilityGroupId blue("Blue");
    const VisibilityGroupId red("Red");

    sys->RegisterSightSource(blueEntity, blue, 2.0f);
    sys->RegisterSightSource(redEntity,  red,  2.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    // Remove Blue's source and run one more tick.
    sys->UnregisterSightSource(blueEntity);
    sys->Update(1.0f, *world.spatial, world.domain);

    // Red's coverage must be unchanged.
    AssertCellVisible(*sys, CellCoord{ 12, 12 }, red);
    // Blue's origin decays to Revealed.
    AssertCellRevealed(*sys, CellCoord{ 3, 3 }, blue);
}

// ---------------------------------------------------------------------------
// Chunk-size variants
// ---------------------------------------------------------------------------

TEST(GridVisibilityChunkSize, ChunkSizeTwo_VisCellCoversTwoByTwoPathCells)
{
    // 16x16 path grid at chunkSize 2 → 8x8 vis grid.
    // Source at world (2.5, 2.5) with cellSize=1 → visCellSize=2 → vis cell (1,1).
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 2);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(2.5f, 2.5f);

    ASSERT_EQ(sys->GetVisWidth(),  8);
    ASSERT_EQ(sys->GetVisHeight(), 8);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellVisible(*sys, CellCoord{ 1, 1 }, blue);
}

TEST(GridVisibilityChunkSize, ChunkSizeOne_VisGridMatchesPathGrid)
{
    SquarePathGrid grid(8, 8);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    EXPECT_EQ(sys->GetVisWidth(),  8);
    EXPECT_EQ(sys->GetVisHeight(), 8);
}

TEST(GridVisibilityChunkSize, ChunkSizeImpassableBlockPassable_MixedChunkIsPassable)
{
    // A 4x4 path grid at chunkSize=2 → 2x2 vis grid.
    // Chunk (0,0) has 3 impassable + 1 passable → vis cell (0,0) is passable.
    SquarePathGrid grid(4, 4);
    grid.SetPassable(CellCoord{ 0, 0 }, false);
    grid.SetPassable(CellCoord{ 1, 0 }, false);
    grid.SetPassable(CellCoord{ 0, 1 }, false);
    // (1,1) remains passable → chunk (0,0) is passable.
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 2);
    EXPECT_TRUE(sys->IsVisCellPassable(CellCoord{ 0, 0 }));
}

TEST(GridVisibilityChunkSize, ChunkSizeAllImpassable_VisCellImpassable)
{
    // A 4x4 path grid at chunkSize=2 → 2x2 vis grid.
    // Chunk (1,0) has all 4 cells impassable → vis cell (1,0) is impassable.
    SquarePathGrid grid(4, 4);
    grid.SetPassable(CellCoord{ 2, 0 }, false);
    grid.SetPassable(CellCoord{ 3, 0 }, false);
    grid.SetPassable(CellCoord{ 2, 1 }, false);
    grid.SetPassable(CellCoord{ 3, 1 }, false);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 2);
    EXPECT_FALSE(sys->IsVisCellPassable(CellCoord{ 1, 0 }));
}

// ---------------------------------------------------------------------------
// Stress — large grid with many sources must not crash or corrupt state
// ---------------------------------------------------------------------------

// Helper observer types used by the callback and entity-query suites below.
namespace
{
    class DisappearTrackingObserver final : public Dia::GridVisibility::IVisibilityChangeObserver
    {
    public:
        void OnEntityVisibilityChanged(Dia::Entity::Entity entity, VisibilityGroupId,
                                       bool isNowVisible) override
        {
            if (!isNowVisible)
            {
                ++mDisappearCount;
                mLastDisappeared = entity;
            }
        }

        int                 mDisappearCount = 0;
        Dia::Entity::Entity mLastDisappeared{};
    };

    class OriginCellObserver final : public Dia::GridVisibility::IVisibilityChangeObserver
    {
    public:
        explicit OriginCellObserver(CellCoord watchCell) : mWatchCell(watchCell) {}

        void OnCellStateChanged(CellCoord cell, VisibilityGroupId,
                                VisibilityState oldState, VisibilityState newState) override
        {
            if (cell.x == mWatchCell.x && cell.y == mWatchCell.y)
            {
                mFired    = true;
                mOldState = oldState;
                mNewState = newState;
            }
        }

        bool            mFired    = false;
        VisibilityState mOldState = VisibilityState::Unexplored;
        VisibilityState mNewState = VisibilityState::Unexplored;

    private:
        CellCoord mWatchCell;
    };
}

TEST(GridVisibilityStress, LargeGridManySourcesNoCorruption)
{
    // 32x32 vis grid (path grid 32x32, chunkSize=1), 4 sight sources.
    // Just verifies no crash and the sources' origin cells are all Visible.
    SquarePathGrid grid(32, 32);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;

    const VisibilityGroupId blue("Blue");

    struct SourcePos { float x; float y; CellCoord visCell; };
    const SourcePos sources[] = {
        {  4.5f,  4.5f, { 4,  4 } },
        { 16.5f,  4.5f, {16,  4 } },
        {  4.5f, 16.5f, { 4, 16 } },
        { 16.5f, 16.5f, {16, 16 } },
    };

    for (const auto& sp : sources)
    {
        const Dia::Entity::Entity e = world.SpawnAt(sp.x, sp.y);
        sys->RegisterSightSource(e, blue, 5.0f);
    }

    // Run 5 update ticks without crash.
    for (int tick = 0; tick < 5; ++tick)
        sys->Update(1.0f, *world.spatial, world.domain);

    // Verify each source's origin cell is Visible.
    for (const auto& sp : sources)
        AssertCellVisible(*sys, sp.visCell, blue);
}

// ---------------------------------------------------------------------------
// Radius change — re-registering with a larger radius dirtifies and expands
// ---------------------------------------------------------------------------

TEST(GridVisibilityRadiusChange, LargerRadiusAfterReregister_ExpandsVisibility)
{
    // Source at (4.5, 4.5), initial radius 1.0 — only nearby cells lit.
    // Cell (4,9) is 5 units away and must be Unexplored.
    // Re-register with radius 6.0 — cell (4,9) must become Visible.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(4.5f, 4.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 1.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellUnexplored(*sys, CellCoord{ 4, 9 }, blue);

    sys->RegisterSightSource(src, blue, 6.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellVisible(*sys, CellCoord{ 4, 9 }, blue);
}

// ---------------------------------------------------------------------------
// Entity query round-trips — CanSee true, GetVisibleEntities non-empty
// ---------------------------------------------------------------------------

TEST(GridVisibilityEntityQueries, CanSee_EntityInVisibleCell_ReturnsTrue)
{
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    EXPECT_TRUE(sys->CanSee(src, blue));
}

TEST(GridVisibilityEntityQueries, GetVisibleEntities_ContainsSpatialEntityInRange)
{
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    const auto span = sys->GetVisibleEntities(blue);
    const bool found = std::find(span.begin(), span.end(), src) != span.end();
    EXPECT_TRUE(found) << "Source entity must appear in GetVisibleEntities span";
}

// ---------------------------------------------------------------------------
// Observer callbacks — entity disappear, cell transition args
// ---------------------------------------------------------------------------

TEST(GridVisibilityCallbacks, EntityDisappears_WhenSourceRemovedAndUpdateRuns)
{
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);
    ASSERT_TRUE(sys->CanSee(src, blue));

    DisappearTrackingObserver obs;
    sys->AddChangeListener(&obs);

    sys->UnregisterSightSource(src);
    sys->Update(1.0f, *world.spatial, world.domain);

    EXPECT_GE(obs.mDisappearCount, 1);
    EXPECT_EQ(obs.mLastDisappeared, src);
}

TEST(GridVisibilityCallbacks, CellTransition_UnexploredToVisible_HasCorrectArgs)
{
    // The origin cell (8,8) starts Unexplored and must transition to Visible
    // on the first Update. The callback must receive the correct old/new states.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");

    OriginCellObserver obs(CellCoord{ 8, 8 });
    sys->AddChangeListener(&obs);

    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    EXPECT_TRUE(obs.mFired)    << "OnCellStateChanged must fire for origin cell";
    EXPECT_EQ(obs.mOldState, VisibilityState::Unexplored);
    EXPECT_EQ(obs.mNewState, VisibilityState::Visible);
}

TEST(GridVisibilityCallbacks, CellTransition_VisibleToRevealed_HasCorrectArgs)
{
    // After the source is removed and a tick runs, the origin cell must transition
    // Visible → Revealed. The callback must fire with the correct old/new states.
    SquarePathGrid grid(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<SquarePathGrid>>(grid, 1);
    ExFixtureWorld world;
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 3.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    OriginCellObserver obs(CellCoord{ 8, 8 });
    sys->AddChangeListener(&obs);

    sys->UnregisterSightSource(src);
    sys->Update(1.0f, *world.spatial, world.domain);

    EXPECT_TRUE(obs.mFired)    << "OnCellStateChanged must fire for Visible→Revealed transition";
    EXPECT_EQ(obs.mOldState, VisibilityState::Visible);
    EXPECT_EQ(obs.mNewState, VisibilityState::Revealed);
}
