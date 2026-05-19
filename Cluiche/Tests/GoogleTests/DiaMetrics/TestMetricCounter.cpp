#include <gtest/gtest.h>
#include <DiaMetrics/MetricRegistry.h>
#include <DiaMetrics/Testing/MetricFixture.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace Dia::Metric;

struct MetricCounterTest : ::testing::Test
{
    MetricFixture fixture;
};

TEST_F(MetricCounterTest, IncAndValue)
{
    Counter* c = MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("test.counter"));
    ASSERT_NE(c, nullptr);
    c->Inc(5);
    c->Inc(3);
    EXPECT_EQ(c->Value(), 8u);
}

TEST_F(MetricCounterTest, DefaultValueIsZero)
{
    Counter* c = MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("test.counter.zero"));
    EXPECT_EQ(c->Value(), 0u);
}

TEST_F(MetricCounterTest, ConcurrentInc_TotalCorrect)
{
    Counter* c = MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("test.counter.concurrent"));
    ASSERT_NE(c, nullptr);

    const int kThreads = 4;
    const uint64_t kIncsPerThread = 1000;

    std::vector<std::thread> threads;
    std::atomic<bool> start{ false };

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (uint64_t i = 0; i < kIncsPerThread; ++i)
                c->Inc();
        });
    }
    start.store(true);
    for (auto& t : threads) t.join();

    EXPECT_EQ(c->Value(), static_cast<uint64_t>(kThreads) * kIncsPerThread);
}

TEST_F(MetricCounterTest, IdempotentRegistration)
{
    Counter* c1 = MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("test.counter.idem"));
    Counter* c2 = MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("test.counter.idem"));
    EXPECT_EQ(c1, c2);
}

TEST_F(MetricCounterTest, FindReturnsNullForUnregistered)
{
    Counter* c = MetricRegistry::Instance().FindCounter(Dia::Core::StringCRC("test.counter.nothere"));
    EXPECT_EQ(c, nullptr);
}

TEST_F(MetricCounterTest, SnapshotContainsCounter)
{
    Counter* c = MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC("test.counter.snap"));
    c->Inc(7);

    MetricSnapshot snap;
    MetricRegistry::Instance().Snapshot(snap);

    bool found = false;
    for (unsigned int i = 0; i < snap.entryCount; ++i)
    {
        if (snap.entries[i].name == Dia::Core::StringCRC("test.counter.snap"))
        {
            EXPECT_EQ(snap.entries[i].kind, MetricEntry::Kind::kCounter);
            EXPECT_EQ(snap.entries[i].counterValue, 7u);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}
