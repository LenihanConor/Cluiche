#include <gtest/gtest.h>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/ComponentPool.h>
#include <diaentitytemplate/EntityAddress.h>
#include <diaentitytemplate/IEntityInspectable.h>
#include <diaentitytemplate/JsonBlueprintLoader.h>
#include <diaentitytemplate/Hierarchy/ParentComponent.h>
#include <diaentitytemplate/Hierarchy/ChildBufferComponent.h>
#include <diaentitytemplate/Hierarchy/Hierarchy.h>
#include <diaentitytemplate/Messages/EntityDestroyedMessage.h>
#include "TestComponent.h"
#include "BpTransformComponent.h"

using namespace Dia::Entity;
using namespace DiaEntityTest;

// ============================================================
// Helpers
// ============================================================

static void RegisterIntegrationPools(Domain& domain) {
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::kTypeId));
    domain.RegisterPool(new ComponentPool<BpHealth>(BpHealth::kTypeId));
    domain.RegisterPool(new ComponentPool<Hierarchy::ParentComponent>(Hierarchy::ParentComponent::kTypeId));
    domain.RegisterPool(new ComponentPool<Hierarchy::ChildBufferComponent>(Hierarchy::ChildBufferComponent::kTypeId));
}

// ============================================================
// 1. Multi-frame component lifecycle
// ============================================================

// Frame 1: create 3 entities, add TestComponent to all.
// Frame 2: modify values, remove from entity[0].
// Frame 3: destroy entity[1].
// Verify entity[0] still alive (just no component), entity[2] alive with component.
TEST(DiaEntityIntegration, MultiFrameComponentLifecycle) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));

    // ----- Frame 1: create + add component -----
    Entity e0 = domain.CreateEntity("integ-e0");
    Entity e1 = domain.CreateEntity("integ-e1");
    Entity e2 = domain.CreateEntity("integ-e2");

    domain.QueueAddComponent<TestComponent>(e0, Json::Value());
    domain.QueueAddComponent<TestComponent>(e1, Json::Value());
    domain.QueueAddComponent<TestComponent>(e2, Json::Value());
    domain.EndOfFrame();

    ASSERT_TRUE(domain.HasComponent<TestComponent>(e0));
    ASSERT_TRUE(domain.HasComponent<TestComponent>(e1));
    ASSERT_TRUE(domain.HasComponent<TestComponent>(e2));

    // ----- Frame 2: modify values, remove from e0 -----
    // Modify component values directly (stable pointer until next structural change).
    TestComponent* c1 = domain.GetComponent<TestComponent>(e1);
    TestComponent* c2 = domain.GetComponent<TestComponent>(e2);
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);
    c1->speed = 99.0f;
    c2->speed = 99.0f;

    domain.QueueRemoveComponent<TestComponent>(e0);
    domain.EndOfFrame();

    // e0 still alive, just lost its component.
    EXPECT_TRUE(domain.IsAlive(e0));
    EXPECT_FALSE(domain.HasComponent<TestComponent>(e0));

    // e1 and e2 still have the component with modified value.
    ASSERT_TRUE(domain.HasComponent<TestComponent>(e1));
    ASSERT_TRUE(domain.HasComponent<TestComponent>(e2));

    TestComponent* c1b = domain.GetComponent<TestComponent>(e1);
    TestComponent* c2b = domain.GetComponent<TestComponent>(e2);
    ASSERT_NE(c1b, nullptr);
    ASSERT_NE(c2b, nullptr);
    EXPECT_FLOAT_EQ(c1b->speed, 99.0f);
    EXPECT_FLOAT_EQ(c2b->speed, 99.0f);

    // ----- Frame 3: destroy e1 -----
    domain.QueueDestroy(e1);
    domain.EndOfFrame();

    EXPECT_TRUE(domain.IsAlive(e0))  << "e0 (component removed) should still be alive";
    EXPECT_FALSE(domain.IsAlive(e1)) << "e1 should be destroyed";
    EXPECT_TRUE(domain.IsAlive(e2))  << "e2 should still be alive";
    EXPECT_TRUE(domain.HasComponent<TestComponent>(e2));
}

// ============================================================
// 2. Query and mutate interleaved across 3 frames
// ============================================================

TEST(DiaEntityIntegration, QueryAndMutateInterleaved) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));

    // Create 10 entities with TestComponent.
    const uint32_t kInitial = 10;
    Entity entities[kInitial];
    for (uint32_t i = 0; i < kInitial; ++i) {
        entities[i] = domain.CreateEntity();
        domain.QueueAddComponent<TestComponent>(entities[i], Json::Value());
    }
    domain.EndOfFrame();

    auto view1 = domain.Query<TestComponent>();
    EXPECT_EQ(view1.Count(), 10u);

    // Frame 2: destroy 3 entities.
    domain.QueueDestroy(entities[0]);
    domain.QueueDestroy(entities[1]);
    domain.QueueDestroy(entities[2]);
    domain.EndOfFrame();

    auto view2 = domain.Query<TestComponent>();
    EXPECT_EQ(view2.Count(), 7u);

    // Frame 3: create 2 new entities with TestComponent.
    Entity eNew0 = domain.CreateEntity();
    Entity eNew1 = domain.CreateEntity();
    domain.QueueAddComponent<TestComponent>(eNew0, Json::Value());
    domain.QueueAddComponent<TestComponent>(eNew1, Json::Value());
    domain.EndOfFrame();

    auto view3 = domain.Query<TestComponent>();
    EXPECT_EQ(view3.Count(), 9u);
}

// ============================================================
// 3. Hierarchy with query and subtree destroy
// ============================================================

TEST(DiaEntityIntegration, HierarchyWithQueryAndDestroy) {
    Domain domain;
    RegisterIntegrationPools(domain);

    // Create root + 3 children, parent them.
    Entity root   = domain.CreateEntity("integ-root");
    Entity child0 = domain.CreateEntity("integ-child0");
    Entity child1 = domain.CreateEntity("integ-child1");
    Entity child2 = domain.CreateEntity("integ-child2");

    Hierarchy::QueueSetParent(domain, child0, root);
    Hierarchy::QueueSetParent(domain, child1, root);
    Hierarchy::QueueSetParent(domain, child2, root);
    domain.EndOfFrame();

    // Add TestComponent to root and two of the three children.
    domain.QueueAddComponent<TestComponent>(root,   Json::Value());
    domain.QueueAddComponent<TestComponent>(child0, Json::Value());
    domain.QueueAddComponent<TestComponent>(child1, Json::Value());
    domain.EndOfFrame();

    // Query should return 3 (root + child0 + child1; child2 has no TestComponent).
    auto view1 = domain.Query<TestComponent>();
    EXPECT_EQ(view1.Count(), 3u);

    // Destroy the entire subtree.
    Hierarchy::QueueDestroySubtree(domain, root);
    domain.EndOfFrame();

    EXPECT_FALSE(domain.IsAlive(root));
    EXPECT_FALSE(domain.IsAlive(child0));
    EXPECT_FALSE(domain.IsAlive(child1));
    EXPECT_FALSE(domain.IsAlive(child2));

    // Query cache should be rebuilt — no more entities with TestComponent.
    auto view2 = domain.Query<TestComponent>();
    EXPECT_EQ(view2.Count(), 0u);
}

// ============================================================
// 4. Blueprint load and query
// ============================================================

TEST(DiaEntityIntegration, BlueprintLoadAndQuery) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::kTypeId));
    domain.RegisterPool(new ComponentPool<BpHealth>(BpHealth::kTypeId));

    // Build a blueprint with 2 BpTransform entities and 2 BpHealth entities.
    Json::Value bp;
    bp["version"] = kBlueprintSchemaVersion;

    Json::Value t1Cfg;
    t1Cfg["x"] = 3.0f;
    t1Cfg["y"] = 7.0f;
    bp["entities"]["transform-a"]["bp-transform"] = t1Cfg;

    Json::Value t2Cfg;
    t2Cfg["x"] = -1.0f;
    t2Cfg["y"] = 4.5f;
    bp["entities"]["transform-b"]["bp-transform"] = t2Cfg;

    Json::Value h1Cfg;
    h1Cfg["maxHp"] = 50;
    bp["entities"]["health-a"]["bp-health"] = h1Cfg;

    Json::Value h2Cfg;
    h2Cfg["maxHp"] = 200;
    bp["entities"]["health-b"]["bp-health"] = h2Cfg;

    JsonBlueprintLoader loader;
    ASSERT_TRUE(loader.Load(domain, bp));
    domain.EndOfFrame();

    EXPECT_EQ(domain.GetEntityCount(), 4u);

    // Query<BpTransform> should return 2.
    auto viewT = domain.Query<BpTransform>();
    EXPECT_EQ(viewT.Count(), 2u);

    // Query<BpHealth> should return 2.
    auto viewH = domain.Query<BpHealth>();
    EXPECT_EQ(viewH.Count(), 2u);

    // Verify field values on BpTransform entities.
    float sumX = 0.0f;
    float sumY = 0.0f;
    for (auto entry : viewT) {
        BpTransform* t = std::get<0>(entry.components);
        ASSERT_NE(t, nullptr);
        sumX += t->x;
        sumY += t->y;
    }
    // Expected: 3.0 + (-1.0) = 2.0 and 7.0 + 4.5 = 11.5
    EXPECT_NEAR(sumX, 2.0f, 1e-4f);
    EXPECT_NEAR(sumY, 11.5f, 1e-4f);

    // Verify field values on BpHealth entities.
    int sumHp = 0;
    for (auto entry : viewH) {
        BpHealth* h = std::get<0>(entry.components);
        ASSERT_NE(h, nullptr);
        sumHp += h->maxHp;
    }
    EXPECT_EQ(sumHp, 250); // 50 + 200
}

// ============================================================
// 5. Inspection after mutation (IEntityInspectable round trip)
// ============================================================

TEST(DiaEntityIntegration, InspectionAfterMutation) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));

    // Create entity with component.
    Entity e = domain.CreateEntity("integ-inspect");

    Json::Value cfg;
    cfg["speed"]     = 5.0f;
    cfg["hitPoints"] = 42;
    domain.QueueAddComponent<TestComponent>(e, cfg);
    domain.EndOfFrame();

    // Access through IEntityInspectable interface.
    IEntityInspectable* inspectable = &domain;
    EXPECT_EQ(inspectable->GetEntityCount(), 1u);

    // ReadField returns the correct speed.
    Json::Value speedVal;
    EXPECT_TRUE(inspectable->ReadField(e, TestComponent::kTypeId, "speed", speedVal));
    EXPECT_NEAR(speedVal.asFloat(), 5.0f, 1e-4f);

    // WriteField updates speed.
    Json::Value newSpeed(10.0f);
    EXPECT_TRUE(inspectable->WriteField(e, TestComponent::kTypeId, "speed", newSpeed));

    // ReadField now returns the updated value.
    Json::Value updatedSpeed;
    EXPECT_TRUE(inspectable->ReadField(e, TestComponent::kTypeId, "speed", updatedSpeed));
    EXPECT_NEAR(updatedSpeed.asFloat(), 10.0f, 1e-4f);

    // Destroy entity.
    domain.QueueDestroy(e);
    domain.EndOfFrame();

    EXPECT_EQ(inspectable->GetEntityCount(), 0u);
}

// ============================================================
// 6. Mailbox and hierarchy destroy — both parent and child emit destroy messages
// ============================================================

TEST(DiaEntityIntegration, MailboxAndHierarchyDestroy) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<Hierarchy::ParentComponent>(Hierarchy::ParentComponent::kTypeId));
    domain.RegisterPool(new ComponentPool<Hierarchy::ChildBufferComponent>(Hierarchy::ChildBufferComponent::kTypeId));

    // Create a listener entity and subscribe it to EntityDestroyedMessage.
    Entity listener = domain.CreateEntity("integ-listener");
    Dia::Mailbox::SubscriberId sid = MakeEntitySubscriberId(listener);
    domain.GetMailbox().Subscribe<EntityDestroyedMessage>(sid);

    // Create parent + child, SetParent.
    Entity parent = domain.CreateEntity("integ-parent");
    Entity child  = domain.CreateEntity("integ-child");
    Hierarchy::QueueSetParent(domain, child, parent);
    domain.EndOfFrame();

    ASSERT_TRUE(domain.HasComponent<Hierarchy::ParentComponent>(child));

    // Save handles before destroy.
    Entity savedParent = parent;
    Entity savedChild  = child;

    // Destroy the whole subtree.
    Hierarchy::QueueDestroySubtree(domain, parent);
    domain.EndOfFrame();

    EXPECT_FALSE(domain.IsAlive(savedParent));
    EXPECT_FALSE(domain.IsAlive(savedChild));

    // Drain EntityDestroyedMessage and count how many we received.
    int destroyCount = 0;
    bool parentReceived = false;
    bool childReceived  = false;

    domain.GetMailbox().Drain<EntityDestroyedMessage>(
        [&](const Dia::Mailbox::Address& /*addr*/, const EntityDestroyedMessage& msg) {
            ++destroyCount;
            if (msg.destroyed.GetIndex() == savedParent.GetIndex()) {
                parentReceived = true;
            }
            if (msg.destroyed.GetIndex() == savedChild.GetIndex()) {
                childReceived = true;
            }
        });

    // Both parent and child should have emitted a destroy message.
    EXPECT_GE(destroyCount, 2) << "Expected at least 2 EntityDestroyedMessages (parent + child)";
    EXPECT_TRUE(parentReceived) << "Parent should have emitted EntityDestroyedMessage";
    EXPECT_TRUE(childReceived)  << "Child should have emitted EntityDestroyedMessage";
}
