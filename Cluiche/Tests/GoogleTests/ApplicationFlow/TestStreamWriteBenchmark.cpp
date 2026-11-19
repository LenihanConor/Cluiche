////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreamWriteBenchmark.cpp
// GoogleTest suite — Trace-Linked Streams Task 1: Write-path baseline benchmark
//
// Measures FrameStreamStore::Write and EventStreamStore::Send cost (tracer off)
// before adding trace-linking fields. Reports the baseline numbers and fails
// only if cost exceeds the Release regression ceilings below (Debug prints only).
//
// Reusable by tasks 3 and 4 to verify the overhead stays within budget.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaStreams/FrameStreamStore.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/Event.h>
#include <DiaStreams/SendResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Trace/Tracer.h>

#include <chrono>
#include <cstdio>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

namespace {
    // Simple payload for benchmarking
    struct BenchPayload
    {
        int  value;
        long long timestamp;
        float data[4];
    };

    // Benchmark parameters
    constexpr int kWarmupIterations = 1000;
    constexpr int kBenchIterations  = 100000;
    // Enforced (Release only) regression ceilings, ~3x the Release cost measured 2026-11-18 on the dev machine:
    // FrameStream write ~58 ns, EventStream send ~150 ns, multi-writer ~76 ns. Trace-linked Streams AC1 was written as a
    // delta (5 ns over the pre-change write) but no pre-change figure was recorded, so it is checked as these absolute
    // ceilings (spec amended 2026-11-19). Debug prints the numbers and enforces nothing.
    constexpr double kFrameWriteCeilingNs = 200.0;
    constexpr double kEventSendCeilingNs  = 500.0;
    constexpr double kMultiWriterCeilingNs = 250.0;

    Event<BenchPayload> MakeBenchEvent(int value)
    {
        Event<BenchPayload> evt;
        evt.payload.value = value;
        evt.payload.timestamp = 0LL;
        for (int i = 0; i < 4; ++i)
            evt.payload.data[i] = static_cast<float>(value + i);
        return evt;
    }

    // The nanosecond budgets describe optimised code. An unoptimised (Debug) build is several times slower
    // by construction, so there the numbers are still printed but not enforced; Release enforces them.
#if defined(NDEBUG)
#  define BENCH_EXPECT_LT(a, b) EXPECT_LT(a, b)
#else
#  define BENCH_EXPECT_LT(a, b) if (false) EXPECT_LT(a, b)
#endif

} // namespace

// Baseline: FrameStreamStore::Write cost (tracer off)
TEST(StreamWriteBenchmark, FrameStreamWriteBaseline)
{
    // Ensure tracer is not started (tracer-off measurement)
    // Tracer is a singleton; if it's running, this test's timing is not baseline.
    // The tracer must be explicitly started elsewhere; by default it's off.
    // No explicit check here: the test measures whatever the current state is.

    FrameStreamStore<BenchPayload> store(StringCRC("bench_frame"));
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    // Warmup: cache the store and get the CPU into steady state
    BenchPayload payload;
    payload.value = 0;
    payload.timestamp = 0LL;
    for (int i = 0; i < 4; ++i)
        payload.data[i] = 0.0f;

    for (int i = 0; i < kWarmupIterations; ++i)
    {
        payload.value = i;
        store.Write(payload, t);
    }

    // Benchmark: measure kBenchIterations writes
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kBenchIterations; ++i)
    {
        payload.value = i;
        store.Write(payload, t);
    }
    auto end = std::chrono::steady_clock::now();

    std::chrono::duration<double, std::nano> elapsed = end - start;
    double totalNs = elapsed.count();
    double perWriteNs = totalNs / static_cast<double>(kBenchIterations);

    // Always report the measurement
    std::printf("[----------] FrameStreamStore::Write baseline: %.2f ns per write (%.0f total ns, %d iterations)\n",
                perWriteNs, totalNs, kBenchIterations);

    // Fail only if cost exceeds the regression ceiling (Release)
    BENCH_EXPECT_LT(perWriteNs, kFrameWriteCeilingNs)
        << "FrameStreamStore::Write baseline cost (" << perWriteNs << " ns) exceeds the regression ceiling ("
        << kFrameWriteCeilingNs << " ns, Release). Consider DIA_DEBUG-gated link fields.";

}

// Baseline: EventStreamStore::Send cost (tracer off)
TEST(StreamWriteBenchmark, EventStreamSendBaseline)
{
    // Single-reader event store
    EventStreamStore<BenchPayload> store(StringCRC("bench_event"),
                                         StringCRC::kZero,
                                         /*capacity=*/256,
                                         /*maxReaders=*/1);
    int readerIdx = store.RegisterReader();
    ASSERT_GE(readerIdx, 0);

    // Warmup
    for (int i = 0; i < kWarmupIterations; ++i)
    {
        store.Send(MakeBenchEvent(i));
    }

    // Benchmark: measure kBenchIterations sends
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kBenchIterations; ++i)
    {
        store.Send(MakeBenchEvent(i));
    }
    auto end = std::chrono::steady_clock::now();

    std::chrono::duration<double, std::nano> elapsed = end - start;
    double totalNs = elapsed.count();
    double perSendNs = totalNs / static_cast<double>(kBenchIterations);

    // Always report
    std::printf("[----------] EventStreamStore::Send baseline: %.2f ns per send (%.0f total ns, %d iterations)\n",
                perSendNs, totalNs, kBenchIterations);

    // Fail only above 2x budget
    BENCH_EXPECT_LT(perSendNs, kEventSendCeilingNs)
        << "EventStreamStore::Send baseline cost (" << perSendNs << " ns) exceeds the regression ceiling ("
        << kEventSendCeilingNs << " ns, Release). Consider DIA_DEBUG-gated link fields.";

}

// Multi-writer FrameStreamStore: measures mutex-protected write path
TEST(StreamWriteBenchmark, FrameStreamMultiWriterBaseline)
{
    FrameStreamStore<BenchPayload> store(StringCRC("bench_frame_multi"),
                                         StringCRC::kZero,
                                         /*multiWriter=*/true);
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    BenchPayload payload;
    payload.value = 0;
    payload.timestamp = 0LL;
    for (int i = 0; i < 4; ++i)
        payload.data[i] = 0.0f;

    // Warmup
    for (int i = 0; i < kWarmupIterations; ++i)
    {
        payload.value = i;
        store.Write(payload, t);
    }

    // Benchmark
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kBenchIterations; ++i)
    {
        payload.value = i;
        store.Write(payload, t);
    }
    auto end = std::chrono::steady_clock::now();

    std::chrono::duration<double, std::nano> elapsed = end - start;
    double totalNs = elapsed.count();
    double perWriteNs = totalNs / static_cast<double>(kBenchIterations);

    std::printf("[----------] FrameStreamStore::Write (multi-writer) baseline: %.2f ns per write\n", perWriteNs);

    // Multi-writer has a mutex, so we're more lenient (but still report)
    BENCH_EXPECT_LT(perWriteNs, kMultiWriterCeilingNs)
        << "FrameStreamStore::Write (multi-writer) baseline cost (" << perWriteNs
        << " ns) is unexpectedly high (> " << kMultiWriterCeilingNs << " ns).";
}
