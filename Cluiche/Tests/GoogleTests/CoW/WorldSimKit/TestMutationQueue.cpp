#include <gtest/gtest.h>

#include <Shared/WorldStep/MutationQueue.h>

namespace CoW {
namespace WorldStep {
namespace Testing {

namespace {
using Queue = MutationQueue<int, 128>;
}

TEST(MutationQueueTest, DequeuesInArrivalOrder)
{
    Queue q;
    EXPECT_TRUE(q.Enqueue(1));
    EXPECT_TRUE(q.Enqueue(2));
    EXPECT_TRUE(q.Enqueue(3));
    EXPECT_EQ(3u, q.Count());
    int v = 0;
    ASSERT_TRUE(q.Dequeue(v)); EXPECT_EQ(1, v);
    ASSERT_TRUE(q.Dequeue(v)); EXPECT_EQ(2, v);
    ASSERT_TRUE(q.Dequeue(v)); EXPECT_EQ(3, v);
    EXPECT_EQ(0u, q.Count());
}

TEST(MutationQueueTest, RejectsWhenFullAndLeavesStateUnchanged)
{
    Queue q;
    for (int i = 0; i < 128; ++i)
    {
        EXPECT_TRUE(q.Enqueue(i)) << i;
    }
    EXPECT_EQ(128u, q.Count());
    EXPECT_FALSE(q.Enqueue(999));
    EXPECT_EQ(128u, q.Count());
    int v = -1;
    for (int i = 0; i < 128; ++i)
    {
        ASSERT_TRUE(q.Dequeue(v));
        EXPECT_EQ(i, v);
    }
    EXPECT_FALSE(q.Dequeue(v));
}

TEST(MutationQueueTest, ClearThenRefill)
{
    Queue q;
    for (int i = 0; i < 128; ++i) { q.Enqueue(i); }
    q.Clear();
    EXPECT_EQ(0u, q.Count());
    for (int i = 0; i < 128; ++i)
    {
        EXPECT_TRUE(q.Enqueue(i + 1000));
    }
    EXPECT_FALSE(q.Enqueue(1));
    int v = 0;
    ASSERT_TRUE(q.Dequeue(v));
    EXPECT_EQ(1000, v);
}

TEST(MutationQueueTest, InterleavedKeepsOrderAcrossWrap)
{
    MutationQueue<int, 4> q;
    int nextIn = 0;
    int nextOut = 0;
    for (int round = 0; round < 50; ++round)
    {
        EXPECT_TRUE(q.Enqueue(nextIn++));
        EXPECT_TRUE(q.Enqueue(nextIn++));
        EXPECT_TRUE(q.Enqueue(nextIn++));
        int v = -1;
        ASSERT_TRUE(q.Dequeue(v));
        EXPECT_EQ(nextOut++, v);
        ASSERT_TRUE(q.Dequeue(v));
        EXPECT_EQ(nextOut++, v);
        ASSERT_TRUE(q.Dequeue(v));
        EXPECT_EQ(nextOut++, v);
    }
    EXPECT_EQ(0u, q.Count());
}

TEST(MutationQueueTest, DequeueOnEmptyReturnsFalse)
{
    Queue q;
    int v = 42;
    EXPECT_FALSE(q.Dequeue(v));
    EXPECT_EQ(42, v);
}

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
