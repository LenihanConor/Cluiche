#include <gtest/gtest.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaEntity/ComponentPool.h>

using namespace Dia::Entity;

// ============================================================
// Test component with DoUpdate override
// ============================================================

namespace DiaEntityTest {

    // A simple updatable component for testing.
    // Tracks update count and the dt value passed in the most recent DoUpdate.
    class UpdatableComp : public Dia::Entity::IComponent {
        DIA_COMPONENT(UpdatableComp, "updatable-comp", 1)
        DIA_UPDATABLE

    public:
        int updateCount = 0;
        float lastDt = 0.0f;

        void DoUpdate(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/, float dt) override {
            ++updateCount;
            lastDt = dt;
        }
    };

} // namespace DiaEntityTest

// Serialize free function (required for registration even with no fields).
DIA_SERIALIZE(DiaEntityTest::UpdatableComp, DiaEntityTest::UpdatableComp::kVersion)
DIA_SERIALIZE_END

namespace DiaEntityTest {

    // Registration with no fields or requirements.
    // Use kIsUpdatable to automatically set the flag.
    DIA_COMPONENT_REGISTER(UpdatableComp, "updatable-comp", UpdatableComp::kIsUpdatable, false,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0)

} // namespace DiaEntityTest

// ============================================================
// Helper: register UpdatableComp pool with a domain
// ============================================================

static void RegisterUpdatableCompPool(Domain& domain) {
    domain.RegisterPool(new ComponentPool<DiaEntityTest::UpdatableComp>(DiaEntityTest::UpdatableComp::kTypeId));
}

// ============================================================
// Update Loop Tests
// ============================================================

TEST(DiaEntityUpdateLoop, DoUpdateCalledOncePerFrame) {
    Domain domain;
    RegisterUpdatableCompPool(domain);
    Entity e = domain.CreateEntity();

    // Add the updatable component.
    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaEntityTest::UpdatableComp::kTypeId, config);
    domain.EndOfFrame();

    // Component should not have been updated yet.
    DiaEntityTest::UpdatableComp* comp = domain.GetComponent<DiaEntityTest::UpdatableComp>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->updateCount, 0);

    // Call Update.
    domain.Update(0.016f);

    // Component should now have been updated once.
    EXPECT_EQ(comp->updateCount, 1);
}

TEST(DiaEntityUpdateLoop, DtPassedCorrectly) {
    Domain domain;
    RegisterUpdatableCompPool(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaEntityTest::UpdatableComp::kTypeId, config);
    domain.EndOfFrame();

    DiaEntityTest::UpdatableComp* comp = domain.GetComponent<DiaEntityTest::UpdatableComp>(e);
    ASSERT_NE(comp, nullptr);

    // Update with a specific dt value.
    const float testDt = 0.0333f;
    domain.Update(testDt);

    // The component should have received the exact dt value.
    EXPECT_EQ(comp->updateCount, 1);
    EXPECT_EQ(comp->lastDt, testDt);
}

TEST(DiaEntityUpdateLoop, MultipleEntitiesAllUpdated) {
    Domain domain;
    RegisterUpdatableCompPool(domain);
    Entity e1 = domain.CreateEntity();
    Entity e2 = domain.CreateEntity();
    Entity e3 = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e1, DiaEntityTest::UpdatableComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e2, DiaEntityTest::UpdatableComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e3, DiaEntityTest::UpdatableComp::kTypeId, config);
    domain.EndOfFrame();

    domain.Update(0.016f);

    DiaEntityTest::UpdatableComp* c1 = domain.GetComponent<DiaEntityTest::UpdatableComp>(e1);
    DiaEntityTest::UpdatableComp* c2 = domain.GetComponent<DiaEntityTest::UpdatableComp>(e2);
    DiaEntityTest::UpdatableComp* c3 = domain.GetComponent<DiaEntityTest::UpdatableComp>(e3);

    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);
    ASSERT_NE(c3, nullptr);

    EXPECT_EQ(c1->updateCount, 1);
    EXPECT_EQ(c2->updateCount, 1);
    EXPECT_EQ(c3->updateCount, 1);
}

TEST(DiaEntityUpdateLoop, UpdateAndEndOfFrameIndependent) {
    Domain domain;
    RegisterUpdatableCompPool(domain);
    Entity e1 = domain.CreateEntity();
    Entity e2 = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e1, DiaEntityTest::UpdatableComp::kTypeId, config);
    domain.EndOfFrame();

    // e1 is now live with the component.
    domain.Update(0.016f);

    DiaEntityTest::UpdatableComp* c1 = domain.GetComponent<DiaEntityTest::UpdatableComp>(e1);
    ASSERT_NE(c1, nullptr);
    EXPECT_EQ(c1->updateCount, 1);

    // Now queue another component add for e2 and a destroy for e1.
    domain.QueueAddComponentByTypeId(e2, DiaEntityTest::UpdatableComp::kTypeId, config);
    domain.QueueDestroy(e1);

    // Before EndOfFrame, e1's component should still be updated (not yet destroyed).
    domain.Update(0.016f);
    EXPECT_EQ(c1->updateCount, 2);

    // After EndOfFrame, e1 is gone and e2 is live.
    domain.EndOfFrame();

    EXPECT_FALSE(domain.IsAlive(e1));
    EXPECT_TRUE(domain.IsAlive(e2));

    DiaEntityTest::UpdatableComp* c2 = domain.GetComponent<DiaEntityTest::UpdatableComp>(e2);
    ASSERT_NE(c2, nullptr);
    EXPECT_EQ(c2->updateCount, 0); // not yet updated

    // Update again — e2 should be updated, e1's component is gone.
    domain.Update(0.016f);
    EXPECT_EQ(c2->updateCount, 1);
}

TEST(DiaEntityUpdateLoop, QueuedComponentNotUpdatedBeforeEndOfFrame) {
    Domain domain;
    RegisterUpdatableCompPool(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaEntityTest::UpdatableComp::kTypeId, config);
    // NOT calling EndOfFrame yet.

    // Update should skip this component since it hasn't been applied.
    domain.Update(0.016f);

    // Component is still queued, not applied.
    DiaEntityTest::UpdatableComp* comp = domain.GetComponent<DiaEntityTest::UpdatableComp>(e);
    EXPECT_EQ(comp, nullptr); // component does not exist yet

    // Now apply the queued add.
    domain.EndOfFrame();
    comp = domain.GetComponent<DiaEntityTest::UpdatableComp>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->updateCount, 0); // still zero, not updated until next Update call
}

TEST(DiaEntityUpdateLoop, ZeroEntitiesNoOp) {
    Domain domain;
    // No entities created, no Update operation queued.
    domain.Update(0.016f); // must not crash
    domain.EndOfFrame();
}

TEST(DiaEntityUpdateLoop, MultipleUpdatesPerFrame) {
    Domain domain;
    RegisterUpdatableCompPool(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaEntityTest::UpdatableComp::kTypeId, config);
    domain.EndOfFrame();

    DiaEntityTest::UpdatableComp* comp = domain.GetComponent<DiaEntityTest::UpdatableComp>(e);
    ASSERT_NE(comp, nullptr);

    // Call Update multiple times.
    domain.Update(0.016f);
    EXPECT_EQ(comp->updateCount, 1);

    domain.Update(0.016f);
    EXPECT_EQ(comp->updateCount, 2);

    domain.Update(0.016f);
    EXPECT_EQ(comp->updateCount, 3);
}
