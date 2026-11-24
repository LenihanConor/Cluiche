////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreamLinkEdgeCases.cpp
// GoogleTest suite — Trace-Linked Streams Task 8: Edge cases and gap audit
// (ring wrap, no-open-span, multi-writer, tracer start/stop, deep stack)
////////////////////////////////////////////////////////////////////////////////
#include <DiaObservation/Testing/UniqueTestPath.h>
#include <gtest/gtest.h>
#include <DiaStreams/FrameStreamStore.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/Event.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaObservation/Trace/ScopedZone.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cstdio>

using namespace Dia::ApplicationFlow;
using namespace Dia::Observation::Trace;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

namespace {
    const Dia::Observation::Testing::UniqueTestPath kTestTraceFile("test_edge_cases_trace.jsonl");

    struct TestPayload
    {
        int value;
    };

    Event<TestPayload> MakeEvent(int value)
    {
        Event<TestPayload> evt;
        evt.payload.value = value;
        evt.timestampUs = 0;
        return evt;
    }

    void CleanupTestFile()
    {
        std::remove(kTestTraceFile);
    }
}

// Task 8: FrameStreamStore ring wrap (8 slots, 10 writes)
TEST(StreamLinkEdgeCases, FrameStreamRingWrap)
{
    CleanupTestFile();
    FrameStreamStore<TestPayload> store(StringCRC("ring_wrap"));
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "edge-test", 0));
    tracer.RegisterThreadSpanBuffer();

    // Write 10 times to wrap the 8-slot ring
    for (int i = 0; i < 10; ++i)
    {
        ScopedZone zone(StringCRC("Write"), Category::kAll);
        TestPayload payload = { i };
        store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));
    }

    // The last write (i=9) should be readable with its link
    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 9);
    EXPECT_TRUE(link.valid) << "Link should be valid after ring wrap";
    EXPECT_NE(link.traceId, 0u) << "traceId should be non-zero";
    EXPECT_NE(link.spanId, 0u) << "spanId should be non-zero";

    tracer.UnregisterThreadSpanBuffer();
    tracer.Stop();
    CleanupTestFile();
}

// Task 8: EventStreamStore ring wrap
TEST(StreamLinkEdgeCases, EventStreamRingWrap)
{
    CleanupTestFile();
    EventStreamStore<TestPayload> store(StringCRC("event_wrap"), StringCRC::kZero, 4);
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "edge-test", 0));
    tracer.RegisterThreadSpanBuffer();

    // Send 6 events to a 4-capacity store (wraps)
    for (int i = 0; i < 6; ++i)
    {
        ScopedZone zone(StringCRC("Send"), Category::kAll);
        store.Send(MakeEvent(i));
    }

    // Consume: oldest 2 were dropped, so we get 4 most recent (2, 3, 4, 5)
    DynamicArrayC<Event<TestPayload>, 32> events;
    store.Consume(rIdx, events);
    EXPECT_EQ(events.Size(), 4u);
    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        EXPECT_TRUE(events[i].traceId != 0 && events[i].spanId != 0) << "Event " << i << " should have valid link";
    }

    tracer.UnregisterThreadSpanBuffer();
    tracer.Stop();
    CleanupTestFile();
}

// Task 8: Write with no open span (tracer off or no zone)
TEST(StreamLinkEdgeCases, NoOpenSpanWriter)
{
    CleanupTestFile();
    FrameStreamStore<TestPayload> store(StringCRC("no_span"));
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "edge-test", 0));
    tracer.RegisterThreadSpanBuffer();

    // Write without opening a zone
    TestPayload payload = { 42 };
    store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));

    // Link should be invalid (zeros)
    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 42);
    EXPECT_FALSE(link.valid) << "Link should be invalid when no span is open";
    EXPECT_EQ(link.traceId, 0u);
    EXPECT_EQ(link.spanId, 0u);

    tracer.UnregisterThreadSpanBuffer();
    tracer.Stop();
    CleanupTestFile();
}

// Task 8: Write with tracer stopped mid-run
TEST(StreamLinkEdgeCases, TracerStartStopMidRun)
{
    CleanupTestFile();
    FrameStreamStore<TestPayload> store(StringCRC("tracer_toggle"));
    Tracer& tracer = Tracer::Instance();

    // Write 1: tracer off
    {
        TestPayload payload = { 1 };
        store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));
    }

    // Write 2: tracer on
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "edge-test", 0));
    tracer.RegisterThreadSpanBuffer();
    {
        ScopedZone zone(StringCRC("Write2"), Category::kAll);
        TestPayload payload = { 2 };
        store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));
    }
    tracer.UnregisterThreadSpanBuffer();
    tracer.Stop();

    // Write 3: tracer off again
    {
        TestPayload payload = { 3 };
        store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));
    }

    // Latest write (3) should have no link
    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 3);
    EXPECT_FALSE(link.valid) << "Link should be invalid when tracer was stopped";
    CleanupTestFile();
}

// Task 8: Link with a full open-span stack (64 deep)
TEST(StreamLinkEdgeCases, DeepSpanStack)
{
    CleanupTestFile();
    FrameStreamStore<TestPayload> store(StringCRC("deep_stack"));
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "edge-test", 0));
    tracer.RegisterThreadSpanBuffer();

    // Open 63 spans (stack depth 63) then write
    std::vector<ScopedZone*> zones;
    for (int i = 0; i < 63; ++i)
    {
        zones.push_back(new ScopedZone(StringCRC("StackLevel"), Category::kAll));
    }

    // Write at depth 63
    {
        ScopedZone writeZone(StringCRC("WriteAtDepth63"), Category::kAll);
        TestPayload payload = { 999 };
        store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));

        StreamLink link;
        const TestPayload* data = store.FetchLatestLinked(link);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(data->value, 999);
        EXPECT_TRUE(link.valid) << "Link should be valid even at deep stack depth";
        EXPECT_NE(link.spanId, 0u);
    }

    // Clean up stack
    for (auto* z : zones)
        delete z;

    tracer.UnregisterThreadSpanBuffer();
    tracer.Stop();
    CleanupTestFile();
}

// Task 8: Multi-writer FrameStreamStore with links
TEST(StreamLinkEdgeCases, MultiWriterStore)
{
    CleanupTestFile();
    FrameStreamStore<TestPayload> store(StringCRC("multi_writer"), StringCRC::kZero, /*multiWriter=*/true);
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "edge-test", 0));
    tracer.RegisterThreadSpanBuffer();

    // Write from "thread 1" (simulated by writing twice with different spans)
    {
        ScopedZone zone1(StringCRC("Writer1"), Category::kAll);
        TestPayload payload = { 100 };
        store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));
    }

    {
        ScopedZone zone2(StringCRC("Writer2"), Category::kAll);
        TestPayload payload = { 200 };
        store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));
    }

    // Latest write (200) should have a valid link
    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->value, 200);
    EXPECT_TRUE(link.valid);
    EXPECT_NE(link.spanId, 0u);

    tracer.UnregisterThreadSpanBuffer();
    tracer.Stop();
    CleanupTestFile();
}

// Task 8: EventStream link with dropped events
TEST(StreamLinkEdgeCases, EventStreamDroppedEventsKeepLinks)
{
    CleanupTestFile();
    EventStreamStore<TestPayload> store(StringCRC("drop_test"), StringCRC::kZero, 2);
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "edge-test", 0));
    tracer.RegisterThreadSpanBuffer();

    // Send 4 events to a 2-capacity store (2 oldest dropped)
    for (int i = 0; i < 4; ++i)
    {
        ScopedZone zone(StringCRC("Send"), Category::kAll);
        store.Send(MakeEvent(i));
    }

    // Consume the 2 surviving events (2 and 3)
    DynamicArrayC<Event<TestPayload>, 32> events;
    store.Consume(rIdx, events);
    ASSERT_EQ(events.Size(), 2u);
    EXPECT_EQ(events[0].payload.value, 2);
    EXPECT_EQ(events[1].payload.value, 3);
    // Both should have valid links
    EXPECT_TRUE(events[0].traceId != 0 && events[0].spanId != 0);
    EXPECT_TRUE(events[1].traceId != 0 && events[1].spanId != 0);

    tracer.UnregisterThreadSpanBuffer();
    tracer.Stop();
    CleanupTestFile();
}
