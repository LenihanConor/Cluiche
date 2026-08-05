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
