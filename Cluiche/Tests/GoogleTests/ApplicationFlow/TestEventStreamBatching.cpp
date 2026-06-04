////////////////////////////////////////////////////////////////////////////////
// TestEventStreamBatching.cpp
// GoogleTest suite — EventStream frame-batching (flush sequence semantics)
//
// Task 14 (TDD RED): tests written against the flush API that task 5 adds to
// EventStreamStore. The Flush() method, mFlushSequence field, and the
// corresponding Consume() batch-boundary logic are NOT yet implemented.
//
// These tests will fail (RED) until task 5 wires in EventStreamStore::Flush().
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/Event.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

struct BatchPayload { int value = 0; };

using BatchStore = EventStreamStore<BatchPayload>;

BatchStore* MakeStore(unsigned int capacity = 64)
{
    return new BatchStore(StringCRC("events"), StringCRC("BatchPayload"),
                          capacity, /*maxReaders=*/4);
}

Event<BatchPayload> MakeEvent(int v)
{
    Event<BatchPayload> ev;
    ev.payload.value = v;
    return ev;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Before first Flush: Consume() returns nothing (events are not visible
// until the producer flushes its frame boundary).
// ---------------------------------------------------------------------------
TEST(EventStreamBatching, EventsNotVisibleBeforeFlush)
{
    auto* store = MakeStore();
    int readerIndex = store->RegisterReader();
    ASSERT_GE(readerIndex, 0);

    store->Send(MakeEvent(1));
    store->Send(MakeEvent(2));

    DynamicArrayC<Event<BatchPayload>, 16> out;
    store->ConsumeUpToFlush(readerIndex, out);

    EXPECT_EQ(out.Size(), 0u)
        << "Events sent before Flush() must not be visible to ConsumeUpToFlush()";

    delete store;
}

// ---------------------------------------------------------------------------
// After Flush: Consume() returns exactly the events sent before the flush.
// ---------------------------------------------------------------------------
TEST(EventStreamBatching, EventsVisibleAfterFlush)
{
    auto* store = MakeStore();
    int readerIndex = store->RegisterReader();
    ASSERT_GE(readerIndex, 0);

    store->Send(MakeEvent(10));
    store->Send(MakeEvent(20));
    store->Flush();

    DynamicArrayC<Event<BatchPayload>, 16> out;
    store->ConsumeUpToFlush(readerIndex, out);

    ASSERT_EQ(out.Size(), 2u) << "Both events from flushed batch should be visible";
    EXPECT_EQ(out[0].payload.value, 10);
    EXPECT_EQ(out[1].payload.value, 20);

    delete store;
}

// ---------------------------------------------------------------------------
// Events sent after a flush are not visible until the NEXT flush.
// ---------------------------------------------------------------------------
TEST(EventStreamBatching, EventsAfterFlushNotVisibleUntilNextFlush)
{
    auto* store = MakeStore();
    int readerIndex = store->RegisterReader();
    ASSERT_GE(readerIndex, 0);

    // Frame 1: send + flush
    store->Send(MakeEvent(1));
    store->Flush();

    // Frame 2: send but don't flush
    store->Send(MakeEvent(2));

    DynamicArrayC<Event<BatchPayload>, 16> out;
    store->ConsumeUpToFlush(readerIndex, out);

    // Only frame 1's event is visible
    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0].payload.value, 1);

    delete store;
}

// ---------------------------------------------------------------------------
// Multiple flushes: each flush makes its batch available independently.
// The consumer can drain multiple batches in sequence.
// ---------------------------------------------------------------------------
TEST(EventStreamBatching, MultipleFlushesDrainInOrder)
{
    auto* store = MakeStore();
    int readerIndex = store->RegisterReader();
    ASSERT_GE(readerIndex, 0);

    // Frame 1
    store->Send(MakeEvent(100));
    store->Flush();
    // Frame 2
    store->Send(MakeEvent(200));
    store->Send(MakeEvent(201));
    store->Flush();

    DynamicArrayC<Event<BatchPayload>, 16> out;
    store->ConsumeUpToFlush(readerIndex, out);

    // Both batches are now available — 3 events total
    ASSERT_EQ(out.Size(), 3u);
    EXPECT_EQ(out[0].payload.value, 100);
    EXPECT_EQ(out[1].payload.value, 200);
    EXPECT_EQ(out[2].payload.value, 201);

    delete store;
}

// ---------------------------------------------------------------------------
// Zero events in a flush: an empty Flush() is a no-op that still advances
// the sequence, but the consumer sees zero new events.
// ---------------------------------------------------------------------------
TEST(EventStreamBatching, EmptyFlushProducesNoEvents)
{
    auto* store = MakeStore();
    int readerIndex = store->RegisterReader();
    ASSERT_GE(readerIndex, 0);

    store->Flush();  // empty flush

    DynamicArrayC<Event<BatchPayload>, 16> out;
    store->ConsumeUpToFlush(readerIndex, out);
    EXPECT_EQ(out.Size(), 0u);

    // Second flush with one event
    store->Send(MakeEvent(5));
    store->Flush();
    out.RemoveAll();
    store->ConsumeUpToFlush(readerIndex, out);
    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0].payload.value, 5);

    delete store;
}

// ---------------------------------------------------------------------------
// GetFlushSequence() increments with each Flush() call.
// ---------------------------------------------------------------------------
TEST(EventStreamBatching, FlushSequenceIncrements)
{
    auto* store = MakeStore();

    EXPECT_EQ(store->GetFlushSequence(), uint64_t(0));
    store->Flush();
    EXPECT_EQ(store->GetFlushSequence(), uint64_t(1));
    store->Flush();
    EXPECT_EQ(store->GetFlushSequence(), uint64_t(2));

    delete store;
}

// ---------------------------------------------------------------------------
// Cross-tick isolation: events from tick N are not visible to the consumer
// until tick N's Flush() is called, even if the consumer runs concurrently.
// ---------------------------------------------------------------------------
TEST(EventStreamBatching, CrossTickIsolation)
{
    auto* store = MakeStore();
    int readerIndex = store->RegisterReader();
    ASSERT_GE(readerIndex, 0);

    // Tick 1: producer sends, consumer drains before flush — sees nothing
    store->Send(MakeEvent(1));
    {
        DynamicArrayC<Event<BatchPayload>, 16> prematureOut;
        store->ConsumeUpToFlush(readerIndex, prematureOut);
        EXPECT_EQ(prematureOut.Size(), 0u) << "Consumer must not see tick 1 events before Flush";
    }

    // Tick 1 ends: producer flushes
    store->Flush();

    // Tick 2: consumer now drains — sees exactly tick 1's events
    {
        DynamicArrayC<Event<BatchPayload>, 16> tick1Out;
        store->ConsumeUpToFlush(readerIndex, tick1Out);
        ASSERT_EQ(tick1Out.Size(), 1u);
        EXPECT_EQ(tick1Out[0].payload.value, 1);
    }

    delete store;
}
