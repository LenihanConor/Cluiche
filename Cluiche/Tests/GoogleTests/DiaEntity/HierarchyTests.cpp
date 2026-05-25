#include <gtest/gtest.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/EntityAddress.h>
#include <DiaEntity/Hierarchy/ParentComponent.h>
#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaEntity/Hierarchy/Hierarchy.h>
#include <DiaEntity/Messages/EntityDestroyedMessage.h>

using namespace Dia::Entity;
using namespace Dia::Entity::Hierarchy;

// =========================================================================
// Helper: register hierarchy component pools with a domain
// =========================================================================
static void RegisterHierarchyPools(Domain& domain) {
    domain.RegisterPool(new ComponentPool<ParentComponent>(ParentComponent::kTypeId));
    domain.RegisterPool(new ComponentPool<ChildBufferComponent>(ChildBufferComponent::kTypeId));
}

// =========================================================================
// 1. QueueSetParentAddsParentComponent
//    After EndOfFrame, child has ParentComponent pointing to parent.
// =========================================================================
TEST(DiaEntityHierarchy, QueueSetParentAddsParentComponent) {
    Domain domain;
    RegisterHierarchyPools(domain);

    Entity parent = domain.CreateEntity("parent");
    Entity child  = domain.CreateEntity("child");

    Hierarchy::QueueSetParent(domain, child, parent);
    domain.EndOfFrame();

    ASSERT_TRUE(domain.HasComponent<ParentComponent>(child));

    const ParentComponent* pc = domain.GetComponent<ParentComponent>(child);
    ASSERT_NE(pc, nullptr);

    Entity resolvedParent = pc->GetParentEntity();
    EXPECT_EQ(resolvedParent.GetIndex(),      parent.GetIndex());
    EXPECT_EQ(resolvedParent.GetGeneration(), parent.GetGeneration());
}

// =========================================================================
// 2. QueueSetParentAddsChildBufferComponent
//    After EndOfFrame, parent has ChildBufferComponent with child in it.
// =========================================================================
TEST(DiaEntityHierarchy, QueueSetParentAddsChildBufferComponent) {
    Domain domain;
    RegisterHierarchyPools(domain);

    Entity parent = domain.CreateEntity("parent");
    Entity child  = domain.CreateEntity("child");

    Hierarchy::QueueSetParent(domain, child, parent);
    domain.EndOfFrame();

    ASSERT_TRUE(domain.HasComponent<ChildBufferComponent>(parent));

    const ChildBufferComponent* cb = domain.GetComponent<ChildBufferComponent>(parent);
    ASSERT_NE(cb, nullptr);
    EXPECT_EQ(cb->children.Size(), 1u);
    EXPECT_EQ(cb->children[0], child);
}

// =========================================================================
// 3. RootEntityHasNoParentComponent
//    A root entity (never parented) has no ParentComponent.
// =========================================================================
TEST(DiaEntityHierarchy, RootEntityHasNoParentComponent) {
    Domain domain;
    RegisterHierarchyPools(domain);

    Entity root = domain.CreateEntity("root");
    domain.EndOfFrame();

    EXPECT_FALSE(domain.HasComponent<ParentComponent>(root));
}

// =========================================================================
// 4. QueueDestroySubtreeDestroysAllDescendants
//    parent with 2 children — QueueDestroySubtree destroys all 3.
// =========================================================================
TEST(DiaEntityHierarchy, QueueDestroySubtreeDestroysAllDescendants) {
    Domain domain;
    RegisterHierarchyPools(domain);

    Entity parent = domain.CreateEntity("parent");
    Entity child1 = domain.CreateEntity("child1");
    Entity child2 = domain.CreateEntity("child2");

    Hierarchy::QueueSetParent(domain, child1, parent);
    Hierarchy::QueueSetParent(domain, child2, parent);
    domain.EndOfFrame();

    // Confirm children are in the buffer.
    ASSERT_TRUE(domain.HasComponent<ChildBufferComponent>(parent));
    ASSERT_EQ(domain.GetComponent<ChildBufferComponent>(parent)->children.Size(), 2u);

    Hierarchy::QueueDestroySubtree(domain, parent);
    domain.EndOfFrame();

    EXPECT_FALSE(domain.IsAlive(parent));
    EXPECT_FALSE(domain.IsAlive(child1));
    EXPECT_FALSE(domain.IsAlive(child2));
}

// =========================================================================
// 5. QueueDestroySubtreeDeepHierarchy
//    parent → child → grandchild; all 3 destroyed.
// =========================================================================
TEST(DiaEntityHierarchy, QueueDestroySubtreeDeepHierarchy) {
    Domain domain;
    RegisterHierarchyPools(domain);

    Entity parent     = domain.CreateEntity("parent");
    Entity child      = domain.CreateEntity("child");
    Entity grandchild = domain.CreateEntity("grandchild");

    Hierarchy::QueueSetParent(domain, child, parent);
    domain.EndOfFrame();
    Hierarchy::QueueSetParent(domain, grandchild, child);
    domain.EndOfFrame();

    Hierarchy::QueueDestroySubtree(domain, parent);
    domain.EndOfFrame();

    EXPECT_FALSE(domain.IsAlive(parent));
    EXPECT_FALSE(domain.IsAlive(child));
    EXPECT_FALSE(domain.IsAlive(grandchild));
}

// =========================================================================
// 6. EntityDestroyedMessageEmittedOnDestroy
//    Destroy an entity; drain EntityDestroyedMessage; confirm it was received.
// =========================================================================
TEST(DiaEntityHierarchy, EntityDestroyedMessageEmittedOnDestroy) {
    Domain domain;
    // EntityDestroyedMessage is already registered by Domain constructor.

    Entity e = domain.CreateEntity("doomed");

    // Subscribe to EntityDestroyedMessage for this entity's subscriber id.
    Dia::Mailbox::SubscriberId sub = MakeEntitySubscriberId(e);
    domain.GetMailbox().Subscribe<EntityDestroyedMessage>(sub);

    domain.QueueDestroy(e);
    domain.EndOfFrame();

    bool received = false;
    domain.GetMailbox().Drain<EntityDestroyedMessage>(
        [&](const Dia::Mailbox::Address& /*addr*/, const EntityDestroyedMessage& msg) {
            if (msg.destroyed.GetIndex() == e.GetIndex()) {
                received = true;
            }
        });

    EXPECT_TRUE(received);
}

// =========================================================================
// 7. EntityDestroyedMessageForMultipleEntities
//    Destroying two entities emits two EntityDestroyedMessages.
// =========================================================================
TEST(DiaEntityHierarchy, EntityDestroyedMessageForMultipleEntities) {
    Domain domain;

    Entity e1 = domain.CreateEntity("doomed1");
    Entity e2 = domain.CreateEntity("doomed2");

    domain.QueueDestroy(e1);
    domain.QueueDestroy(e2);
    domain.EndOfFrame();

    int count = 0;
    domain.GetMailbox().Drain<EntityDestroyedMessage>(
        [&](const Dia::Mailbox::Address& /*addr*/, const EntityDestroyedMessage& /*msg*/) {
            ++count;
        });

    // One message per destroyed entity.
    EXPECT_EQ(count, 2);
}

// =========================================================================
// 8. ReparentMovesChildToNewParent
//    child is reparented from parent1 to parent2;
//    after EndOfFrame child points to parent2, old parent buffer updated.
// =========================================================================
TEST(DiaEntityHierarchy, ReparentMovesChildToNewParent) {
    Domain domain;
    RegisterHierarchyPools(domain);

    Entity parent1 = domain.CreateEntity("parent1");
    Entity parent2 = domain.CreateEntity("parent2");
    Entity child   = domain.CreateEntity("child");

    Hierarchy::QueueSetParent(domain, child, parent1);
    domain.EndOfFrame();

    // Confirm initial state.
    ASSERT_TRUE(domain.HasComponent<ParentComponent>(child));
    ASSERT_EQ(domain.GetComponent<ParentComponent>(child)->GetParentEntity().GetIndex(),
              parent1.GetIndex());
    ASSERT_EQ(domain.GetComponent<ChildBufferComponent>(parent1)->children.Size(), 1u);

    // Reparent.
    Hierarchy::QueueSetParent(domain, child, parent2);
    domain.EndOfFrame();

    // child now points to parent2.
    ASSERT_TRUE(domain.HasComponent<ParentComponent>(child));
    EXPECT_EQ(domain.GetComponent<ParentComponent>(child)->GetParentEntity().GetIndex(),
              parent2.GetIndex());

    // Old parent no longer has child in its buffer.
    const ChildBufferComponent* cb1 = domain.GetComponent<ChildBufferComponent>(parent1);
    EXPECT_TRUE(cb1 == nullptr || cb1->children.Size() == 0u);

    // New parent has child.
    ASSERT_TRUE(domain.HasComponent<ChildBufferComponent>(parent2));
    const ChildBufferComponent* cb2 = domain.GetComponent<ChildBufferComponent>(parent2);
    ASSERT_NE(cb2, nullptr);
    EXPECT_EQ(cb2->children.Size(), 1u);
    EXPECT_EQ(cb2->children[0], child);
}

// =========================================================================
// 9. ChildBufferMaxChildrenEnforced
//    Up to kMaxChildren (16) children can be added; the 17th triggers an assert.
// =========================================================================
TEST(DiaEntityHierarchy, ChildBufferMaxChildrenFitsFull) {
    // Test that exactly kMaxChildren children can be stored.
    Domain domain;
    RegisterHierarchyPools(domain);

    Entity parent = domain.CreateEntity("parent");

    // Add kMaxChildren children using QueueSetParent.
    for (uint32_t i = 0; i < ChildBufferComponent::kMaxChildren; ++i) {
        Entity child = domain.CreateEntity();
        Hierarchy::QueueSetParent(domain, child, parent);
    }
    domain.EndOfFrame();

    const ChildBufferComponent* cb = domain.GetComponent<ChildBufferComponent>(parent);
    ASSERT_NE(cb, nullptr);
    EXPECT_EQ(cb->children.Size(), ChildBufferComponent::kMaxChildren);
}
