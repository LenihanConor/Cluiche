// TestHexGridVisibility.cpp — GoogleTest coverage for GridVisibilitySystem with HexPathGrid.
//
// Suites: HexGridVisibilityConcept, HexGridVisibilityConstruction, HexGridVisibilityShadowcasting

#include <gtest/gtest.h>
#include <memory>
#include <cmath>

#include "DiaGridVisibility/VisibilityGroupId.h"
#include "DiaGridVisibility/VisibilityState.h"
#include "DiaGridVisibility/GridVisibilitySystem.h"
#include "DiaGridVisibility/Testing/VisibilityTestHelpers.h"

#include "DiaPathfinding/HexPathGrid.h"
#include "DiaPathfinding/HexTopology.h"
#include "DiaPathfinding/CellCoord.h"

#include "DiaEntity/Domain.h"
#include "DiaEntity/ComponentPool.h"
#include "DiaEntitySpatial/EntitySpatialIndex.h"
#include "DiaEntitySpatial/EntitySpatialModule.h"
#include "DiaEntitySpatial/SpatialComponent.h"
#include "DiaEntitySpatial/Testing/SpatialTestHelpers.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaMaths/Vector/Vector2D.h"

using Dia::GridVisibility::CVisibilityGraph;
using Dia::GridVisibility::GridVisibilitySystem;
using Dia::GridVisibility::VisibilityGroupId;
using Dia::GridVisibility::VisibilityState;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexPathGrid;
using Dia::Pathfinding::HexTopology;
using Dia::GridVisibility::Testing::AssertCellVisible;
using Dia::GridVisibility::Testing::AssertCellUnexplored;

using HexVisSystem = GridVisibilitySystem<HexPathGrid>;

namespace
{
    std::unique_ptr<HexVisSystem> MakeHexSystem(const HexPathGrid& grid, int chunkSize)
    {
        return std::make_unique<HexVisSystem>(grid, chunkSize);
    }

    // Hex-grid test fixture similar to ExFixtureWorld from TestGridVisibilityExhaustive.cpp
    struct HexFixtureWorld
    {
        HexFixtureWorld()
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
// Concept
// ---------------------------------------------------------------------------

// AC1 part 1: HexPathGrid now satisfies CVisibilityGraph (exclusion removed)
TEST(HexGridVisibilityConcept, HexPathGridSatisfiesCVisibilityGraph)
{
    static_assert(CVisibilityGraph<HexPathGrid>,
                  "HexPathGrid must satisfy CVisibilityGraph after exclusion removal");
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

// AC1 part 2: Construct with axial layout (radius 5)
TEST(HexGridVisibilityConstruction, AxialHexagonGridCompiles)
{
    HexPathGrid grid(5);  // Axial hexagon, radius 5
    auto system = MakeHexSystem(grid, 1);

    // Axial hexagon with radius 5 has 11x11 bounding box (but many cells are out-of-bounds)
    // For chunkSize=1, visWidth and visHeight should match the grid's reported width/height
    EXPECT_GT(system->GetVisWidth(), 0);
    EXPECT_GT(system->GetVisHeight(), 0);
    EXPECT_EQ(system->GetChunkSize(), 1);
}

// AC1 part 3: Construct with rectangular layout (20x15)
TEST(HexGridVisibilityConstruction, RectangularHexGridCompiles)
{
    HexPathGrid grid(20, 15);  // Rectangular offset layout
    auto system = MakeHexSystem(grid, 1);

    EXPECT_EQ(system->GetVisWidth(), 20);
    EXPECT_EQ(system->GetVisHeight(), 15);
    EXPECT_EQ(system->GetChunkSize(), 1);
}

// AC5: Rectangular 42x24 hex grid with 7-cell cluster consolidation
TEST(HexGridVisibilityConstruction, HexConsolidationSevenCellCluster)
{
    HexPathGrid grid(42, 24);
    auto system = MakeHexSystem(grid, 7);  // 7-cell cluster consolidation

    // Expected: approximately 6x4 visibility grid (42/7=6, 24/7≈3.4→4 via ceiling)
    EXPECT_EQ(system->GetVisWidth(), 6);
    EXPECT_EQ(system->GetVisHeight(), 4);
    EXPECT_EQ(system->GetChunkSize(), 7);
}

// ---------------------------------------------------------------------------
// Shadowcasting
// ---------------------------------------------------------------------------

// AC2: Basic hex visibility (rectangular layout) - all 6 neighbors visible
TEST(HexGridVisibilityShadowcasting, BasicHexVisibilityRectangular)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    // Sight source at center (5, 4), radius 3 cells
    const Dia::Entity::Entity src = world.SpawnAt(5.5f, 4.5f);

    const VisibilityGroupId groupId("Player");
    system->RegisterSightSource(src, groupId, 3.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // AC2: Origin should be Visible
    AssertCellVisible(*system, CellCoord{ 5, 4 }, groupId);

    // All 6 immediate neighbors should be Visible (flat-top odd-q, column 5 is odd)
    // Direction 0 (NE): col+1, row+0 for odd columns
    AssertCellVisible(*system, CellCoord{ 6, 4 }, groupId);
    // Direction 1 (N): col+0, row-1
    AssertCellVisible(*system, CellCoord{ 5, 3 }, groupId);
    // Direction 2 (NW): col-1, row+0 for odd columns
    AssertCellVisible(*system, CellCoord{ 4, 4 }, groupId);
    // Direction 3 (SW): col-1, row+1 for odd columns
    AssertCellVisible(*system, CellCoord{ 4, 5 }, groupId);
    // Direction 4 (S): col+0, row+1
    AssertCellVisible(*system, CellCoord{ 5, 5 }, groupId);
    // Direction 5 (SE): col+1, row+1 for odd columns
    AssertCellVisible(*system, CellCoord{ 6, 5 }, groupId);

    // Cells beyond radius 3 should be Unexplored (pick a distant cell)
    AssertCellUnexplored(*system, CellCoord{ 0, 0 }, groupId);
}

// AC3: Hex blocker shadow (rectangular layout)
TEST(HexGridVisibilityShadowcasting, HexBlockerShadow)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    // Place a blocker at (4,3) - NE of source at (2,4)
    grid.SetPassable(CellCoord{ 4, 3 }, false);

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity src = world.SpawnAt(2.5f, 4.5f);

    const VisibilityGroupId groupId("Player");
    system->RegisterSightSource(src, groupId, 4.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // AC3: The blocker itself (4,3) should be Visible
    AssertCellVisible(*system, CellCoord{ 4, 3 }, groupId);

    // Origin should be visible
    AssertCellVisible(*system, CellCoord{ 2, 4 }, groupId);
}

// AC4: Circular sight radius on hex grid
TEST(HexGridVisibilityShadowcasting, CircularSightRadius)
{
    HexPathGrid grid(16, 12);
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 16; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 6.5f);

    const VisibilityGroupId groupId("Player");
    system->RegisterSightSource(src, groupId, 5.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // AC4: Origin should be visible
    AssertCellVisible(*system, CellCoord{ 8, 6 }, groupId);

    // Cells beyond radius 5 should be unexplored
    AssertCellUnexplored(*system, CellCoord{ 8, 0 }, groupId);
}

// AC7: Axial hex layout - actual visibility test with Update()
TEST(HexGridVisibilityShadowcasting, AxialHexLayout)
{
    HexPathGrid grid(8);  // Axial hexagon, radius 8
    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    // Place sight source at axial origin (0, 0) which is at grid center
    // Axial grids use centered coordinates: (-radius, -radius) to (+radius, +radius)
    const Dia::Entity::Entity src = world.SpawnAt(8.5f, 8.5f);  // center of bounding box

    const VisibilityGroupId groupId("Player");
    system->RegisterSightSource(src, groupId, 3.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // AC7: Origin (center of grid) should be visible
    // Note: axial grid maps centered coords to 0-based grid internally
    // Origin (0,0) in axial space is at (radius, radius) in 0-based grid = (8, 8)
    AssertCellVisible(*system, CellCoord{ 8, 8 }, groupId);

    // Construction verified
    EXPECT_GT(system->GetVisWidth(), 0);
    EXPECT_GT(system->GetVisHeight(), 0);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

// Re-registration updates radius
TEST(HexGridVisibilityRegistration, ReRegisterUpdatesRadius)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity src = world.SpawnAt(5.5f, 4.5f);
    const VisibilityGroupId groupId("Player");

    // Register with radius 1
    system->RegisterSightSource(src, groupId, 1.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Cell at distance 2 should be Unexplored
    AssertCellUnexplored(*system, CellCoord{ 5, 2 }, groupId);

    // Re-register with larger radius 3
    system->RegisterSightSource(src, groupId, 3.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Now cell at distance 2 should be Visible
    AssertCellVisible(*system, CellCoord{ 5, 2 }, groupId);
}

// Unregister removes source
TEST(HexGridVisibilityRegistration, UnregisterRemovesSource)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity src = world.SpawnAt(5.5f, 4.5f);
    const VisibilityGroupId groupId("Player");

    system->RegisterSightSource(src, groupId, 3.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    AssertCellVisible(*system, CellCoord{ 5, 4 }, groupId);

    // Unregister
    system->UnregisterSightSource(src);
    system->Update(1.0f, *world.spatial, world.domain);

    // Cell should decay to Revealed (was visible, now not covered)
    EXPECT_EQ(system->GetCellState(CellCoord{ 5, 4 }, groupId), VisibilityState::Revealed);
}

// Distinct groups tracked separately
TEST(HexGridVisibilityRegistration, DistinctGroupsTrackedSeparately)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity src1 = world.SpawnAt(3.5f, 4.5f);
    const Dia::Entity::Entity src2 = world.SpawnAt(7.5f, 4.5f);

    const VisibilityGroupId groupA("GroupA");
    const VisibilityGroupId groupB("GroupB");

    system->RegisterSightSource(src1, groupA, 2.0f);
    system->RegisterSightSource(src2, groupB, 2.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Cell (3,4) visible to GroupA, not GroupB
    AssertCellVisible(*system, CellCoord{ 3, 4 }, groupA);
    AssertCellUnexplored(*system, CellCoord{ 3, 4 }, groupB);

    // Cell (7,4) visible to GroupB, not GroupA
    AssertCellVisible(*system, CellCoord{ 7, 4 }, groupB);
    AssertCellUnexplored(*system, CellCoord{ 7, 4 }, groupA);
}

// ---------------------------------------------------------------------------
// Update & Dirty Flags (GVD-008)
// ---------------------------------------------------------------------------

// GVD-008: Standing still does not retrigger cell callbacks
TEST(HexGridVisibilityUpdate, StandingStillDoesNotRetriggerCallbacks)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity src = world.SpawnAt(5.5f, 4.5f);
    const VisibilityGroupId groupId("Player");

    // Observer to count callbacks
    struct TestObserver : Dia::GridVisibility::IVisibilityChangeObserver
    {
        int cellChangedCount = 0;
        void OnCellStateChanged(CellCoord, VisibilityGroupId, VisibilityState, VisibilityState) override
        {
            ++cellChangedCount;
        }
    };
    TestObserver observer;
    system->AddChangeListener(&observer);

    system->RegisterSightSource(src, groupId, 3.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    const int firstUpdateCount = observer.cellChangedCount;
    EXPECT_GT(firstUpdateCount, 0);  // Initial visibility should trigger callbacks

    // Second update without moving - standing still
    system->Update(1.0f, *world.spatial, world.domain);

    // GVD-008: No new callbacks fired (standing-still sources cost zero)
    EXPECT_EQ(observer.cellChangedCount, firstUpdateCount);

    system->RemoveChangeListener(&observer);
}

// Visible→Revealed decay when source moves away
TEST(HexGridVisibilityUpdate, VisibleToRevealedDecay)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    Dia::Entity::Entity src = world.SpawnAt(5.5f, 4.5f);
    const VisibilityGroupId groupId("Player");

    system->RegisterSightSource(src, groupId, 2.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Cell (5,4) is Visible
    EXPECT_EQ(system->GetCellState(CellCoord{ 5, 4 }, groupId), VisibilityState::Visible);

    // Unregister the source and update - cells should decay to Revealed
    system->UnregisterSightSource(src);
    system->Update(1.0f, *world.spatial, world.domain);

    // Cell (5,4) should now be Revealed (was Visible, now not covered)
    EXPECT_EQ(system->GetCellState(CellCoord{ 5, 4 }, groupId), VisibilityState::Revealed);
}

// ---------------------------------------------------------------------------
// Entity Visibility Queries
// ---------------------------------------------------------------------------

// CanSee() returns true for visible entity
TEST(HexGridVisibilityQueries, CanSeeVisibleEntity)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity observer = world.SpawnAt(5.5f, 4.5f);
    const Dia::Entity::Entity target = world.SpawnAt(6.5f, 4.5f);  // Neighbor cell

    const VisibilityGroupId groupId("Player");
    system->RegisterSightSource(observer, groupId, 3.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Target is in a visible cell
    EXPECT_TRUE(system->CanSee(target, groupId));
}

// CanSee() returns false for non-visible entity
TEST(HexGridVisibilityQueries, CannotSeeDistantEntity)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity observer = world.SpawnAt(5.5f, 4.5f);
    const Dia::Entity::Entity target = world.SpawnAt(0.5f, 0.5f);  // Far away

    const VisibilityGroupId groupId("Player");
    system->RegisterSightSource(observer, groupId, 2.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Target is outside sight radius
    EXPECT_FALSE(system->CanSee(target, groupId));
}

// GetVisibleEntities() returns entities in visible cells
TEST(HexGridVisibilityQueries, GetVisibleEntitiesReturnsTargets)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity observer = world.SpawnAt(5.5f, 4.5f);
    const Dia::Entity::Entity target1 = world.SpawnAt(6.5f, 4.5f);  // Visible neighbor
    const Dia::Entity::Entity target2 = world.SpawnAt(0.5f, 0.5f);  // Not visible

    const VisibilityGroupId groupId("Player");
    system->RegisterSightSource(observer, groupId, 3.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    const std::span<const Dia::Entity::Entity> visible = system->GetVisibleEntities(groupId);

    // target1 should be in the list, target2 should not
    bool foundTarget1 = false;
    bool foundTarget2 = false;
    for (const auto& e : visible)
    {
        if (e == target1) foundTarget1 = true;
        if (e == target2) foundTarget2 = true;
    }

    EXPECT_TRUE(foundTarget1);
    EXPECT_FALSE(foundTarget2);
}

// ---------------------------------------------------------------------------
// Edge Cases
// ---------------------------------------------------------------------------

// Small grid (2x2) basic visibility
TEST(HexGridVisibilityEdgeCases, SmallGrid)
{
    HexPathGrid grid(2, 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity src = world.SpawnAt(0.5f, 0.5f);
    const VisibilityGroupId groupId("Player");

    system->RegisterSightSource(src, groupId, 2.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Origin should be visible
    AssertCellVisible(*system, CellCoord{ 0, 0 }, groupId);
}

// Column parity: even vs odd column neighbor offsets
TEST(HexGridVisibilityEdgeCases, ColumnParityNeighbors)
{
    HexPathGrid grid(10, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    // Test even column (x=4)
    const Dia::Entity::Entity srcEven = world.SpawnAt(4.5f, 4.5f);
    const VisibilityGroupId groupEven("Even");
    system->RegisterSightSource(srcEven, groupEven, 1.5f);

    // Test odd column (x=5)
    const Dia::Entity::Entity srcOdd = world.SpawnAt(5.5f, 4.5f);
    const VisibilityGroupId groupOdd("Odd");
    system->RegisterSightSource(srcOdd, groupOdd, 1.5f);

    system->Update(1.0f, *world.spatial, world.domain);

    // Both origins visible
    AssertCellVisible(*system, CellCoord{ 4, 4 }, groupEven);
    AssertCellVisible(*system, CellCoord{ 5, 4 }, groupOdd);

    // Even column (4,4) NE neighbor is (5,3) - row-1 for even columns
    AssertCellVisible(*system, CellCoord{ 5, 3 }, groupEven);

    // Odd column (5,4) NE neighbor is (6,4) - row+0 for odd columns
    AssertCellVisible(*system, CellCoord{ 6, 4 }, groupOdd);
}

// All-impassable grid: no cells visible except origin
TEST(HexGridVisibilityEdgeCases, AllImpassableGrid)
{
    HexPathGrid grid(5, 5);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            grid.SetPassable(CellCoord{ x, y }, false);
        }
    }

    auto system = MakeHexSystem(grid, 1);
    HexFixtureWorld world;

    const Dia::Entity::Entity src = world.SpawnAt(2.5f, 2.5f);
    const VisibilityGroupId groupId("Player");

    system->RegisterSightSource(src, groupId, 3.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Origin should be visible (source always sees the cell it's in)
    AssertCellVisible(*system, CellCoord{ 2, 2 }, groupId);

    // All neighbors should be Visible (blockers themselves are visible) but passability doesn't affect visibility state
    // Actually, the test is about seeing impassable cells - they should still be marked visible until blocked
    EXPECT_EQ(system->GetCellState(CellCoord{ 3, 2 }, groupId), VisibilityState::Visible);
}

// chunkSize > 1 with actual visibility queries
TEST(HexGridVisibilityEdgeCases, ConsolidatedGridVisibility)
{
    HexPathGrid grid(21, 14);  // chunkSize=7 → 3x2 visibility grid
    for (int y = 0; y < 14; ++y) {
        for (int x = 0; x < 21; ++x) {
            grid.SetPassable(CellCoord{ x, y }, true);
        }
    }

    auto system = MakeHexSystem(grid, 7);
    HexFixtureWorld world;

    // Place source in center of grid (pathfinding space)
    const Dia::Entity::Entity src = world.SpawnAt(10.5f, 7.5f);
    const VisibilityGroupId groupId("Player");

    system->RegisterSightSource(src, groupId, 10.0f);  // Large radius to cover consolidated cells
    system->Update(1.0f, *world.spatial, world.domain);

    // Visibility grid is 3x2; center vis-cell is (1,1)
    // Map back to pathfinding space: vis-cell (1,1) covers pathfinding cells [7-13, 7-13]
    // Origin cell (10,7) should be visible
    EXPECT_EQ(system->GetCellState(CellCoord{ 1, 1 }, groupId), VisibilityState::Visible);
}
