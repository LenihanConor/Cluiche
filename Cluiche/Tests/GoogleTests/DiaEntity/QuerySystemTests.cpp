#include <gtest/gtest.h>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/Entity.h>
#include <diaentitytemplate/IComponent.h>
#include <diaentitytemplate/ComponentMacros.h>
#include <diaentitytemplate/ComponentPool.h>
#include <diaentitytemplate/QueryView.h>

using namespace Dia::Entity;

// ============================================================
// Test components for query tests
// ============================================================

namespace DiaQueryTest {

    class QCompA : public Dia::Entity::IComponent {
        DIA_COMPONENT(QCompA, "qcomp-a", 1)
        FIELD(int32_t, value, 0)
    };

    class QCompB : public Dia::Entity::IComponent {
        DIA_COMPONENT(QCompB, "qcomp-b", 1)
        FIELD(float, strength, 1.0f)
    };

    class QCompC : public Dia::Entity::IComponent {
        DIA_COMPONENT(QCompC, "qcomp-c", 1)
        FIELD(int32_t, level, 1)
    };

} // namespace DiaQueryTest

// ---------------------------------------------------------------------------
// DIA_SERIALIZE and DIA_COMPONENT_REGISTER — one .cpp per component type.
// ---------------------------------------------------------------------------

DIA_SERIALIZE(DiaQueryTest::QCompA, DiaQueryTest::QCompA::kVersion)
    DIA_FIELD(value)
DIA_SERIALIZE_END

DIA_SERIALIZE(DiaQueryTest::QCompB, DiaQueryTest::QCompB::kVersion)
    DIA_FIELD(strength)
DIA_SERIALIZE_END

DIA_SERIALIZE(DiaQueryTest::QCompC, DiaQueryTest::QCompC::kVersion)
    DIA_FIELD(level)
DIA_SERIALIZE_END

namespace DiaQueryTest {

    DIA_COMPONENT_REGISTER(QCompA, "qcomp-a", false, false, nullptr, 0, nullptr, 0, nullptr, 0)
    DIA_COMPONENT_REGISTER(QCompB, "qcomp-b", false, false, nullptr, 0, nullptr, 0, nullptr, 0)
    DIA_COMPONENT_REGISTER(QCompC, "qcomp-c", false, false, nullptr, 0, nullptr, 0, nullptr, 0)

} // namespace DiaQueryTest

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void RegisterQueryPools(Dia::Entity::Domain& domain) {
    domain.RegisterPool(new Dia::Entity::ComponentPool<DiaQueryTest::QCompA>(DiaQueryTest::QCompA::kTypeId));
    domain.RegisterPool(new Dia::Entity::ComponentPool<DiaQueryTest::QCompB>(DiaQueryTest::QCompB::kTypeId));
    domain.RegisterPool(new Dia::Entity::ComponentPool<DiaQueryTest::QCompC>(DiaQueryTest::QCompC::kTypeId));
}

// ============================================================
// Tests
// ============================================================

// 1. Query returns empty QueryView when no entities exist.
TEST(DiaEntityQuery, QueryReturnsEmptyWhenNoEntities) {
    Domain domain;
    RegisterQueryPools(domain);

    auto view = domain.Query<DiaQueryTest::QCompA>();
    EXPECT_EQ(view.Count(), 0u);

    uint32_t count = 0;
    for (auto entry : view) {
        (void)entry;
        ++count;
    }
    EXPECT_EQ(count, 0u);
}

// 2. Query returns an entity after it has the component and EndOfFrame is called.
TEST(DiaEntityQuery, QueryReturnsEntityWithComponent) {
    Domain domain;
    RegisterQueryPools(domain);

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<DiaQueryTest::QCompA>(e, {});
    domain.EndOfFrame();

    auto view = domain.Query<DiaQueryTest::QCompA>();
    EXPECT_EQ(view.Count(), 1u);

    bool found = false;
    for (auto entry : view) {
        if (entry.entity == e) {
            found = true;
            EXPECT_NE(std::get<0>(entry.components), nullptr);
        }
    }
    EXPECT_TRUE(found);
}

// 3. Query excludes entities that don't have the queried component.
TEST(DiaEntityQuery, QueryExcludesEntitiesWithoutComponent) {
    Domain domain;
    RegisterQueryPools(domain);

    Entity eWithComp    = domain.CreateEntity();
    Entity eWithoutComp = domain.CreateEntity();

    domain.QueueAddComponent<DiaQueryTest::QCompA>(eWithComp, {});
    domain.EndOfFrame();

    auto view = domain.Query<DiaQueryTest::QCompA>();
    EXPECT_EQ(view.Count(), 1u);

    for (auto entry : view) {
        EXPECT_EQ(entry.entity, eWithComp);
        EXPECT_NE(entry.entity, eWithoutComp);
    }
}

// 4. Multi-component query returns only entities with all listed components.
TEST(DiaEntityQuery, QueryMultipleComponents) {
    Domain domain;
    RegisterQueryPools(domain);

    Entity eBoth  = domain.CreateEntity();
    Entity eOnlyA = domain.CreateEntity();

    domain.QueueAddComponent<DiaQueryTest::QCompA>(eBoth,  {});
    domain.QueueAddComponent<DiaQueryTest::QCompB>(eBoth,  {});
    domain.QueueAddComponent<DiaQueryTest::QCompA>(eOnlyA, {});
    domain.EndOfFrame();

    auto viewAB = domain.Query<DiaQueryTest::QCompA, DiaQueryTest::QCompB>();
    EXPECT_EQ(viewAB.Count(), 1u);

    bool foundBoth = false;
    for (auto entry : viewAB) {
        EXPECT_EQ(entry.entity, eBoth);
        EXPECT_NE(std::get<0>(entry.components), nullptr); // QCompA*
        EXPECT_NE(std::get<1>(entry.components), nullptr); // QCompB*
        foundBoth = true;
    }
    EXPECT_TRUE(foundBoth);
}

// 5. Query is order-independent: Query<A,B> and Query<B,A> return same entity count.
TEST(DiaEntityQuery, QueryIsOrderIndependent) {
    Domain domain;
    RegisterQueryPools(domain);

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<DiaQueryTest::QCompA>(e, {});
    domain.QueueAddComponent<DiaQueryTest::QCompB>(e, {});
    domain.EndOfFrame();

    auto viewAB = domain.Query<DiaQueryTest::QCompA, DiaQueryTest::QCompB>();
    auto viewBA = domain.Query<DiaQueryTest::QCompB, DiaQueryTest::QCompA>();

    EXPECT_EQ(viewAB.Count(), viewBA.Count());
    EXPECT_EQ(viewAB.Count(), 1u);

    // Both queries should return the same entity.
    Entity entityFromAB = Entity::Invalid();
    for (auto entry : viewAB) { entityFromAB = entry.entity; }

    Entity entityFromBA = Entity::Invalid();
    for (auto entry : viewBA) { entityFromBA = entry.entity; }

    EXPECT_EQ(entityFromAB, entityFromBA);
}

// 6. Cache is invalidated and rebuilt after a structural mutation.
TEST(DiaEntityQuery, QueryCacheInvalidatedAfterMutation) {
    Domain domain;
    RegisterQueryPools(domain);

    Entity e1 = domain.CreateEntity();
    domain.QueueAddComponent<DiaQueryTest::QCompA>(e1, {});
    domain.EndOfFrame();

    // First query — cache built with e1.
    auto view1 = domain.Query<DiaQueryTest::QCompA>();
    EXPECT_EQ(view1.Count(), 1u);

    // Add a second entity with QCompA.
    Entity e2 = domain.CreateEntity();
    domain.QueueAddComponent<DiaQueryTest::QCompA>(e2, {});
    domain.EndOfFrame(); // cache rebuilt here

    // Query again — should now return both entities.
    auto view2 = domain.Query<DiaQueryTest::QCompA>();
    EXPECT_EQ(view2.Count(), 2u);
}

// 7. Count() matches the actual number of entities returned by iteration.
TEST(DiaEntityQuery, QueryCountMatchesIteration) {
    Domain domain;
    RegisterQueryPools(domain);

    Entity e1 = domain.CreateEntity();
    Entity e2 = domain.CreateEntity();
    Entity e3 = domain.CreateEntity();

    domain.QueueAddComponent<DiaQueryTest::QCompA>(e1, {});
    domain.QueueAddComponent<DiaQueryTest::QCompA>(e2, {});
    domain.QueueAddComponent<DiaQueryTest::QCompB>(e3, {}); // e3 has only B, not A
    domain.EndOfFrame();

    auto view = domain.Query<DiaQueryTest::QCompA>();
    uint32_t iterated = 0;
    for (auto entry : view) {
        (void)entry;
        ++iterated;
    }
    EXPECT_EQ(view.Count(), iterated);
    EXPECT_EQ(view.Count(), 2u);
}

// 8. Empty QueryView is valid and Count() returns 0.
TEST(DiaEntityQuery, EmptyQueryViewIsValid) {
    Domain domain;
    RegisterQueryPools(domain);

    // No entities at all — query should return empty view without assert.
    auto view = domain.Query<DiaQueryTest::QCompB>();
    EXPECT_EQ(view.Count(), 0u);
    // begin() == end() when empty — use != negated since we only define operator!=
    EXPECT_FALSE(view.begin() != view.end());
}

// 9. Three-component query only matches entities with all three.
TEST(DiaEntityQuery, QueryThreeComponentsMatchesCorrectly) {
    Domain domain;
    RegisterQueryPools(domain);

    Entity eAll   = domain.CreateEntity();
    Entity eAB    = domain.CreateEntity();
    Entity eA     = domain.CreateEntity();

    domain.QueueAddComponent<DiaQueryTest::QCompA>(eAll, {});
    domain.QueueAddComponent<DiaQueryTest::QCompB>(eAll, {});
    domain.QueueAddComponent<DiaQueryTest::QCompC>(eAll, {});

    domain.QueueAddComponent<DiaQueryTest::QCompA>(eAB, {});
    domain.QueueAddComponent<DiaQueryTest::QCompB>(eAB, {});

    domain.QueueAddComponent<DiaQueryTest::QCompA>(eA, {});
    domain.EndOfFrame();

    auto viewABC = domain.Query<DiaQueryTest::QCompA, DiaQueryTest::QCompB, DiaQueryTest::QCompC>();
    EXPECT_EQ(viewABC.Count(), 1u);

    for (auto entry : viewABC) {
        EXPECT_EQ(entry.entity, eAll);
    }
}
