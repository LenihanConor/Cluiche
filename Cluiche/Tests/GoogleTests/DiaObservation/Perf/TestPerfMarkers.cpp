// perf-live-control task 1: PerfMarkers ring, macros, cross-thread snapshot.
#include <gtest/gtest.h>
#include <DiaObservation/Perf/PerfMarkers.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using Dia::Observation::Perf::PerfMarker;
using Dia::Observation::Perf::PerfMarkers;

namespace
{
    class PerfMarkerTest : public ::testing::Test
    {
    protected:
        void SetUp() override    { PerfMarkers::RegisterThread(); }
        void TearDown() override { PerfMarkers::UnregisterThread(); }

        static unsigned int Snap(std::vector<PerfMarker>& v, uint64_t* dropped = nullptr)
        {
            v.assign(PerfMarkers::kRingCapacity * 4, PerfMarker());
            const unsigned int n = PerfMarkers::Snapshot(v.data(), static_cast<unsigned int>(v.size()), dropped);
            v.resize(n);
            return n;
        }
    };
}

TEST_F(PerfMarkerTest, RingKeepsOrderAndNoDrops)
{
    const uint64_t droppedBase = PerfMarkers::TotalDropped();   // process-wide: other tests' unregistered threads also count
    for (uint32_t i = 1; i <= 10; ++i)
        PerfMarkers::Mark(i);

    std::vector<PerfMarker> v;
    uint64_t dropped = 99;
    ASSERT_EQ(10u, Snap(v, &dropped));
    EXPECT_EQ(0u, dropped - droppedBase);
    for (uint32_t i = 0; i < 10; ++i)
    {
        EXPECT_EQ(i + 1, v[i].nameCrc);
        if (i > 0) EXPECT_GE(v[i].steadyNs, v[i - 1].steadyNs);
    }
}

TEST_F(PerfMarkerTest, ValueFlagIsOptional)
{
    PerfMarkers::Mark(1);
    PerfMarkers::MarkValue(2, 0);
    PerfMarkers::MarkValue(3, 0xFFFFFFFFFFFFull);

    std::vector<PerfMarker> v;
    ASSERT_EQ(3u, Snap(v));
    EXPECT_FALSE(v[0].hasValue);
    EXPECT_TRUE(v[1].hasValue);
    EXPECT_EQ(0u, v[1].value);
    EXPECT_TRUE(v[2].hasValue);
    EXPECT_EQ(0xFFFFFFFFFFFFull, v[2].value);
}

TEST_F(PerfMarkerTest, FullRingExactlyAtCapacityDropsNothing)
{
    const uint64_t droppedBase = PerfMarkers::TotalDropped();
    for (uint32_t i = 0; i < PerfMarkers::kRingCapacity; ++i)
        PerfMarkers::Mark(i);

    std::vector<PerfMarker> v;
    uint64_t dropped = 99;
    ASSERT_EQ(PerfMarkers::kRingCapacity, Snap(v, &dropped));
    EXPECT_EQ(0u, dropped - droppedBase);
    EXPECT_EQ(0u, v.front().nameCrc);
}

TEST_F(PerfMarkerTest, OverflowDropsOldestAndCounts)
{
    const uint64_t droppedBase = PerfMarkers::TotalDropped();
    const uint32_t total = PerfMarkers::kRingCapacity + 100;
    for (uint32_t i = 0; i < total; ++i)
        PerfMarkers::Mark(i);

    std::vector<PerfMarker> v;
    uint64_t dropped = 0;
    ASSERT_EQ(PerfMarkers::kRingCapacity, Snap(v, &dropped));
    EXPECT_EQ(100u, dropped - droppedBase);
    EXPECT_EQ(100u, v.front().nameCrc);          // oldest 100 gone
    EXPECT_EQ(total - 1, v.back().nameCrc);
}

TEST_F(PerfMarkerTest, SnapshotMaxReturnsNewest)
{
    for (uint32_t i = 0; i < 50; ++i)
        PerfMarkers::Mark(i);

    PerfMarker out[5];
    ASSERT_EQ(5u, PerfMarkers::Snapshot(out, 5));
    EXPECT_EQ(45u, out[0].nameCrc);
    EXPECT_EQ(49u, out[4].nameCrc);
}

TEST_F(PerfMarkerTest, ReRegisterResetsRing)
{
    PerfMarkers::Mark(1);
    PerfMarkers::UnregisterThread();
    PerfMarkers::RegisterThread();

    std::vector<PerfMarker> v;
    EXPECT_EQ(0u, Snap(v));
}

TEST_F(PerfMarkerTest, MacrosRecordNameCrcAndValue)
{
    DIA_PERF_MARK("perf.test.plain");
    DIA_PERF_MARK_VALUE("perf.test.valued", 1234);

    std::vector<PerfMarker> v;
    ASSERT_EQ(2u, Snap(v));
    EXPECT_EQ(Dia::Core::StringCRC("perf.test.plain").Value(), v[0].nameCrc);
    EXPECT_FALSE(v[0].hasValue);
    EXPECT_EQ(Dia::Core::StringCRC("perf.test.valued").Value(), v[1].nameCrc);
    EXPECT_TRUE(v[1].hasValue);
    EXPECT_EQ(1234u, v[1].value);
}

TEST(PerfMarkerUnregistered, MarksAreDroppedAndCounted)
{
    uint64_t before = 0;
    PerfMarkers::Snapshot(nullptr, 0, &before);

    std::thread t([]
    {
        EXPECT_FALSE(PerfMarkers::IsThreadRegistered());
        for (int i = 0; i < 25; ++i)
            PerfMarkers::Mark(7);
        PerfMarkers::MarkValue(8, 1);
    });
    t.join();

    uint64_t after = 0;
    PerfMarker out[8];
    EXPECT_EQ(0u, PerfMarkers::Snapshot(out, 8, &after));
    EXPECT_EQ(before + 26u, after);
}

TEST(PerfMarkerUnregistered, RegistrationBeyondThreadCapDropsAndCounts)
{
    std::atomic<bool> release{false};
    std::atomic<unsigned int> registered{0};
    std::vector<std::thread> threads;
    const unsigned int count = PerfMarkers::kMaxThreads + 2;
    std::atomic<unsigned int> ready{0};
    for (unsigned int i = 0; i < count; ++i)
    {
        threads.emplace_back([&]
        {
            PerfMarkers::RegisterThread();
            if (PerfMarkers::IsThreadRegistered()) registered.fetch_add(1);
            PerfMarkers::Mark(1);   // must not crash either way
            ready.fetch_add(1);
            while (!release.load()) std::this_thread::yield();
            PerfMarkers::UnregisterThread();
        });
    }
    while (ready.load() < count) std::this_thread::yield();
    EXPECT_LE(registered.load(), PerfMarkers::kMaxThreads);
    release.store(true);
    for (auto& t : threads) t.join();
}

TEST_F(PerfMarkerTest, MergesTwoThreadsByTime)
{
    std::atomic<int> stage{0};
    std::thread helper([&]
    {
        PerfMarkers::RegisterThread();
        while (stage.load() < 1) std::this_thread::yield();
        PerfMarkers::Mark(2);
        stage.store(2);
        while (stage.load() < 3) std::this_thread::yield();
        PerfMarkers::UnregisterThread();
    });

    // Helper registers asynchronously; marks are sequenced by stage, 2 ms apart.
    PerfMarkers::Mark(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    stage.store(1);
    while (stage.load() < 2) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    PerfMarkers::Mark(3);

    std::vector<PerfMarker> v;
    ASSERT_EQ(3u, Snap(v));
    EXPECT_EQ(1u, v[0].nameCrc);
    EXPECT_EQ(2u, v[1].nameCrc);
    EXPECT_EQ(3u, v[2].nameCrc);
    EXPECT_NE(v[0].threadId, v[1].threadId);
    EXPECT_EQ(v[0].threadId, v[2].threadId);

    stage.store(3);
    helper.join();
}

TEST(PerfMarkerThreads, CrossThreadReaderNeverSeesTornEntries)
{
    std::atomic<bool> producerReady{false};
    std::atomic<bool> stop{false};
    std::atomic<bool> readerDone{false};

    std::thread producer([&]
    {
        PerfMarkers::RegisterThread();
        producerReady.store(true);
        uint32_t i = 1;
        while (!stop.load(std::memory_order_relaxed))
        {
            // Internally consistent entry: value == f(nameCrc)
            PerfMarkers::MarkValue(i, static_cast<uint64_t>(i) * 7u + 3u);
            ++i;
        }
        while (!readerDone.load()) std::this_thread::yield();
        PerfMarkers::UnregisterThread();
    });

    while (!producerReady.load()) std::this_thread::yield();

    std::vector<PerfMarker> v(PerfMarkers::kRingCapacity * 2);
    unsigned int totalChecked = 0;
    unsigned int bad = 0;
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < until)
    {
        const unsigned int n = PerfMarkers::Snapshot(v.data(), static_cast<unsigned int>(v.size()));
        for (unsigned int k = 0; k < n; ++k)
        {
            ++totalChecked;
            if (!v[k].hasValue || v[k].value != static_cast<uint64_t>(v[k].nameCrc) * 7u + 3u)
                ++bad;
            if (k > 0 && (v[k].steadyNs < v[k - 1].steadyNs || v[k].nameCrc <= v[k - 1].nameCrc))
                ++bad;
        }
    }
    stop.store(true);
    readerDone.store(true);
    producer.join();

    EXPECT_GT(totalChecked, 0u);
    EXPECT_EQ(0u, bad);
}

TEST_F(PerfMarkerTest, BenchmarkMarkCostReported)
{
    const int kIters = 1000000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kIters; ++i)
        DIA_PERF_MARK("perf.bench");
    const auto t1 = std::chrono::steady_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kIters;
    RecordProperty("ns_per_mark", std::to_string(ns));
    std::printf("[ PERF ] DIA_PERF_MARK: %.1f ns per mark (target < 50, reported not gating)\n", ns);
    SUCCEED();
}
