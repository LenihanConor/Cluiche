#include <gtest/gtest.h>
#include <memory>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/ComponentPool.h>
#include <diaentitytemplate/EntityAddress.h>
#include <diaentitytemplate/Hierarchy/ParentComponent.h>
#include <diaentitytemplate/Hierarchy/ChildBufferComponent.h>
#include <diaentitytemplate/Hierarchy/Hierarchy.h>
#include "TestComponent.h"
#include "BpTransformComponent.h"

using namespace Dia::Entity;
using namespace DiaEntityTest;

// ============================================================
// Helpers
// ============================================================

static void RegisterStressPools(Domain& domain) {
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::kTypeId));
    domain.RegisterPool(new ComponentPool<BpHealth>(BpHealth::kTypeId));
    domain.RegisterPool(new ComponentPool<Hierarchy::ParentComponent>(Hierarchy::ParentComponent::kTypeId));
    domain.RegisterPool(new ComponentPool<Hierarchy::ChildBufferComponent>(Hierarchy::ChildBufferComponent::kTypeId));
}

// ============================================================
// Pool capacity stress
// ============================================================

// Fill domain to capacity, verify all alive, then destroy 10 and verify
// that 10 new entities can be created from the recycled slots.
// Note: calling CreateEntity() when pool is full triggers DIA_ASSERT in DEBUG builds,
// so we verify pool-full behaviour via GetEntityCount() instead.
// Domain and entity array are heap-allocated to avoid large stack frames.
TEST(DiaEntityStress, EntityPoolFull) {
    auto domain = std::make_unique<Domain>();
    auto entities = std::make_unique<Entity[]>(kMaxEntitiesPerDomain);

    // Fill the pool to kMaxEntitiesPerDomain.
    for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
        entities[i] = domain->CreateEntity();
        EXPECT_TRUE(entities[i].IsValid()) << "Entity " << i << " should be valid";
    }

    // All entities must be alive.
    for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
        EXPECT_TRUE(domain->IsAlive(entities[i])) << "Entity " << i << " should be alive";
    }

    // Verify that all slots are used (GetEntityCount reflects capacity).
    EXPECT_EQ(domain->GetEntityCount(), kMaxEntitiesPerDomain);

    // Destroy 10 entities.
    for (uint32_t i = 0; i < 10; ++i) {
        domain->QueueDestroy(entities[i]);
    }
    domain->EndOfFrame();

    // Those 10 should be dead.
    for (uint32_t i = 0; i < 10; ++i) {
        EXPECT_FALSE(domain->IsAlive(entities[i])) << "Destroyed entity " << i << " should be dead";
    }

    // The remaining 1014 should still be alive.
    for (uint32_t i = 10; i < kMaxEntitiesPerDomain; ++i) {
        EXPECT_TRUE(domain->IsAlive(entities[i])) << "Surviving entity " << i << " should still be alive";
    }

    EXPECT_EQ(domain->GetEntityCount(), kMaxEntitiesPerDomain - 10u);

    // 10 new entities can be created (slots recycled).
    for (uint32_t i = 0; i < 10; ++i) {
        Entity e = domain->CreateEntity();
        EXPECT_TRUE(e.IsValid()) << "Recycled slot " << i << " should produce a valid entity";
        EXPECT_TRUE(domain->IsAlive(e)) << "Recycled entity " << i << " should be alive";
    }

    EXPECT_EQ(domain->GetEntityCount(), kMaxEntitiesPerDomain);
}

// Fill domain, add TestComponent to every entity, verify all have it,
// then remove all components.
// Domain and entity array are heap-allocated to avoid large stack frames.
TEST(DiaEntityStress, ComponentPoolFull) {
    auto domain = std::make_unique<Domain>();
    domain->RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));
    auto entities = std::make_unique<Entity[]>(kMaxEntitiesPerDomain);

    // Create all entities.
    for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
        entities[i] = domain->CreateEntity();
        ASSERT_TRUE(entities[i].IsValid());
    }

    // The mutation queue is kMaxMutationsPerFrame (256). Queue in batches.
    // We need kMaxEntitiesPerDomain (1024) add-ops, so 4 frames of 256.
    for (uint32_t batch = 0; batch < kMaxEntitiesPerDomain / kMaxMutationsPerFrame; ++batch) {
        for (uint32_t i = 0; i < kMaxMutationsPerFrame; ++i) {
            uint32_t idx = batch * kMaxMutationsPerFrame + i;
            domain->QueueAddComponent<TestComponent>(entities[idx], Json::Value());
        }
        domain->EndOfFrame();
    }

    // All entities should now have TestComponent.
    for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
        EXPECT_TRUE(domain->HasComponent<TestComponent>(entities[i]))
            << "Entity " << i << " should have TestComponent";
    }

    // Remove all components in batches.
    for (uint32_t batch = 0; batch < kMaxEntitiesPerDomain / kMaxMutationsPerFrame; ++batch) {
        for (uint32_t i = 0; i < kMaxMutationsPerFrame; ++i) {
            uint32_t idx = batch * kMaxMutationsPerFrame + i;
            domain->QueueRemoveComponent<TestComponent>(entities[idx]);
        }
        domain->EndOfFrame();
    }

    // None should have the component now.
    for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
        EXPECT_FALSE(domain->HasComponent<TestComponent>(entities[i]))
            << "Entity " << i << " should not have TestComponent after removal";
    }
}

// Queue exactly kMaxMutationsPerFrame mutations, then queue one more.
// After EndOfFrame, verify only the first kMaxMutationsPerFrame applied.
// The 257th op should be dropped (queue full at time of queue call).
TEST(DiaEntityStress, MutationQueuePressure) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));

    // Create kMaxMutationsPerFrame + 1 entities (creation doesn't use the queue).
    const uint32_t kNumEntities = kMaxMutationsPerFrame + 1; // 257
    auto entities = std::make_unique<Entity[]>(kNumEntities);
    for (uint32_t i = 0; i < kNumEntities; ++i) {
        entities[i] = domain.CreateEntity();
        ASSERT_TRUE(entities[i].IsValid());
    }

    // Queue kMaxMutationsPerFrame (256) add-component ops — fills the queue exactly.
    for (uint32_t i = 0; i < kMaxMutationsPerFrame; ++i) {
        domain.QueueAddComponent<TestComponent>(entities[i], Json::Value());
    }

    // Queue one more — this should be dropped silently (queue full).
    domain.QueueAddComponent<TestComponent>(entities[kMaxMutationsPerFrame], Json::Value());

    domain.EndOfFrame();

    // The first 256 entities should have TestComponent.
    uint32_t countWithComp = 0;
    for (uint32_t i = 0; i < kNumEntities; ++i) {
        if (domain.HasComponent<TestComponent>(entities[i])) {
            ++countWithComp;
        }
    }

    // Exactly 256 ops were applied; the 257th was dropped.
    EXPECT_EQ(countWithComp, kMaxMutationsPerFrame);

    // The last entity (index kMaxMutationsPerFrame) should have been dropped.
    EXPECT_FALSE(domain.HasComponent<TestComponent>(entities[kMaxMutationsPerFrame]))
        << "257th queued op should have been dropped";
}

// Verify Query<TestComponent>, Query<BpTransform>, and Query<TestComponent,BpTransform>
// each produce separate, correct cache entries with correct counts.
TEST(DiaEntityStress, QueryCacheCapacity) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::kTypeId));

    // Create 5 entities with TestComponent, 3 with BpTransform, 2 with both.
    Entity eTestOnly[3];
    Entity eBpOnly[1];
    Entity eBoth[2];

    for (uint32_t i = 0; i < 3; ++i) {
        eTestOnly[i] = domain.CreateEntity();
        domain.QueueAddComponent<TestComponent>(eTestOnly[i], Json::Value());
    }
    eBpOnly[0] = domain.CreateEntity();
    domain.QueueAddComponent<BpTransform>(eBpOnly[0], Json::Value());

    for (uint32_t i = 0; i < 2; ++i) {
        eBoth[i] = domain.CreateEntity();
        domain.QueueAddComponent<TestComponent>(eBoth[i], Json::Value());
        domain.QueueAddComponent<BpTransform>(eBoth[i], Json::Value());
    }
    domain.EndOfFrame();
    // BpTransform queued but AddComponent ops queued together — need another frame for the BpTransform adds on eBoth.
    // Actually all 8 adds fit in kMaxMutationsPerFrame (256), so they all apply in one EndOfFrame.

    // Query<TestComponent> should return 3 + 2 = 5 entities.
    auto viewTest = domain.Query<TestComponent>();
    EXPECT_EQ(viewTest.Count(), 5u);

    // Query<BpTransform> should return 1 + 2 = 3 entities.
    auto viewBp = domain.Query<BpTransform>();
    EXPECT_EQ(viewBp.Count(), 3u);

    // Query<TestComponent, BpTransform> should return 2 entities.
    auto viewBoth = domain.Query<TestComponent, BpTransform>();
    EXPECT_EQ(viewBoth.Count(), 2u);

    // Each query type has a distinct cache entry — verify counts are independent.
    // Re-querying returns same results (cache hits).
    auto viewTest2 = domain.Query<TestComponent>();
    EXPECT_EQ(viewTest2.Count(), 5u);

    auto viewBp2 = domain.Query<BpTransform>();
    EXPECT_EQ(viewBp2.Count(), 3u);

    auto viewBoth2 = domain.Query<TestComponent, BpTransform>();
    EXPECT_EQ(viewBoth2.Count(), 2u);
}

// ============================================================
// Generation correctness
// ============================================================

// Destroying an entity bumps the generation. The stale handle is rejected.
// A new entity at the same slot (different generation) is valid.
TEST(DiaEntityStress, StaleHandleRejectedAfterDestroy) {
    Domain domain;

    // Create a single entity; with a fresh domain the first slot is 0, generation=1.
    Entity e1 = domain.CreateEntity("stress-stale");
    ASSERT_TRUE(e1.IsValid());
    ASSERT_TRUE(domain.IsAlive(e1));

    // Save the stale handle before destroying.
    Entity stale = e1;

    domain.QueueDestroy(e1);
    domain.EndOfFrame();

    EXPECT_FALSE(domain.IsAlive(stale)) << "Stale handle should be dead after destroy";

    // Create a new entity — may reuse the same slot index.
    Entity e2 = domain.CreateEntity("stress-fresh");
    ASSERT_TRUE(e2.IsValid());
    ASSERT_TRUE(domain.IsAlive(e2));

    // The stale handle is still invalid even though slot index may be the same.
    EXPECT_FALSE(domain.IsAlive(stale)) << "Stale handle must remain dead after slot reuse";
    EXPECT_TRUE(domain.IsAlive(e2)) << "New entity handle must be alive";
}

// Create and destroy the same slot 5 times. After each cycle verify old handles are dead.
TEST(DiaEntityStress, GenerationRolloverPreservesCorrectness) {
    Domain domain;

    Entity prev = Entity::Invalid();

    for (int cycle = 0; cycle < 5; ++cycle) {
        Entity e = domain.CreateEntity();
        ASSERT_TRUE(e.IsValid()) << "Cycle " << cycle << ": entity should be valid";
        ASSERT_TRUE(domain.IsAlive(e)) << "Cycle " << cycle << ": entity should be alive";

        // The previous cycle's handle should still be dead.
        if (prev.IsValid()) {
            EXPECT_FALSE(domain.IsAlive(prev))
                << "Cycle " << cycle << ": previous-cycle handle should remain dead";
        }

        prev = e;
        domain.QueueDestroy(e);
        domain.EndOfFrame();

        EXPECT_FALSE(domain.IsAlive(prev))
            << "Cycle " << cycle << ": handle should be dead after destroy";
    }
}

// Stale handle returns nullptr/false for component access.
TEST(DiaEntityStress, ComponentAccessWithStaleHandle) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));

    Entity e = domain.CreateEntity("stress-comp-stale");
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    ASSERT_TRUE(domain.HasComponent<TestComponent>(e));

    // Save stale handle.
    Entity stale = e;

    domain.QueueDestroy(e);
    domain.EndOfFrame();

    EXPECT_FALSE(domain.HasComponent<TestComponent>(stale))
        << "HasComponent on stale handle should return false";
    EXPECT_EQ(domain.GetComponent<TestComponent>(stale), nullptr)
        << "GetComponent on stale handle should return nullptr";
}
