////////////////////////////////////////////////////////////////////////////////
// Filename: TestFrameStreamLink.cpp
// GoogleTest suite — Trace-Linked Streams Task 3: FrameStream link capture
// and FetchLatestLinked (AC2)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaStreams/FrameStreamStore.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaObservation/Trace/ScopedZone.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>

#include <thread>
#include <chrono>

using namespace Dia::ApplicationFlow;
using namespace Dia::Observation::Trace;
using namespace Dia::Core;

namespace {
    const char* kTestTraceFile = "test_framestream_link.jsonl";

    void CleanupTestFile()
    {
        std::remove(kTestTraceFile);
    }

    struct TestPayload
    {
        int value;
        float data;
    };
}

// AC2: FetchLatestLinked returns invalid link when no data written
TEST(FrameStreamLink, FetchLatestLinkedReturnsInvalidWhenNoData)
{
    FrameStreamStore<TestPayload> store(StringCRC("frame_no_data"));
    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);

    EXPECT_EQ(data, nullptr);
    EXPECT_FALSE(link.valid) << "Link should be invalid when no data written";
}

// AC2: FetchLatestLinked returns invalid link when tracer is off
TEST(FrameStreamLink, FetchLatestLinkedReturnsInvalidLinkWhenTracerOff)
{
    FrameStreamStore<TestPayload> store(StringCRC("frame_tracer_off"));
    TestPayload payload = { 42, 3.14f };
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    store.Write(payload, t);

    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 42);
    EXPECT_FALSE(link.valid) << "Link should be invalid when tracer is not started";
    EXPECT_EQ(link.traceId, 0u);
    EXPECT_EQ(link.spanId, 0u);
}

// AC2: FetchLatestLinked returns invalid link when tracer is on but no span is open
TEST(FrameStreamLink, FetchLatestLinkedReturnsInvalidLinkWhenNoSpanOpen)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    FrameStreamStore<TestPayload> store(StringCRC("frame_no_span"));
    TestPayload payload = { 100, 1.5f };
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    store.Write(payload, t);

    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 100);
    EXPECT_FALSE(link.valid) << "Link should be invalid when no span is open";
    EXPECT_EQ(link.traceId, 0u);
    EXPECT_EQ(link.spanId, 0u);

    tracer.Stop();
    CleanupTestFile();
}

// AC2: FetchLatestLinked returns valid link when span is open during write
TEST(FrameStreamLink, FetchLatestLinkedReturnsValidLinkWhenSpanOpen)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    FrameStreamStore<TestPayload> store(StringCRC("frame_with_span"));
    TestPayload payload = { 200, 2.5f };
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    {
        ScopedZone zone(StringCRC("WriterSpan"), Category::kAll);
        store.Write(payload, t);
    }

    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 200);
    EXPECT_TRUE(link.valid) << "Link should be valid when span was open during write";
    EXPECT_NE(link.traceId, 0u);
    EXPECT_NE(link.spanId, 0u);
    EXPECT_GT(link.writeSteadyNs, 0u);

    tracer.Stop();
    CleanupTestFile();
}

// AC2: Link survives across threads (two-thread test)
TEST(FrameStreamLink, LinkSurvivesAcrossThreads)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    FrameStreamStore<TestPayload> store(StringCRC("frame_cross_thread"));
    TestPayload payload = { 300, 3.5f };
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    uint64_t writerTraceId = 0;
    uint64_t writerSpanId = 0;

    // Main thread: write with span
    {
        ScopedZone writerZone(StringCRC("MainThreadWriter"), Category::kAll);
        TraceLink currentLink = Tracer::GetCurrentLink();
        writerTraceId = currentLink.traceId;
        writerSpanId = currentLink.spanId;
        store.Write(payload, t);
    }

    // Worker thread: read the link
    std::thread worker([&]() {
        tracer.RegisterThreadSpanBuffer();

        StreamLink link;
        const TestPayload* data = store.FetchLatestLinked(link);

        ASSERT_NE(data, nullptr);
        EXPECT_EQ(data->value, 300);
        EXPECT_TRUE(link.valid);
        EXPECT_EQ(link.traceId, writerTraceId) << "Link traceId should match writer";
        EXPECT_EQ(link.spanId, writerSpanId) << "Link spanId should match writer";
    });
    worker.join();

    tracer.Stop();
    CleanupTestFile();
}

// AC2: Multiple writes overwrite link (latest write wins)
TEST(FrameStreamLink, MultipleWritesOverwriteLink)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    FrameStreamStore<TestPayload> store(StringCRC("frame_multiple_writes"));
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    uint64_t firstSpanId = 0;
    uint64_t secondSpanId = 0;

    // First write with span
    {
        ScopedZone zone1(StringCRC("FirstSpan"), Category::kAll);
        firstSpanId = Tracer::GetCurrentLink().spanId;
        TestPayload payload1 = { 100, 1.0f };
        store.Write(payload1, t);
    }

    // Second write with different span
    {
        ScopedZone zone2(StringCRC("SecondSpan"), Category::kAll);
        secondSpanId = Tracer::GetCurrentLink().spanId;
        TestPayload payload2 = { 200, 2.0f };
        store.Write(payload2, t);
    }

    ASSERT_NE(firstSpanId, secondSpanId) << "Span IDs should differ";

    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 200) << "Latest write should be fetched";
    EXPECT_TRUE(link.valid);
    EXPECT_EQ(link.spanId, secondSpanId) << "Link should match the second (latest) write";

    tracer.Stop();
    CleanupTestFile();
}

// AC2: FetchLatest (no link) still works unchanged
TEST(FrameStreamLink, FetchLatestStillWorksWithoutLink)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    FrameStreamStore<TestPayload> store(StringCRC("frame_fetch_latest"));
    TestPayload payload = { 400, 4.5f };
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    {
        ScopedZone zone(StringCRC("WriteSpan"), Category::kAll);
        store.Write(payload, t);
    }

    // FetchLatest should work as before (no link exposed)
    const TestPayload* data = store.FetchLatest();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 400);

    tracer.Stop();
    CleanupTestFile();
}

// AC2: Ring wrap preserves link in new slot
TEST(FrameStreamLink, RingWrapPreservesLink)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    FrameStreamStore<TestPayload> store(StringCRC("frame_ring_wrap"));
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);

    uint64_t lastSpanId = 0;

    // Write more than ring capacity (8) entries
    for (int i = 0; i < 10; ++i)
    {
        ScopedZone zone(StringCRC("WrapSpan"), Category::kAll);
        lastSpanId = Tracer::GetCurrentLink().spanId;
        TestPayload payload = { i, static_cast<float>(i) };
        store.Write(payload, t);
    }

    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 9) << "Latest write after ring wrap";
    EXPECT_TRUE(link.valid);
    EXPECT_EQ(link.spanId, lastSpanId);

    tracer.Stop();
    CleanupTestFile();
}
