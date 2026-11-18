////////////////////////////////////////////////////////////////////////////////
// Filename: TestEventStreamLink.cpp
// GoogleTest suite — Trace-Linked Streams Task 4: EventStream link capture
// and Event<T> envelope (AC3)
////////////////////////////////////////////////////////////////////////////////
#include <DiaObservation/Testing/UniqueTestPath.h>
#include <gtest/gtest.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/Event.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaObservation/Trace/ScopedZone.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <thread>

using namespace Dia::ApplicationFlow;
using namespace Dia::Observation::Trace;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

namespace {
    const Dia::Observation::Testing::UniqueTestPath kTestTraceFile("test_eventstream_link.jsonl");

    void CleanupTestFile()
    {
        std::remove(kTestTraceFile);
        Tracer::Instance().UnregisterThreadSpanBuffer();   // do not leave this thread's ring registered for later tests
    }

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
}

// AC3: Event<T> link fields are zero when tracer is off
TEST(EventStreamLink, EventLinkFieldsZeroWhenTracerOff)
{
    EventStreamStore<TestPayload> store(StringCRC("event_tracer_off"));
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    store.Send(MakeEvent(42));

    DynamicArrayC<Event<TestPayload>, 32> events;
    store.Consume(rIdx, events);

    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].payload.value, 42);
    EXPECT_EQ(events[0].traceId, 0u);
    EXPECT_EQ(events[0].spanId, 0u);
}

// AC3: Event<T> link fields are zero when tracer is on but no span open
TEST(EventStreamLink, EventLinkFieldsZeroWhenNoSpanOpen)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    EventStreamStore<TestPayload> store(StringCRC("event_no_span"));
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    store.Send(MakeEvent(100));

    DynamicArrayC<Event<TestPayload>, 32> events;
    store.Consume(rIdx, events);

    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].payload.value, 100);
    EXPECT_EQ(events[0].traceId, 0u) << "Link should be zero when no span open";
    EXPECT_EQ(events[0].spanId, 0u);

    tracer.Stop();
    CleanupTestFile();
}

// AC3: Event<T> link fields non-zero when span is open during send
TEST(EventStreamLink, EventLinkFieldsNonZeroWhenSpanOpen)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    EventStreamStore<TestPayload> store(StringCRC("event_with_span"));
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    {
        ScopedZone zone(StringCRC("WriterSpan"), Category::kAll);
        store.Send(MakeEvent(200));
    }

    DynamicArrayC<Event<TestPayload>, 32> events;
    store.Consume(rIdx, events);

    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].payload.value, 200);
    EXPECT_NE(events[0].traceId, 0u) << "Link should be non-zero when span open";
    EXPECT_NE(events[0].spanId, 0u);
    EXPECT_GT(events[0].writeSteadyNs, 0u);

    tracer.Stop();
    CleanupTestFile();
}

// AC3: Link survives across threads
TEST(EventStreamLink, LinkSurvivesAcrossThreads)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    EventStreamStore<TestPayload> store(StringCRC("event_cross_thread"));
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    uint64_t writerTraceId = 0;
    uint64_t writerSpanId = 0;

    // Main thread: send with span
    {
        ScopedZone writerZone(StringCRC("MainThreadWriter"), Category::kAll);
        TraceLink currentLink = Tracer::GetCurrentLink();
        writerTraceId = currentLink.traceId;
        writerSpanId = currentLink.spanId;
        store.Send(MakeEvent(300));
    }

    // Worker thread: consume events
    std::thread worker([&]() {
        tracer.RegisterThreadSpanBuffer();
        struct Unregister { ~Unregister() { Tracer::Instance().UnregisterThreadSpanBuffer(); } } unregisterOnExit;

        DynamicArrayC<Event<TestPayload>, 32> events;
        store.Consume(rIdx, events);

        ASSERT_EQ(events.Size(), 1u);
        EXPECT_EQ(events[0].payload.value, 300);
        EXPECT_EQ(events[0].traceId, writerTraceId) << "Link traceId should match writer";
        EXPECT_EQ(events[0].spanId, writerSpanId) << "Link spanId should match writer";
    });
    worker.join();

    tracer.Stop();
    CleanupTestFile();
}

// AC3: Frame batching preserves link (ConsumeUpToFlush)
TEST(EventStreamLink, FrameBatchingPreservesLink)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    EventStreamStore<TestPayload> store(StringCRC("event_batching"));
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    uint64_t spanId1 = 0;
    uint64_t spanId2 = 0;

    // Send two events in different batches
    {
        ScopedZone zone1(StringCRC("Span1"), Category::kAll);
        spanId1 = Tracer::GetCurrentLink().spanId;
        store.Send(MakeEvent(100));
    }
    store.Flush();

    {
        ScopedZone zone2(StringCRC("Span2"), Category::kAll);
        spanId2 = Tracer::GetCurrentLink().spanId;
        store.Send(MakeEvent(200));
    }
    store.Flush();

    ASSERT_NE(spanId1, spanId2);

    // Consume all events
    DynamicArrayC<Event<TestPayload>, 32> events;
    store.ConsumeUpToFlush(rIdx, events);

    ASSERT_EQ(events.Size(), 2u);
    EXPECT_EQ(events[0].payload.value, 100);
    EXPECT_EQ(events[0].spanId, spanId1) << "First event should have Span1 link";
    EXPECT_EQ(events[1].payload.value, 200);
    EXPECT_EQ(events[1].spanId, spanId2) << "Second event should have Span2 link";

    tracer.Stop();
    CleanupTestFile();
}

// AC3: Dropped events don't leave stale links
TEST(EventStreamLink, DroppedEventDoesNotLeaveStaleLink)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    // Small capacity (2 events) to trigger overflow
    EventStreamStore<TestPayload> store(StringCRC("event_drop"),
                                        StringCRC::kZero,
                                        /*capacity=*/2);
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    uint64_t span1Id = 0;
    uint64_t span2Id = 0;
    uint64_t span3Id = 0;

    // Send 3 events (drops oldest)
    {
        ScopedZone zone1(StringCRC("Span1"), Category::kAll);
        span1Id = Tracer::GetCurrentLink().spanId;
        store.Send(MakeEvent(1));  // Will be dropped
    }

    {
        ScopedZone zone2(StringCRC("Span2"), Category::kAll);
        span2Id = Tracer::GetCurrentLink().spanId;
        store.Send(MakeEvent(2));
    }

    {
        ScopedZone zone3(StringCRC("Span3"), Category::kAll);
        span3Id = Tracer::GetCurrentLink().spanId;
        store.Send(MakeEvent(3));
    }

    ASSERT_NE(span1Id, span2Id);
    ASSERT_NE(span2Id, span3Id);

    DynamicArrayC<Event<TestPayload>, 32> events;
    store.Consume(rIdx, events);

    ASSERT_EQ(events.Size(), 2u) << "Only 2 events fit in buffer";
    EXPECT_EQ(events[0].payload.value, 2);
    EXPECT_EQ(events[0].spanId, span2Id) << "Kept event should have correct link";
    EXPECT_EQ(events[1].payload.value, 3);
    EXPECT_EQ(events[1].spanId, span3Id);

    tracer.Stop();
    CleanupTestFile();
}

// AC3: Multiple sends overwrite link per event
TEST(EventStreamLink, MultipleSendsHaveIndependentLinks)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    EventStreamStore<TestPayload> store(StringCRC("event_multiple"));
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    uint64_t span1 = 0;
    uint64_t span2 = 0;
    uint64_t span3 = 0;

    {
        ScopedZone zone1(StringCRC("S1"), Category::kAll);
        span1 = Tracer::GetCurrentLink().spanId;
        store.Send(MakeEvent(1));
    }

    {
        ScopedZone zone2(StringCRC("S2"), Category::kAll);
        span2 = Tracer::GetCurrentLink().spanId;
        store.Send(MakeEvent(2));
    }

    {
        ScopedZone zone3(StringCRC("S3"), Category::kAll);
        span3 = Tracer::GetCurrentLink().spanId;
        store.Send(MakeEvent(3));
    }

    ASSERT_NE(span1, span2);
    ASSERT_NE(span2, span3);

    DynamicArrayC<Event<TestPayload>, 32> events;
    store.Consume(rIdx, events);

    ASSERT_EQ(events.Size(), 3u);
    EXPECT_EQ(events[0].spanId, span1);
    EXPECT_EQ(events[1].spanId, span2);
    EXPECT_EQ(events[2].spanId, span3);

    tracer.Stop();
    CleanupTestFile();
}
