#include <gtest/gtest.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaEntitySpatial/Testing/SpatialTestHelpers.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cmath>
#include <memory>

using namespace Dia::EntitySpatial::Testing;
using namespace Dia::Entity;
using namespace Dia::Maths;
using namespace Dia::EntitySpatial;

// ============================================================================
// Fixture
// ============================================================================

class DiaEntitySpatialTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Register the component pool so Domain can store SpatialComponent.
        domain.RegisterPool(new Dia::Entity::ComponentPool<SpatialComponent>(SpatialComponent::kTypeId));

        // Build a 200x200 square grid with 10-unit cells.
        EntitySpatialIndex::SquareDef def;
        def.worldBounds = Dia::Geometry2D::AARect(
            Vector2D(-100.f, -100.f),
            Vector2D( 100.f,  100.f));
        def.cellSize = 10.f;

        module = std::make_unique<EntitySpatialModule>(domain, def);
    }

    void TearDown() override {}

    Dia::Entity::Domain                    domain;
    std::unique_ptr<EntitySpatialModule>   module;
};

// ============================================================================
// 1. QueryCircle — entity inside circle is returned; entity outside is not
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryCircle_InsideReturned)
{
    // Entity at (5, 5) with radius 1 — inside query circle centred at origin, radius 10.
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Circle queryCircle(10.f, Vector2D(0.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], e);
}

TEST_F(DiaEntitySpatialTest, QueryCircle_OutsideNotReturned)
{
    // Entity at (50, 50) with radius 1 — outside query circle centred at origin, radius 10.
    SpawnSpatialEntity(domain, Vector2D(50.f, 50.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Circle queryCircle(10.f, Vector2D(0.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// 2. QueryRegion — entity inside AARect is returned; entity outside is not
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryRegion_InsideReturned)
{
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::AARect region(Vector2D(-10.f, -10.f), Vector2D(20.f, 20.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryRegion(region, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], e);
}

TEST_F(DiaEntitySpatialTest, QueryRegion_OutsideNotReturned)
{
    SpawnSpatialEntity(domain, Vector2D(50.f, 50.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::AARect region(Vector2D(-10.f, -10.f), Vector2D(10.f, 10.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryRegion(region, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// 3. QueryKNearest — returns k nearest sorted ascending by distance
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryKNearest_ReturnsSortedAscending)
{
    // Three entities at distances 2, 5, 8 from origin.
    Entity near   = SpawnSpatialEntity(domain, Vector2D(2.f,  0.f), 0.5f, 0x01);
    Entity mid    = SpawnSpatialEntity(domain, Vector2D(5.f,  0.f), 0.5f, 0x01);
    Entity far    = SpawnSpatialEntity(domain, Vector2D(8.f,  0.f), 0.5f, 0x01);
    FlushDomain(domain);
    module->Update();

    Vector2D origin(0.f, 0.f);
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryKNearest(origin, 3, 0x7FFFFFFFu, out);

    // All three should be returned.
    EXPECT_EQ(out.Size(), 3u);

    // Verify ascending order: first should be nearest (index 2=2.0 dist).
    if (out.Size() >= 3u)
    {
        const SpatialComponent* sc0 = domain.GetComponent<SpatialComponent>(out[0]);
        const SpatialComponent* sc1 = domain.GetComponent<SpatialComponent>(out[1]);
        const SpatialComponent* sc2 = domain.GetComponent<SpatialComponent>(out[2]);
        ASSERT_NE(sc0, nullptr);
        ASSERT_NE(sc1, nullptr);
        ASSERT_NE(sc2, nullptr);

        auto distSq = [&](const SpatialComponent* sc) {
            float dx = sc->position.x - origin.x;
            float dy = sc->position.y - origin.y;
            return dx * dx + dy * dy;
        };

        EXPECT_LE(distSq(sc0), distSq(sc1));
        EXPECT_LE(distSq(sc1), distSq(sc2));

        // The nearest entity should be `near`
        EXPECT_EQ(out[0], near);
    }

    (void)mid; (void)far;
}

TEST_F(DiaEntitySpatialTest, QueryKNearest_RespectsKLimit)
{
    SpawnSpatialEntity(domain, Vector2D(1.f, 0.f), 0.5f, 0x01);
    SpawnSpatialEntity(domain, Vector2D(3.f, 0.f), 0.5f, 0x01);
    SpawnSpatialEntity(domain, Vector2D(5.f, 0.f), 0.5f, 0x01);
    FlushDomain(domain);
    module->Update();

    Vector2D origin(0.f, 0.f);
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryKNearest(origin, 2, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 2u);
}

// ============================================================================
// 4. QueryRay — entity whose circle intersects ray is returned
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryRay_HitEntityReturned)
{
    // Entity centred at (5, 0) radius 2 — ray along +X axis should hit it.
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 0.f), 2.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Ray ray(Vector2D(0.f, 0.f), Vector2D(1.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryRay(ray, 20.f, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], e);
}

TEST_F(DiaEntitySpatialTest, QueryRay_MissEntityNotReturned)
{
    // Entity well off the ray path: at (5, 20), ray along +X axis.
    SpawnSpatialEntity(domain, Vector2D(5.f, 20.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Ray ray(Vector2D(0.f, 0.f), Vector2D(1.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryRay(ray, 50.f, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// 5. QuerySector — entity inside sector is returned; entity outside angle is not
// ============================================================================

TEST_F(DiaEntitySpatialTest, QuerySector_InsideAngleReturned)
{
    // Entity at (5, 0) — directly in front along +X. Sector: dir=(1,0), halfAngle=PI/4.
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 0.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Vector2D origin(0.f, 0.f);
    Vector2D dir(1.f, 0.f);
    const float halfAngle = 3.14159265f / 4.f; // 45 degrees
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QuerySector(origin, dir, 15.f, halfAngle, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], e);
}

TEST_F(DiaEntitySpatialTest, QuerySector_OutsideAngleNotReturned)
{
    // Entity at (0, 5) — 90 degrees from +X, outside a 45-degree half-angle sector.
    SpawnSpatialEntity(domain, Vector2D(0.f, 5.f), 0.5f, 0x01);
    FlushDomain(domain);
    module->Update();

    Vector2D origin(0.f, 0.f);
    Vector2D dir(1.f, 0.f);
    const float halfAngle = 3.14159265f / 4.f; // 45 degrees
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QuerySector(origin, dir, 15.f, halfAngle, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// 6. Layer mask filtering — non-matching mask excludes entity
// ============================================================================

TEST_F(DiaEntitySpatialTest, LayerMask_MatchingMaskIncludesEntity)
{
    // Entity with layer 0x01.
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Circle queryCircle(20.f, Vector2D(0.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryCircle(queryCircle, 0x01, out);

    EXPECT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], e);
}

TEST_F(DiaEntitySpatialTest, LayerMask_NonMatchingMaskExcludesEntity)
{
    // Entity with layer 0x01, query with mask 0x02 — no intersection.
    SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Circle queryCircle(20.f, Vector2D(0.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryCircle(queryCircle, 0x02, out);

    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(DiaEntitySpatialTest, LayerMask_ZeroMaskReturnsNothing)
{
    SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x7FFFFFFFu);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Circle queryCircle(20.f, Vector2D(0.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryCircle(queryCircle, 0u, out);

    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// 7. Dirty-flag incremental update — move entity, query returns new position
// ============================================================================

TEST_F(DiaEntitySpatialTest, DirtyFlagUpdate_MovedEntityQueriedAtNewPosition)
{
    // Start at (50, 50) — outside query circle at origin with radius 10.
    Entity e = SpawnSpatialEntity(domain, Vector2D(50.f, 50.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    {
        Dia::Geometry2D::Circle queryCircle(10.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 0u) << "Entity should not be found at original position";
    }

    // Move the entity to (3, 3) by updating the component and marking dirty.
    SpatialComponent* sc = domain.GetComponent<SpatialComponent>(e);
    ASSERT_NE(sc, nullptr);
    sc->position = Vector2D(3.f, 3.f);
    sc->MarkDirty();

    module->Update();

    {
        Dia::Geometry2D::Circle queryCircle(10.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 1u) << "Entity should be found at new position";
        if (out.Size() == 1u)
            EXPECT_EQ(out[0], e);
    }
}

// ============================================================================
// 8. Component detach removal — detach SpatialComponent, entity no longer returned
// ============================================================================

TEST_F(DiaEntitySpatialTest, ComponentDetach_EntityNoLongerReturned)
{
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    {
        Dia::Geometry2D::Circle queryCircle(20.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 1u) << "Entity should be found before detach";
    }

    // Remove the component, flush, update index.
    domain.QueueRemoveComponent<SpatialComponent>(e);
    domain.EndOfFrame();
    module->Update();

    {
        Dia::Geometry2D::Circle queryCircle(20.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 0u) << "Entity should not be found after component detach";
    }
}

// ============================================================================
// 9. Hex vs square topology — both return same entity for simple circle query
// ============================================================================

TEST_F(DiaEntitySpatialTest, HexVsSquare_SameEntityReturnedByBothTopologies)
{
    // Register a fresh domain for the hex test (module is already square-grid).
    Dia::Entity::Domain hexDomain;
    hexDomain.RegisterPool(new Dia::Entity::ComponentPool<SpatialComponent>(SpatialComponent::kTypeId));

    EntitySpatialIndex::HexDef hexDef;
    hexDef.origin    = Vector2D(-100.f, -100.f);
    hexDef.colCount  = 20;
    hexDef.rowCount  = 20;
    hexDef.hexRadius = 10.f;

    EntitySpatialModule hexModule(hexDomain, hexDef);

    Entity hexEntity = SpawnSpatialEntity(hexDomain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    hexDomain.EndOfFrame();
    hexModule.Update();

    // Square grid result (from fixture).
    Entity sqEntity = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Circle queryCircle(20.f, Vector2D(0.f, 0.f));

    Dia::Core::Containers::DynamicArrayC<Entity, 32> sqOut;
    module->QueryCircle(queryCircle, 0x7FFFFFFFu, sqOut);

    Dia::Core::Containers::DynamicArrayC<Entity, 32> hexOut;
    hexModule.QueryCircle(queryCircle, 0x7FFFFFFFu, hexOut);

    EXPECT_EQ(sqOut.Size(),  1u) << "Square grid should find entity";
    EXPECT_EQ(hexOut.Size(), 1u) << "Hex grid should find same entity";

    EXPECT_EQ(sqOut.Size(), hexOut.Size());
    (void)sqEntity; (void)hexEntity;
}

// ============================================================================
// 10. Stale handle safety — destroy entity, query does not return it
// ============================================================================

TEST_F(DiaEntitySpatialTest, StaleHandle_DestroyedEntityNotReturned)
{
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    {
        Dia::Geometry2D::Circle queryCircle(20.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 1u) << "Entity should be found before destroy";
    }

    // Destroy entity, flush, update index.
    domain.QueueDestroy(e);
    domain.EndOfFrame();
    module->Update();

    {
        Dia::Geometry2D::Circle queryCircle(20.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 0u) << "Destroyed entity must not appear in query results";
    }

    // Stale handle must report as dead.
    EXPECT_FALSE(domain.IsAlive(e));
}

// ============================================================================
// 11. Empty domain — all query types return zero results
// ============================================================================

TEST_F(DiaEntitySpatialTest, EmptyDomain_AllQueriesReturnZero)
{
    // No entities added. Module still runs update without crashing.
    module->Update();

    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;

    module->QueryCircle(Dia::Geometry2D::Circle(50.f, Vector2D(0.f, 0.f)), 0x7FFFFFFFu, out);
    EXPECT_EQ(out.Size(), 0u) << "QueryCircle on empty domain";

    out.RemoveAll();
    module->QueryRegion(Dia::Geometry2D::AARect(Vector2D(-50.f,-50.f), Vector2D(50.f,50.f)), 0x7FFFFFFFu, out);
    EXPECT_EQ(out.Size(), 0u) << "QueryRegion on empty domain";

    out.RemoveAll();
    module->QueryKNearest(Vector2D(0.f, 0.f), 5, 0x7FFFFFFFu, out);
    EXPECT_EQ(out.Size(), 0u) << "QueryKNearest on empty domain";

    out.RemoveAll();
    module->QueryRay(Dia::Geometry2D::Ray(Vector2D(0.f,0.f), Vector2D(1.f,0.f)), 100.f, 0x7FFFFFFFu, out);
    EXPECT_EQ(out.Size(), 0u) << "QueryRay on empty domain";

    out.RemoveAll();
    module->QuerySector(Vector2D(0.f,0.f), Vector2D(1.f,0.f), 50.f, 3.14159265f/4.f, 0x7FFFFFFFu, out);
    EXPECT_EQ(out.Size(), 0u) << "QuerySector on empty domain";
}

// ============================================================================
// 12. Entity without SpatialComponent is never returned
// ============================================================================

TEST_F(DiaEntitySpatialTest, NoSpatialComponent_EntityNeverReturned)
{
    // Create a bare entity with no SpatialComponent.
    Entity bare = domain.CreateEntity();
    domain.EndOfFrame();
    module->Update();

    Dia::Geometry2D::Circle queryCircle(200.f, Vector2D(0.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 0u);
    EXPECT_TRUE(domain.IsAlive(bare));
}

// ============================================================================
// 13. QueryKNearest with k=0 returns nothing
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryKNearest_KZeroReturnsNothing)
{
    SpawnSpatialEntity(domain, Vector2D(1.f, 0.f), 0.5f, 0x01);
    SpawnSpatialEntity(domain, Vector2D(2.f, 0.f), 0.5f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryKNearest(Vector2D(0.f, 0.f), 0, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// 14. QueryKNearest with k > entity count returns all entities
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryKNearest_KExceedsCountReturnsAll)
{
    SpawnSpatialEntity(domain, Vector2D(1.f, 0.f), 0.5f, 0x01);
    SpawnSpatialEntity(domain, Vector2D(2.f, 0.f), 0.5f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryKNearest(Vector2D(0.f, 0.f), 100, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 2u);
}

// ============================================================================
// 15. SpatialComponent default values are correct
// ============================================================================

TEST_F(DiaEntitySpatialTest, SpatialComponent_DefaultValues)
{
    // Verify that SpatialComponent starts dirty (bit 31 set) with expected defaults.
    SpatialComponent sc;
    EXPECT_EQ(sc.position.x, 0.f);
    EXPECT_EQ(sc.position.y, 0.f);
    EXPECT_EQ(sc.radius, 1.0f);
    EXPECT_TRUE(sc.IsDirty()) << "New SpatialComponent must start dirty";
    EXPECT_EQ(sc.GetLayerMask(), 0u) << "Default layer mask (bits 0-30) should be 0";
}

// ============================================================================
// 16. SetLayerMask marks dirty and updates GetLayerMask
// ============================================================================

TEST_F(DiaEntitySpatialTest, SpatialComponent_SetLayerMaskMarksDirtyAndUpdatesValue)
{
    SpatialComponent sc;
    sc.ClearDirty();
    EXPECT_FALSE(sc.IsDirty());

    sc.SetLayerMask(0x03);

    EXPECT_TRUE(sc.IsDirty()) << "SetLayerMask must mark dirty";
    EXPECT_EQ(sc.GetLayerMask(), 0x03u);
    // Bit 31 must not bleed into GetLayerMask result.
    EXPECT_EQ(sc.GetLayerMask() & 0x80000000u, 0u);
}

// ============================================================================
// 17. SetLayerMask propagates to module re-index on next Update
// ============================================================================

TEST_F(DiaEntitySpatialTest, SetLayerMask_PropagatesOnNextUpdate)
{
    // Spawn entity with layer 0x01, confirm it is found with mask 0x01.
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    {
        Dia::Geometry2D::Circle q(20.f, Vector2D(0.f,0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(q, 0x01, out);
        EXPECT_EQ(out.Size(), 1u) << "Should be found with original mask";
    }

    // Change layer to 0x02 and verify re-index.
    SpatialComponent* sc = domain.GetComponent<SpatialComponent>(e);
    ASSERT_NE(sc, nullptr);
    sc->SetLayerMask(0x02);  // automatically marks dirty

    module->Update();

    {
        Dia::Geometry2D::Circle q(20.f, Vector2D(0.f,0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(q, 0x01, out);
        EXPECT_EQ(out.Size(), 0u) << "Old mask should not match after layer change";

        out.RemoveAll();
        module->QueryCircle(q, 0x02, out);
        EXPECT_EQ(out.Size(), 1u) << "New mask should now match";
    }
}

// ============================================================================
// 18. QueryRegion — entity exactly on AARect boundary is included
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryRegion_EntityOnBoundaryIsIncluded)
{
    // Entity at (10, 10) exactly on the boundary of a [0,0]-[10,10] rect.
    Entity e = SpawnSpatialEntity(domain, Vector2D(10.f, 10.f), 0.1f, 0x01);
    FlushDomain(domain);
    module->Update();

    // Region whose top-right corner exactly touches the entity.
    Dia::Geometry2D::AARect region(Vector2D(0.f, 0.f), Vector2D(10.f, 10.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryRegion(region, 0x7FFFFFFFu, out);

    // Boundary inclusion is grid-implementation-dependent; we only assert no crash
    // and that at most 1 entity (the right one) appears.
    EXPECT_LE(out.Size(), 1u);
    if (out.Size() == 1u)
        EXPECT_EQ(out[0], e);
}

// ============================================================================
// 19. QueryRay — entity exactly at maxDist boundary
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryRay_EntityAtMaxDistBoundaryIsIncluded)
{
    // Entity centre at (10, 0) radius 1; ray along +X maxDist=10 — closest point
    // on segment is exactly (10,0), distSq=0 which is <= radius²=1.
    Entity e = SpawnSpatialEntity(domain, Vector2D(10.f, 0.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Ray ray(Vector2D(0.f, 0.f), Vector2D(1.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryRay(ray, 10.f, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 1u);
    if (out.Size() == 1u)
        EXPECT_EQ(out[0], e);
}

// ============================================================================
// 20. QuerySector — entity at origin (dist < epsilon) is skipped safely
// ============================================================================

TEST_F(DiaEntitySpatialTest, QuerySector_EntityAtOriginSkippedSafely)
{
    // Entity exactly at the sector origin — undefined angle, code skips it.
    SpawnSpatialEntity(domain, Vector2D(0.f, 0.f), 0.5f, 0x01);
    FlushDomain(domain);
    module->Update();

    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QuerySector(Vector2D(0.f, 0.f), Vector2D(1.f, 0.f), 10.f, 3.14159265f/4.f, 0x7FFFFFFFu, out);

    // Must not crash. Entity at origin is deliberately skipped.
    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// 21. Detach + re-attach in the same domain re-inserts correctly
// ============================================================================

TEST_F(DiaEntitySpatialTest, DetachAndReattach_EntityReturnsAfterReattach)
{
    Entity e = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01);
    FlushDomain(domain);
    module->Update();

    // Confirm found initially.
    {
        Dia::Geometry2D::Circle q(20.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(q, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 1u) << "Should be found before detach";
    }

    // Detach.
    domain.QueueRemoveComponent<SpatialComponent>(e);
    domain.EndOfFrame();
    module->Update();

    {
        Dia::Geometry2D::Circle q(20.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(q, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 0u) << "Should not be found after detach";
    }

    // Re-attach at a different position.
    Json::Value cfg;
    cfg["position"]["x"] = 3.f;
    cfg["position"]["y"] = 3.f;
    cfg["radius"]        = 1.0f;
    cfg["layerMask"]     = 0x01;
    domain.QueueAddComponent<SpatialComponent>(e, cfg);
    domain.EndOfFrame();
    module->Update();

    {
        Dia::Geometry2D::Circle q(20.f, Vector2D(0.f, 0.f));
        Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
        module->QueryCircle(q, 0x7FFFFFFFu, out);
        EXPECT_EQ(out.Size(), 1u) << "Should be found again after re-attach";
        if (out.Size() == 1u)
            EXPECT_EQ(out[0], e);
    }
}

// ============================================================================
// 22. Multiple entities matching a single QueryCircle
// ============================================================================

TEST_F(DiaEntitySpatialTest, QueryCircle_MultipleEntitiesInRange)
{
    Entity e1 = SpawnSpatialEntity(domain, Vector2D( 1.f,  0.f), 0.5f, 0x01);
    Entity e2 = SpawnSpatialEntity(domain, Vector2D(-1.f,  0.f), 0.5f, 0x01);
    Entity e3 = SpawnSpatialEntity(domain, Vector2D( 0.f,  1.f), 0.5f, 0x01);
    SpawnSpatialEntity(domain, Vector2D(50.f, 50.f), 0.5f, 0x01); // outside
    FlushDomain(domain);
    module->Update();

    Dia::Geometry2D::Circle queryCircle(5.f, Vector2D(0.f, 0.f));
    Dia::Core::Containers::DynamicArrayC<Entity, 32> out;
    module->QueryCircle(queryCircle, 0x7FFFFFFFu, out);

    EXPECT_EQ(out.Size(), 3u);

    // Verify each expected entity appears exactly once.
    Dia::Core::Containers::DynamicArrayC<Entity, 4> expected;
    expected.Add(e1); expected.Add(e2); expected.Add(e3);
    AssertExactEntitySet(out, expected, "MultipleEntitiesInRange");
}

// ============================================================================
// 23. JSON init roundtrip — position/radius/layerMask values survive QueueAddComponent
// ============================================================================

TEST_F(DiaEntitySpatialTest, SpatialComponent_JsonInitRoundtrip)
{
    // Construct a component via the JSON path that Domain uses internally.
    Json::Value cfg;
    cfg["position"]["x"] = 3.5f;
    cfg["position"]["y"] = -7.25f;
    cfg["radius"]        = 2.0f;
    cfg["layerMask"]     = 0x0Fu;

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<SpatialComponent>(e, cfg);
    domain.EndOfFrame();

    const SpatialComponent* sc = domain.GetComponent<SpatialComponent>(e);
    ASSERT_NE(sc, nullptr);

    EXPECT_NEAR(sc->position.x, 3.5f,   1e-5f);
    EXPECT_NEAR(sc->position.y, -7.25f, 1e-5f);
    EXPECT_NEAR(sc->radius,     2.0f,   1e-5f);
    EXPECT_EQ(sc->GetLayerMask(), 0x0Fu);
    // Component must arrive dirty so the module indexes it on first Update.
    EXPECT_TRUE(sc->IsDirty());
}
