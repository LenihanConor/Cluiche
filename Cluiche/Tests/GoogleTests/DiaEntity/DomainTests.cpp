#include <gtest/gtest.h>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/Entity.h>

using namespace Dia::Entity;

// ============================================================
// Foundation: entity creation and identity
// ============================================================

TEST(DiaEntityFoundation, CreateEntityReturnsValidHandle) {
    Domain domain;
    Entity e = domain.CreateEntity();
    EXPECT_TRUE(e.IsValid());
    EXPECT_TRUE(domain.IsAlive(e));
}

TEST(DiaEntityFoundation, IsAliveReturnsFalseForDefaultConstructedEntity) {
    Domain domain;
    Entity invalid = Entity::Invalid();
    EXPECT_FALSE(domain.IsAlive(invalid));
}

TEST(DiaEntityFoundation, MultipleEntitiesAreUnique) {
    Domain domain;
    Entity e1 = domain.CreateEntity();
    Entity e2 = domain.CreateEntity();
    EXPECT_TRUE(e1.IsValid());
    EXPECT_TRUE(e2.IsValid());
    EXPECT_NE(e1, e2);
}

TEST(DiaEntityFoundation, MultipleEntitiesAllAlive) {
    Domain domain;
    Entity e1 = domain.CreateEntity();
    Entity e2 = domain.CreateEntity();
    Entity e3 = domain.CreateEntity();
    EXPECT_TRUE(domain.IsAlive(e1));
    EXPECT_TRUE(domain.IsAlive(e2));
    EXPECT_TRUE(domain.IsAlive(e3));
}

// ============================================================
// QueueDestroy + EndOfFrame
// ============================================================

TEST(DiaEntityFoundation, QueueDestroyEntityStillAliveBeforeEndOfFrame) {
    Domain domain;
    Entity e = domain.CreateEntity();
    domain.QueueDestroy(e);
    // Mutation is queued — entity must still be alive until EndOfFrame.
    EXPECT_TRUE(domain.IsAlive(e));
}

TEST(DiaEntityFoundation, QueueDestroyThenEndOfFrameKillsEntity) {
    Domain domain;
    Entity e = domain.CreateEntity();
    domain.QueueDestroy(e);
    domain.EndOfFrame();
    EXPECT_FALSE(domain.IsAlive(e));
}

TEST(DiaEntityFoundation, DestroyedEntitySlotReclaimedOnNextCreate) {
    Domain domain;
    Entity e1 = domain.CreateEntity();
    uint32_t idx = e1.GetIndex();
    domain.QueueDestroy(e1);
    domain.EndOfFrame();

    Entity e2 = domain.CreateEntity();
    // The slot index may be reused but the generation must differ.
    if (e2.GetIndex() == idx) {
        EXPECT_NE(e2.GetGeneration(), e1.GetGeneration());
    }
    EXPECT_TRUE(domain.IsAlive(e2));
    EXPECT_FALSE(domain.IsAlive(e1)); // stale handle still invalid
}

TEST(DiaEntityFoundation, EndOfFrameEmptyQueueIsNoOp) {
    Domain domain;
    Entity e = domain.CreateEntity();
    domain.EndOfFrame(); // empty queue — must not crash
    EXPECT_TRUE(domain.IsAlive(e));
}

TEST(DiaEntityFoundation, UpdateIsNoOp) {
    Domain domain;
    Entity e = domain.CreateEntity();
    domain.Update(0.016f); // stub — must not crash
    EXPECT_TRUE(domain.IsAlive(e));
}

// ============================================================
// Debug name
// ============================================================

TEST(DiaEntityFoundation, GetDebugNameReturnsNameInDebugBuilds) {
    Domain domain;
    Entity e = domain.CreateEntity("test-entity");
#ifdef DEBUG
    const char* name = domain.GetDebugName(e);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "test-entity");
#else
    // In release builds GetDebugName always returns nullptr.
    EXPECT_EQ(domain.GetDebugName(e), nullptr);
#endif
}

TEST(DiaEntityFoundation, GetDebugNameNullForAnonymousEntity) {
    Domain domain;
    Entity e = domain.CreateEntity(); // no name overload
#ifdef DEBUG
    // Anonymous entity should return nullptr (mDebugNameSet is false).
    EXPECT_EQ(domain.GetDebugName(e), nullptr);
#else
    EXPECT_EQ(domain.GetDebugName(e), nullptr);
#endif
}

// ============================================================
// IsAlive after EndOfFrame (regression: no false positives)
// ============================================================

TEST(DiaEntityFoundation, IsAliveRemainsStableAcrossMultipleFrames) {
    Domain domain;
    Entity e = domain.CreateEntity();
    domain.EndOfFrame();
    domain.EndOfFrame();
    domain.EndOfFrame();
    EXPECT_TRUE(domain.IsAlive(e));
}

TEST(DiaEntityFoundation, DestroyAndRecreateDoesNotLeakAliveness) {
    Domain domain;
    Entity first = domain.CreateEntity();
    domain.QueueDestroy(first);
    domain.EndOfFrame();
    EXPECT_FALSE(domain.IsAlive(first));

    Entity second = domain.CreateEntity();
    EXPECT_TRUE(domain.IsAlive(second));
    EXPECT_FALSE(domain.IsAlive(first)); // generation mismatch keeps stale handle dead
}
