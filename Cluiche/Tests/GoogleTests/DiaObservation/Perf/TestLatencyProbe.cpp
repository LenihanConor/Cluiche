// perf-live-control task 6: LatencyProbe (AC8), LatencyVisibleMatcher, _lat argument stripping.
#include <gtest/gtest.h>
#include <DiaObservation/Perf/LatencyProbe.h>
#include <DiaCore/Json/external/json/json.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using Dia::Core::StringCRC;
using namespace Dia::Observation::Perf;

namespace
{
    std::atomic<uint64_t> gNow{0};
    uint64_t FakeNow(void*) { return gNow.load(); }

    class LatencyProbeTest : public ::testing::Test
    {
    protected:
        LatencyProbeTest() : probe(&FakeNow, nullptr) { gNow = 1000; }
        LatencyProbe probe;
        const StringCRC action{"slider"};
        const StringCRC hopA{"hopA"};
        const StringCRC hopB{"hopB"};

        // Runs one probe of the given total duration (ns) with no hops.
        void RunProbe(const StringCRC& a, uint64_t durationNs)
        {
            const uint32_t id = probe.Begin(a);
            ASSERT_NE(0u, id);
            gNow += durationNs;
            probe.End(id);
        }
    };
}

TEST_F(LatencyProbeTest, HopDeltasAreReported)
{
    const uint32_t id = probe.Begin(action);
    ASSERT_NE(0u, id);
    gNow += 100;
    probe.Stamp(id, hopA);
    gNow += 250;
    probe.Stamp(id, hopB);
    gNow += 50;
    probe.End(id);

    CompletedProbe out[4];
    ASSERT_EQ(1u, probe.GetCompletions(action, out, 4));
    EXPECT_EQ(400u, out[0].totalNs);
    ASSERT_EQ(2u, out[0].hopCount);
    EXPECT_EQ(hopA.Value(), out[0].hops[0].hopCrc);
    EXPECT_EQ(100u, out[0].hops[0].sinceBeginNs);
    EXPECT_EQ(100u, out[0].hops[0].sincePrevNs);
    EXPECT_EQ(hopB.Value(), out[0].hops[1].hopCrc);
    EXPECT_EQ(350u, out[0].hops[1].sinceBeginNs);
    EXPECT_EQ(250u, out[0].hops[1].sincePrevNs);
}

TEST_F(LatencyProbeTest, FortyProbesGiveKnownP50AndP95)
{
    for (uint64_t i = 1; i <= 40; ++i)
        RunProbe(action, i * 1000);
    const LatencyStats s = probe.GetStats(action);
    EXPECT_EQ(40u, s.count);
    EXPECT_EQ(20000u, s.p50Ns);   // nearest rank: ceil(0.50*40)=20th
    EXPECT_EQ(38000u, s.p95Ns);   // ceil(0.95*40)=38th
}

TEST_F(LatencyProbeTest, RingWrapEvictsOldest)
{
    for (uint64_t i = 1; i <= 45; ++i)
        RunProbe(action, i * 1000);
    const LatencyStats s = probe.GetStats(action);
    EXPECT_EQ(40u, s.count);
    EXPECT_EQ(6000u, s.minNs);    // 1..5 evicted
    EXPECT_EQ(45000u, s.maxNs);
    CompletedProbe out[64];
    ASSERT_EQ(40u, probe.GetCompletions(action, out, 64));
    EXPECT_EQ(6000u, out[0].totalNs);      // oldest first
    EXPECT_EQ(45000u, out[39].totalNs);
}

TEST_F(LatencyProbeTest, UnknownStaleAndDoubleEndAreRejected)
{
    probe.End(0);
    probe.End(0xDEADBEEF);
    probe.Stamp(0xDEADBEEF, hopA);
    EXPECT_EQ(3u, probe.GetCounters().rejected);

    const uint32_t id = probe.Begin(action);
    probe.End(id);
    probe.End(id);                       // double end
    probe.Stamp(id, hopA);               // stamp after end
    EXPECT_EQ(5u, probe.GetCounters().rejected);
    EXPECT_EQ(1u, probe.GetStats(action).count);
}

TEST_F(LatencyProbeTest, ReusedSlotRejectsStaleId)
{
    const uint32_t first = probe.Begin(action);
    probe.End(first);
    uint32_t second = 0;
    // The freed slot is reused eventually; the id must differ because of the generation tag.
    for (unsigned i = 0; i < LatencyProbe::kMaxOpenProbes + 1 && second == 0; ++i)
    {
        const uint32_t id = probe.Begin(action);
        if (id != first) { second = id; probe.End(id); }
    }
    EXPECT_NE(first, second);
    const uint64_t before = probe.GetCounters().rejected;
    probe.Stamp(first, hopA);
    probe.End(first);
    EXPECT_EQ(before + 2, probe.GetCounters().rejected);
}

TEST_F(LatencyProbeTest, TableFullReturnsZero)
{
    uint32_t ids[LatencyProbe::kMaxOpenProbes];
    for (unsigned i = 0; i < LatencyProbe::kMaxOpenProbes; ++i)
    {
        ids[i] = probe.Begin(action);
        ASSERT_NE(0u, ids[i]);
    }
    EXPECT_EQ(0u, probe.Begin(action));
    EXPECT_EQ(1u, probe.GetCounters().tableFull);
    probe.End(ids[0]);
    EXPECT_NE(0u, probe.Begin(action));   // a freed slot is reusable
}

TEST_F(LatencyProbeTest, HopOverflowIsCountedAndExtraHopsDropped)
{
    const uint32_t id = probe.Begin(action);
    for (unsigned i = 0; i < LatencyProbe::kMaxHops + 3; ++i)
    {
        gNow += 10;
        probe.Stamp(id, hopA);
    }
    probe.End(id);
    EXPECT_EQ(3u, probe.GetCounters().hopOverflow);
    CompletedProbe out[1];
    ASSERT_EQ(1u, probe.GetCompletions(action, out, 1));
    EXPECT_EQ(LatencyProbe::kMaxHops, out[0].hopCount);
}

TEST_F(LatencyProbeTest, ActionsAreIndependent)
{
    const StringCRC other("other");
    RunProbe(action, 1000);
    RunProbe(other, 5000);
    RunProbe(other, 7000);
    EXPECT_EQ(1u, probe.GetStats(action).count);
    EXPECT_EQ(2u, probe.GetStats(other).count);
    EXPECT_EQ(0u, probe.GetStats(StringCRC("never")).count);
}

TEST_F(LatencyProbeTest, BeginOnOneThreadStampAndEndOnAnother)
{
    uint32_t id = 0;
    std::thread a([&] { id = probe.Begin(action); });
    a.join();
    ASSERT_NE(0u, id);
    std::thread b([&]
    {
        gNow += 500;
        probe.Stamp(id, hopA);
        gNow += 500;
        probe.End(id);
    });
    b.join();
    CompletedProbe out[1];
    ASSERT_EQ(1u, probe.GetCompletions(action, out, 1));
    EXPECT_EQ(1000u, out[0].totalNs);
    EXPECT_EQ(1u, out[0].hopCount);
}

TEST_F(LatencyProbeTest, ConcurrentProbesFromManyThreads)
{
    std::thread t[4];
    for (auto& th : t)
        th = std::thread([&]
        {
            for (int i = 0; i < 200; ++i)
            {
                const uint32_t id = probe.Begin(action);
                if (id) { probe.Stamp(id, hopA); probe.End(id); }
            }
        });
    for (auto& th : t) th.join();
    EXPECT_EQ(40u, probe.GetStats(action).count);
    EXPECT_EQ(0u, probe.GetCounters().rejected);
}

TEST_F(LatencyProbeTest, DefaultClockIsMonotonic)
{
    LatencyProbe real;
    const uint32_t id = real.Begin(action);
    ASSERT_NE(0u, id);
    real.End(id);
    EXPECT_EQ(1u, real.GetStats(action).count);
}

// ---- LatencyVisibleMatcher ----

TEST_F(LatencyProbeTest, MatcherCompletesOnFirstSnapshotAtOrAfterApplyFrame)
{
    LatencyVisibleMatcher m(probe);
    const uint32_t id = probe.Begin(action);
    gNow += 100;
    ASSERT_TRUE(m.NoteApplied(id, 10));
    gNow += 100;
    EXPECT_EQ(0u, m.OnSnapshot(9));
    EXPECT_EQ(0u, probe.GetStats(action).count);
    gNow += 100;
    EXPECT_EQ(1u, m.OnSnapshot(10));
    EXPECT_EQ(1u, probe.GetStats(action).count);
    EXPECT_EQ(0u, m.OnSnapshot(11));   // already completed

    CompletedProbe out[1];
    ASSERT_EQ(1u, probe.GetCompletions(action, out, 1));
    ASSERT_EQ(2u, out[0].hopCount);
    EXPECT_EQ(LatencyHops::Applied().Value(), out[0].hops[0].hopCrc);
    EXPECT_EQ(LatencyHops::Visible().Value(), out[0].hops[1].hopCrc);
    EXPECT_EQ(300u, out[0].totalNs);
}

TEST_F(LatencyProbeTest, MatcherCompletesOnLaterSnapshot)
{
    LatencyVisibleMatcher m(probe);
    const uint32_t id = probe.Begin(action);
    m.NoteApplied(id, 10);
    EXPECT_EQ(1u, m.OnSnapshot(15));
    EXPECT_EQ(1u, probe.GetStats(action).count);
}

TEST_F(LatencyProbeTest, MatcherHandlesSeveralPendingAndRejectsZeroId)
{
    LatencyVisibleMatcher m(probe);
    EXPECT_FALSE(m.NoteApplied(0, 5));
    const uint32_t a = probe.Begin(action);
    const uint32_t b = probe.Begin(action);
    m.NoteApplied(a, 4);
    m.NoteApplied(b, 8);
    EXPECT_EQ(1u, m.OnSnapshot(6));
    EXPECT_EQ(1u, m.OnSnapshot(8));
    EXPECT_EQ(2u, probe.GetStats(action).count);
}

TEST_F(LatencyProbeTest, MatcherFullRejectsExtraThenAcceptsAfterSnapshot)
{
    LatencyVisibleMatcher m(probe);
    uint32_t ids[LatencyVisibleMatcher::kCapacity + 1];
    for (unsigned int i = 0; i < LatencyVisibleMatcher::kCapacity + 1; ++i)
    {
        ids[i] = probe.Begin(action);
        ASSERT_NE(0u, ids[i]);
    }
    for (unsigned int i = 0; i < LatencyVisibleMatcher::kCapacity; ++i)
        EXPECT_TRUE(m.NoteApplied(ids[i], 10));
    EXPECT_FALSE(m.NoteApplied(ids[LatencyVisibleMatcher::kCapacity], 10));   // full: refused, not dropped silently
    EXPECT_EQ(LatencyVisibleMatcher::kCapacity, m.OnSnapshot(10));
    EXPECT_TRUE(m.NoteApplied(ids[LatencyVisibleMatcher::kCapacity], 11));    // space again
    EXPECT_EQ(1u, m.OnSnapshot(11));
}

TEST_F(LatencyProbeTest, MatcherWithStaleIdDoesNotCompleteAnything)
{
    LatencyVisibleMatcher m(probe);
    const uint32_t id = probe.Begin(action);
    probe.End(id);                       // already ended: id is stale
    const uint64_t before = probe.GetStats(action).count;
    m.NoteApplied(id, 3);
    m.OnSnapshot(3);
    EXPECT_EQ(before, probe.GetStats(action).count);   // no second completion
}

TEST(LatencyProbeBenchmark, BeginStampEndCostReported)
{
    LatencyProbe probe;   // real steady clock
    const StringCRC action("bench"), hop("hop");
    const int kIters = 200000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kIters; ++i)
    {
        const uint32_t id = probe.Begin(action);
        probe.Stamp(id, hop);
        probe.End(id);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kIters;
    std::printf("[ PERF ] LatencyProbe Begin+Stamp+End: %.1f ns per probe (reported, not gating)\n", ns);
    EXPECT_EQ(40u, probe.GetStats(action).count);
}

// ---- _lat argument ----

TEST(LatencyArgTest, StripRemovesLatAndKeepsOtherArgs)
{
    Json::Value args;
    args["value"] = 0.5;
    args["name"] = "x";
    args["_lat"] = 1234u;
    uint32_t id = 0;
    EXPECT_TRUE(StripLatencyArg(args, id));
    EXPECT_EQ(1234u, id);
    EXPECT_FALSE(args.isMember("_lat"));
    EXPECT_DOUBLE_EQ(0.5, args["value"].asDouble());
    EXPECT_EQ("x", args["name"].asString());
}

TEST(LatencyArgTest, StripWithoutLatIsNoOp)
{
    Json::Value args;
    args["value"] = 3;
    uint32_t id = 77;
    EXPECT_FALSE(StripLatencyArg(args, id));
    EXPECT_EQ(0u, id);
    EXPECT_EQ(3, args["value"].asInt());
}

TEST(LatencyArgTest, StripIgnoresNonNumericLatButRemovesIt)
{
    Json::Value args;
    args["_lat"] = "oops";
    uint32_t id = 5;
    EXPECT_FALSE(StripLatencyArg(args, id));
    EXPECT_FALSE(args.isMember("_lat"));
}

TEST(LatencyArgTest, StripOnNonObjectIsSafe)
{
    Json::Value args(Json::nullValue);
    uint32_t id = 0;
    EXPECT_FALSE(StripLatencyArg(args, id));
}
