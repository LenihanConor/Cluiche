// TestGridVisibilitySystem.cpp — GoogleTest coverage for GridVisibilitySystem construction
// (chunk-grid consolidation) and sight-source registration.
//
// Suites: GridVisibilityConcept, GridVisibilityConstruction, GridVisibilityRegistration

#include <gtest/gtest.h>
#include <memory>

#include "DiaGridVisibility/VisibilityGroupId.h"
#include "DiaGridVisibility/VisibilityState.h"
#include "DiaGridVisibility/VisibilityLogChannel.h"
#include "DiaGridVisibility/IVisibilityChangeObserver.h"
#include "DiaGridVisibility/GridVisibilitySystem.h"
#include "DiaGridVisibility/Testing/VisibilityTestHelpers.h"

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

using Dia::GridVisibility::CVisibilityGraph;
using Dia::GridVisibility::GridVisibilitySystem;
using Dia::GridVisibility::VisibilityGroupId;
using Dia::GridVisibility::VisibilityState;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::SquarePathGrid;
using Dia::GridVisibility::Testing::MockVisibilityGraph;
using Dia::GridVisibility::Testing::AssertCellVisible;
using Dia::GridVisibility::Testing::AssertCellUnexplored;
using Dia::GridVisibility::Testing::AssertCanSee;
using Dia::GridVisibility::Testing::AssertCannotSee;

using VisSystem = GridVisibilitySystem<SquarePathGrid>;

namespace
{
    // GridVisibilitySystem owns large fixed-capacity arrays — always heap-allocate.
    std::unique_ptr<VisSystem> MakeSystem(const SquarePathGrid& grid, int chunkSize)
    {
        return std::make_unique<VisSystem>(grid, chunkSize);
    }

    Dia::Entity::Entity MakeEntity(uint32_t index)
    {
        return Dia::Entity::Entity(index, 1u);
    }
}

// ---------------------------------------------------------------------------
// Concept
// ---------------------------------------------------------------------------
// GridVisibilitySystem is ~1.28 MB of fixed-capacity storage (kMaxSightSources x
// kMaxShadowCells dominates at ~1.02 MB). It MUST NOT be stack-allocated — the default
// MSVC stack is 1 MB. This guard catches accidental growth of the internal capacities.
TEST(GridVisibilityConcept, FootprintStaysWithinBudget)
{
    constexpr size_t kBudgetBytes = 1536u * 1024u;   // 1.5 MB
    EXPECT_LE(sizeof(VisSystem), kBudgetBytes);
}

TEST(GridVisibilityConcept, SquarePathGridSatisfiesCVisibilityGraph)
{
    static_assert(CVisibilityGraph<SquarePathGrid>,
                  "SquarePathGrid must satisfy CVisibilityGraph");
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Construction / consolidation
// ---------------------------------------------------------------------------
TEST(GridVisibilityConstruction, ChunkSizeOneMirrorsPathGrid)
{
    SquarePathGrid grid(8, 6);
    grid.SetPassable(CellCoord{ 3, 2 }, false);

    auto system = MakeSystem(grid, 1);

    EXPECT_EQ(system->GetVisWidth(), 8);
    EXPECT_EQ(system->GetVisHeight(), 6);
    EXPECT_EQ(system->GetChunkSize(), 1);

    for (int y = 0; y < 6; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            EXPECT_EQ(system->IsVisCellPassable(CellCoord{ x, y }),
                      grid.IsPassable(CellCoord{ x, y })) << "cell " << x << "," << y;
        }
    }
}

TEST(GridVisibilityConstruction, ChunkSizeTwoHalvesTheGrid)
{
    SquarePathGrid grid(8, 6);
    auto system = MakeSystem(grid, 2);

    EXPECT_EQ(system->GetVisWidth(), 4);
    EXPECT_EQ(system->GetVisHeight(), 3);
    EXPECT_EQ(system->GetChunkSize(), 2);
}

TEST(GridVisibilityConstruction, PartialChunkAtEdgeStillProducesACell)
{
    // 5x5 at chunkSize 2 -> ceil(5/2) = 3 vis cells per side.
    SquarePathGrid grid(5, 5);
    auto system = MakeSystem(grid, 2);

    EXPECT_EQ(system->GetVisWidth(), 3);
    EXPECT_EQ(system->GetVisHeight(), 3);
    EXPECT_TRUE(system->IsVisCellPassable(CellCoord{ 2, 2 }));
}

TEST(GridVisibilityConstruction, VisCellIsPassableIfAnyPathCellInChunkIsPassable)
{
    SquarePathGrid grid(4, 4);

    // Block the whole 0,0 chunk (2x2) -> vis cell 0,0 impassable.
    grid.SetPassable(CellCoord{ 0, 0 }, false);
    grid.SetPassable(CellCoord{ 1, 0 }, false);
    grid.SetPassable(CellCoord{ 0, 1 }, false);
    grid.SetPassable(CellCoord{ 1, 1 }, false);

    // Block only 3 of the 4 cells in chunk 1,0 -> vis cell 1,0 stays passable.
    grid.SetPassable(CellCoord{ 2, 0 }, false);
    grid.SetPassable(CellCoord{ 3, 0 }, false);
    grid.SetPassable(CellCoord{ 2, 1 }, false);

    auto system = MakeSystem(grid, 2);

    EXPECT_FALSE(system->IsVisCellPassable(CellCoord{ 0, 0 }));
    EXPECT_TRUE(system->IsVisCellPassable(CellCoord{ 1, 0 }));
    EXPECT_TRUE(system->IsVisCellPassable(CellCoord{ 0, 1 }));
}

TEST(GridVisibilityConstruction, OutOfBoundsVisCellIsNotPassable)
{
    SquarePathGrid grid(4, 4);
    auto system = MakeSystem(grid, 1);

    EXPECT_FALSE(system->IsVisCellPassable(CellCoord{ -1, 0 }));
    EXPECT_FALSE(system->IsVisCellPassable(CellCoord{ 0, -1 }));
    EXPECT_FALSE(system->IsVisCellPassable(CellCoord{ 4, 0 }));
    EXPECT_FALSE(system->IsVisCellPassable(CellCoord{ 0, 4 }));
}

TEST(GridVisibilityConstruction, NoGroupsBeforeAnyRegistration)
{
    SquarePathGrid grid(4, 4);
    auto system = MakeSystem(grid, 1);

    EXPECT_EQ(system->GetGroupCount(), 0);
}

// ---------------------------------------------------------------------------
// Sight-source registration
// ---------------------------------------------------------------------------
TEST(GridVisibilityRegistration, FirstRegistrationCreatesGroup)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    system->RegisterSightSource(MakeEntity(1), VisibilityGroupId("Blue"), 5.0f);

    EXPECT_EQ(system->GetGroupCount(), 1);
}

TEST(GridVisibilityRegistration, SecondSourceInSameGroupDoesNotCreateAnotherGroup)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    system->RegisterSightSource(MakeEntity(1), VisibilityGroupId("Blue"), 5.0f);
    system->RegisterSightSource(MakeEntity(2), VisibilityGroupId("Blue"), 7.0f);

    EXPECT_EQ(system->GetGroupCount(), 1);
}

TEST(GridVisibilityRegistration, DistinctGroupsAreTrackedSeparately)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    system->RegisterSightSource(MakeEntity(1), VisibilityGroupId("Blue"), 5.0f);
    system->RegisterSightSource(MakeEntity(2), VisibilityGroupId("Red"), 5.0f);

    EXPECT_EQ(system->GetGroupCount(), 2);
}

TEST(GridVisibilityRegistration, ReRegisteringSameEntityUpdatesRatherThanDuplicates)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    const Dia::Entity::Entity entity = MakeEntity(1);
    system->RegisterSightSource(entity, VisibilityGroupId("Blue"), 5.0f);
    system->RegisterSightSource(entity, VisibilityGroupId("Blue"), 9.0f);

    EXPECT_EQ(system->GetGroupCount(), 1);
}

TEST(GridVisibilityRegistration, ReRegisteringWithNewGroupCreatesTheNewGroup)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    const Dia::Entity::Entity entity = MakeEntity(1);
    system->RegisterSightSource(entity, VisibilityGroupId("Blue"), 5.0f);
    system->RegisterSightSource(entity, VisibilityGroupId("Red"), 5.0f);

    EXPECT_EQ(system->GetGroupCount(), 2);
}

TEST(GridVisibilityRegistration, UnregisterKeepsGroupButDropsTheSource)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    const Dia::Entity::Entity entity = MakeEntity(1);
    system->RegisterSightSource(entity, VisibilityGroupId("Blue"), 5.0f);
    system->UnregisterSightSource(entity);

    // Groups persist so previously revealed terrain is not forgotten.
    EXPECT_EQ(system->GetGroupCount(), 1);
}

TEST(GridVisibilityRegistration, UnregisterUnknownEntityIsANoOp)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    system->UnregisterSightSource(MakeEntity(42));

    EXPECT_EQ(system->GetGroupCount(), 0);
}

TEST(GridVisibilityRegistration, QueryStubsReturnDefaultsBeforeUpdateIsImplemented)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    const VisibilityGroupId blue("Blue");
    system->RegisterSightSource(MakeEntity(1), blue, 5.0f);

    EXPECT_FALSE(system->CanSee(MakeEntity(2), blue));
    EXPECT_EQ(system->GetCellState(CellCoord{ 0, 0 }, blue), VisibilityState::Unexplored);
    EXPECT_TRUE(system->GetVisibleEntities(blue).empty());
}

// ---------------------------------------------------------------------------
// Update pass — smoke coverage only.
//
// The query accessors (CanSee / GetCellState / GetVisibleEntities) are still
// Task 5 stubs, so behaviour is observed through IVisibilityChangeObserver.
// Full LOS / decay / entity-list assertions land with Tasks 7-8. These tests
// exist so the Update() template body is actually instantiated and compiled.
// ---------------------------------------------------------------------------
namespace
{
    class RecordingObserver final : public Dia::GridVisibility::IVisibilityChangeObserver
    {
    public:
        void OnCellStateChanged(CellCoord, VisibilityGroupId,
                                VisibilityState, VisibilityState newState) override
        {
            ++mCellChanges;
            if (newState == VisibilityState::Visible)   { ++mBecameVisible; }
            if (newState == VisibilityState::Revealed)  { ++mBecameRevealed; }
        }

        void OnEntityVisibilityChanged(Dia::Entity::Entity, VisibilityGroupId,
                                       bool isNowVisible) override
        {
            if (isNowVisible) { ++mEntityAppeared; }
            else              { ++mEntityDisappeared; }
        }

        int mCellChanges       = 0;
        int mBecameVisible     = 0;
        int mBecameRevealed    = 0;
        int mEntityAppeared    = 0;
        int mEntityDisappeared = 0;
    };

    // Domain + spatial module wired for a 64x64 world of 1-unit cells.
    struct VisFixtureWorld
    {
        VisFixtureWorld()
        {
            domain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpatial::SpatialComponent>(
                Dia::EntitySpatial::SpatialComponent::kTypeId));

            Dia::EntitySpatial::EntitySpatialIndex::SquareDef def;
            def.worldBounds = Dia::Geometry2D::AARect(Dia::Maths::Vector2D(0.f, 0.f),
                                                      Dia::Maths::Vector2D(64.f, 64.f));
            def.cellSize    = 4.f;

            spatial = std::make_unique<Dia::EntitySpatial::EntitySpatialModule>(domain, def);
        }

        Dia::Entity::Domain                                          domain;
        std::unique_ptr<Dia::EntitySpatial::EntitySpatialModule>     spatial;
    };
}

TEST(GridVisibilityUpdate, EmptySystemUpdateIsANoOp)
{
    SquarePathGrid grid(16, 16);
    auto system = MakeSystem(grid, 1);

    VisFixtureWorld world;
    RecordingObserver observer;
    system->AddChangeListener(&observer);

    system->Update(1.0f, *world.spatial, world.domain);

    EXPECT_EQ(observer.mCellChanges, 0);
    EXPECT_EQ(observer.mEntityAppeared, 0);
}

TEST(GridVisibilityUpdate, SightSourceMakesCellsVisible)
{
    SquarePathGrid grid(16, 16);
    auto system = MakeSystem(grid, 1);

    VisFixtureWorld world;
    const Dia::Entity::Entity observerEntity =
        Dia::EntitySpatial::Testing::SpawnSpatialEntity(world.domain,
                                                        Dia::Maths::Vector2D(8.5f, 8.5f));
    Dia::EntitySpatial::Testing::FlushDomain(world.domain);
    world.spatial->Update();

    RecordingObserver observer;
    system->AddChangeListener(&observer);
    system->RegisterSightSource(observerEntity, VisibilityGroupId("Blue"), 4.0f);

    system->Update(1.0f, *world.spatial, world.domain);

    // An unobstructed 4-cell radius must light up more than just the origin cell.
    EXPECT_GT(observer.mBecameVisible, 1);
}

TEST(GridVisibilityUpdate, StandingStillDoesNotRefireCellCallbacks)
{
    SquarePathGrid grid(16, 16);
    auto system = MakeSystem(grid, 1);

    VisFixtureWorld world;
    const Dia::Entity::Entity observerEntity =
        Dia::EntitySpatial::Testing::SpawnSpatialEntity(world.domain,
                                                        Dia::Maths::Vector2D(8.5f, 8.5f));
    Dia::EntitySpatial::Testing::FlushDomain(world.domain);
    world.spatial->Update();

    system->RegisterSightSource(observerEntity, VisibilityGroupId("Blue"), 4.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // Attach only now: the second tick must produce no cell transitions at all
    // because the source is not dirty and coverage is unchanged (GVD-008).
    RecordingObserver observer;
    system->AddChangeListener(&observer);
    system->Update(1.0f, *world.spatial, world.domain);

    EXPECT_EQ(observer.mCellChanges, 0);
}

TEST(GridVisibilityUpdate, EntityAppearsWhenCellBecomesVisible)
{
    SquarePathGrid grid(16, 16);
    auto system = MakeSystem(grid, 1);

    VisFixtureWorld world;
    // The observer entity is also a spatial entity — it should appear in its own group's
    // visible-entity list once Update illuminates the cell it stands on.
    const Dia::Entity::Entity observerEntity =
        Dia::EntitySpatial::Testing::SpawnSpatialEntity(world.domain,
                                                        Dia::Maths::Vector2D(8.5f, 8.5f));
    Dia::EntitySpatial::Testing::FlushDomain(world.domain);
    world.spatial->Update();

    RecordingObserver observer;
    system->AddChangeListener(&observer);
    system->RegisterSightSource(observerEntity, VisibilityGroupId("Blue"), 4.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // At least the observer entity itself should have transitioned to visible.
    EXPECT_GT(observer.mEntityAppeared, 0);
}

TEST(GridVisibilityUpdate, RemovedListenerStopsReceivingCallbacks)
{
    SquarePathGrid grid(16, 16);
    auto system = MakeSystem(grid, 1);

    VisFixtureWorld world;
    const Dia::Entity::Entity observerEntity =
        Dia::EntitySpatial::Testing::SpawnSpatialEntity(world.domain,
                                                        Dia::Maths::Vector2D(8.5f, 8.5f));
    Dia::EntitySpatial::Testing::FlushDomain(world.domain);
    world.spatial->Update();

    RecordingObserver observer;
    system->AddChangeListener(&observer);
    system->RemoveChangeListener(&observer);

    system->RegisterSightSource(observerEntity, VisibilityGroupId("Blue"), 4.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    EXPECT_EQ(observer.mCellChanges, 0);
}

// ---------------------------------------------------------------------------
// Query accessor coverage — GetCellState / GetVisibleEntities / CanSee
// ---------------------------------------------------------------------------
TEST(GridVisibilityQueries, GetCellState_UnknownGroup_ReturnsUnexplored)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    // No group was ever registered — must return Unexplored without crashing.
    EXPECT_EQ(system->GetCellState(CellCoord{ 0, 0 }, VisibilityGroupId("Ghost")),
              VisibilityState::Unexplored);
}

TEST(GridVisibilityQueries, GetCellState_OutOfBoundsCell_ReturnsUnexplored)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    const VisibilityGroupId blue("Blue");
    system->RegisterSightSource(MakeEntity(1), blue, 3.0f);

    // Vis grid is 8x8; coordinates outside [0,8) must return Unexplored.
    EXPECT_EQ(system->GetCellState(CellCoord{ -1,  0 }, blue), VisibilityState::Unexplored);
    EXPECT_EQ(system->GetCellState(CellCoord{  0, -1 }, blue), VisibilityState::Unexplored);
    EXPECT_EQ(system->GetCellState(CellCoord{  8,  0 }, blue), VisibilityState::Unexplored);
    EXPECT_EQ(system->GetCellState(CellCoord{  0,  8 }, blue), VisibilityState::Unexplored);
}

TEST(GridVisibilityQueries, GetCellState_AfterUpdate_VisibleUnderSource)
{
    // 16x16 grid, chunkSize 1 — vis grid == path grid.
    // Source placed at world (8.5, 8.5), cellSize 1.0 → vis cell (8, 8).
    SquarePathGrid grid(16, 16);
    auto system = MakeSystem(grid, 1);

    VisFixtureWorld world;
    const Dia::Entity::Entity sourceEntity =
        Dia::EntitySpatial::Testing::SpawnSpatialEntity(world.domain,
                                                        Dia::Maths::Vector2D(8.5f, 8.5f));
    Dia::EntitySpatial::Testing::FlushDomain(world.domain);
    world.spatial->Update();

    const VisibilityGroupId blue("Blue");
    system->RegisterSightSource(sourceEntity, blue, 4.0f);
    system->Update(1.0f, *world.spatial, world.domain);

    // The cell directly under the source must be Visible.
    EXPECT_EQ(system->GetCellState(CellCoord{ 8, 8 }, blue), VisibilityState::Visible);
}

TEST(GridVisibilityQueries, GetVisibleEntities_NoGroup_ReturnsEmptySpan)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    // Group was never registered — span must be empty.
    EXPECT_TRUE(system->GetVisibleEntities(VisibilityGroupId("Ghost")).empty());
}

TEST(GridVisibilityQueries, CanSee_EntityNotVisible_ReturnsFalse)
{
    SquarePathGrid grid(8, 8);
    auto system = MakeSystem(grid, 1);

    const VisibilityGroupId blue("Blue");
    system->RegisterSightSource(MakeEntity(1), blue, 3.0f);

    // No Update() called — visibleEntities list is empty for the group.
    EXPECT_FALSE(system->CanSee(MakeEntity(2), blue));
}

// ---------------------------------------------------------------------------
// MockVisibilityGraph + assert helper coverage (Task 6)
//
// These tests instantiate GridVisibilitySystem<MockVisibilityGraph> and
// exercise all five assert helpers, proving that VisibilityTestHelpers.h
// compiles and the static_assert(CVisibilityGraph<MockVisibilityGraph>) passes.
// ---------------------------------------------------------------------------
TEST(GridVisibilityMock, MockGraphSatisfiesConcept)
{
    // static_assert inside VisibilityTestHelpers.h already guards this at compile time.
    // This runtime test documents and double-checks the 8x8 default size.
    MockVisibilityGraph mock;
    EXPECT_EQ(mock.GetWidth(),  MockVisibilityGraph::kDefaultWidth);
    EXPECT_EQ(mock.GetHeight(), MockVisibilityGraph::kDefaultHeight);
    EXPECT_TRUE(mock.IsPassable(CellCoord{ 0, 0 }));
    EXPECT_TRUE(mock.IsPassable(CellCoord{ 7, 7 }));
    EXPECT_FALSE(mock.IsPassable(CellCoord{ 8, 0 }));   // out of bounds
}

TEST(GridVisibilityMock, AssertHelpers_BeforeUpdate_AllUnexplored)
{
    MockVisibilityGraph mock(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<MockVisibilityGraph>>(mock, 1);

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(MakeEntity(1), blue, 3.0f);

    // No Update called — every cell must be Unexplored and no entity visible.
    AssertCellUnexplored(*sys, CellCoord{ 0, 0 }, blue);
    AssertCellUnexplored(*sys, CellCoord{ 8, 8 }, blue);
    AssertCannotSee(*sys, MakeEntity(2), blue);
}

TEST(GridVisibilityMock, AssertCellVisible_AfterUpdate)
{
    // Full round-trip using MockVisibilityGraph as the graph type.
    // Entity at (8.5, 8.5), cellSize=1 → vis cell (8,8) must be Visible.
    MockVisibilityGraph mock(16, 16);
    auto sys = std::make_unique<GridVisibilitySystem<MockVisibilityGraph>>(mock, 1);

    VisFixtureWorld world;
    const Dia::Entity::Entity src =
        Dia::EntitySpatial::Testing::SpawnSpatialEntity(world.domain,
                                                        Dia::Maths::Vector2D(8.5f, 8.5f));
    Dia::EntitySpatial::Testing::FlushDomain(world.domain);
    world.spatial->Update();

    const VisibilityGroupId blue("Blue");
    sys->RegisterSightSource(src, blue, 4.0f);
    sys->Update(1.0f, *world.spatial, world.domain);

    AssertCellVisible(*sys, CellCoord{ 8, 8 }, blue);
    AssertCanSee(*sys, src, blue);
}
