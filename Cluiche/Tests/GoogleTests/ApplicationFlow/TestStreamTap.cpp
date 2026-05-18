////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreamTap.cpp
// GoogleTest suite — EventStreamStore Tap API (F4)
//
// Covers:
//   - AttachTap returns non-zero TapHandle
//   - GetTapCount increments/decrements correctly
//   - DetachTap removes callback
//   - Tap callback is invoked on Send with correct payload and streamId
//   - Multiple taps all dispatched
//   - TapHandle 0 is invalid — DetachTap(0) is a no-op
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Streams/IStreamStore.h>
#include <DiaApplicationFlow/Streams/Event.h>
#include <DiaApplicationFlow/Streams/SendResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <atomic>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static Event<int> MakeTapEvent(int payload)
{
    Event<int> ev;
    ev.payload = payload;
    return ev;
}

// ---------------------------------------------------------------------------
// AttachTap / GetTapCount
// ---------------------------------------------------------------------------

TEST(StreamTap, AttachTapReturnsNonZeroHandle)
{
    EventStreamStore<int> store(StringCRC("tap_attach"));
    int tapCallCount = 0;
    TapHandle h = store.AttachTap([&](const void*, unsigned int, const StringCRC&) {
        ++tapCallCount;
    });
    EXPECT_NE(h.id, 0u) << "AttachTap should return a non-zero TapHandle";
}

TEST(StreamTap, GetTapCountIncrements)
{
    EventStreamStore<int> store(StringCRC("tap_count_inc"));

    EXPECT_EQ(store.GetTapCount(), 0u);

    TapHandle h1 = store.AttachTap([](const void*, unsigned int, const StringCRC&) {});
    EXPECT_EQ(store.GetTapCount(), 1u);

    TapHandle h2 = store.AttachTap([](const void*, unsigned int, const StringCRC&) {});
    EXPECT_EQ(store.GetTapCount(), 2u);

    (void)h1; (void)h2;
}

TEST(StreamTap, DetachTapDecrementsCount)
{
    EventStreamStore<int> store(StringCRC("tap_count_dec"));

    TapHandle h1 = store.AttachTap([](const void*, unsigned int, const StringCRC&) {});
    TapHandle h2 = store.AttachTap([](const void*, unsigned int, const StringCRC&) {});
    EXPECT_EQ(store.GetTapCount(), 2u);

    store.DetachTap(h1);
    EXPECT_EQ(store.GetTapCount(), 1u);

    store.DetachTap(h2);
    EXPECT_EQ(store.GetTapCount(), 0u);
}

TEST(StreamTap, DetachTapZeroHandleIsNoOp)
{
    EventStreamStore<int> store(StringCRC("tap_detach_zero"));
    TapHandle h = store.AttachTap([](const void*, unsigned int, const StringCRC&) {});
    EXPECT_EQ(store.GetTapCount(), 1u);

    store.DetachTap(TapHandle{0});  // invalid handle — no-op
    EXPECT_EQ(store.GetTapCount(), 1u) << "DetachTap(0) should be a no-op";

    store.DetachTap(h);  // cleanup
}

// ---------------------------------------------------------------------------
// Tap callback invocation
// ---------------------------------------------------------------------------

TEST(StreamTap, TapCallbackInvokedOnSend)
{
    EventStreamStore<int> store(StringCRC("tap_invoke"));
    store.RegisterReader();

    int callCount = 0;
    store.AttachTap([&](const void*, unsigned int, const StringCRC&) {
        ++callCount;
    });

    store.Send(MakeTapEvent(1));
    EXPECT_EQ(callCount, 1) << "Tap callback should fire once per Send";

    store.Send(MakeTapEvent(2));
    EXPECT_EQ(callCount, 2) << "Tap callback should fire for each Send";
}

TEST(StreamTap, TapCallbackReceivesCorrectPayload)
{
    EventStreamStore<int> store(StringCRC("tap_payload"));
    store.RegisterReader();

    int receivedPayload = -1;
    store.AttachTap([&](const void* bytes, unsigned int size, const StringCRC&) {
        ASSERT_EQ(size, sizeof(int));
        receivedPayload = *static_cast<const int*>(bytes);
    });

    store.Send(MakeTapEvent(42));
    EXPECT_EQ(receivedPayload, 42)
        << "Tap callback should receive the exact payload bytes";
}

TEST(StreamTap, TapCallbackReceivesCorrectStreamId)
{
    const StringCRC expectedId("tap_streamid");
    EventStreamStore<int> store(expectedId);
    store.RegisterReader();

    StringCRC receivedId;
    store.AttachTap([&](const void*, unsigned int, const StringCRC& id) {
        receivedId = id;
    });

    store.Send(MakeTapEvent(1));
    EXPECT_EQ(receivedId, expectedId)
        << "Tap callback should receive the stream's own ID";
}

// Detached tap is NOT invoked on subsequent Sends.
TEST(StreamTap, DetachedTapNotInvoked)
{
    EventStreamStore<int> store(StringCRC("tap_detached"));
    store.RegisterReader();

    int callCount = 0;
    TapHandle h = store.AttachTap([&](const void*, unsigned int, const StringCRC&) {
        ++callCount;
    });

    store.Send(MakeTapEvent(1));
    EXPECT_EQ(callCount, 1);

    store.DetachTap(h);
    store.Send(MakeTapEvent(2));
    EXPECT_EQ(callCount, 1) << "Detached tap should not be invoked";
}

// Multiple taps — all dispatched for each Send.
TEST(StreamTap, MultipleTapsAllDispatched)
{
    EventStreamStore<int> store(StringCRC("tap_multi"));
    store.RegisterReader();

    int count1 = 0, count2 = 0, count3 = 0;
    store.AttachTap([&](const void*, unsigned int, const StringCRC&) { ++count1; });
    store.AttachTap([&](const void*, unsigned int, const StringCRC&) { ++count2; });
    store.AttachTap([&](const void*, unsigned int, const StringCRC&) { ++count3; });

    store.Send(MakeTapEvent(7));

    EXPECT_EQ(count1, 1) << "Tap 1 should have fired";
    EXPECT_EQ(count2, 1) << "Tap 2 should have fired";
    EXPECT_EQ(count3, 1) << "Tap 3 should have fired";
}

// Tap is invoked even when there are no readers.
TEST(StreamTap, TapFiresWithNoReaders)
{
    EventStreamStore<int> store(StringCRC("tap_no_readers"));
    // No RegisterReader call.

    int callCount = 0;
    store.AttachTap([&](const void*, unsigned int, const StringCRC&) {
        ++callCount;
    });

    store.Send(MakeTapEvent(99));
    EXPECT_EQ(callCount, 1) << "Tap should fire even when there are no readers";
}

// Send still delivers to readers when a tap is attached.
TEST(StreamTap, TapDoesNotPreventReaderDelivery)
{
    EventStreamStore<int> store(StringCRC("tap_reader_deliver"));
    int rIdx = store.RegisterReader();

    store.AttachTap([](const void*, unsigned int, const StringCRC&) {});

    store.Send(MakeTapEvent(55));

    DynamicArrayC<Event<int>, 8> out;
    store.Consume(rIdx, out);
    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0].payload, 55)
        << "Reader delivery should not be affected by an attached tap";
}

// GetTapCount via IStreamStore virtual dispatch.
TEST(StreamTap, GetTapCountViaIStreamStore)
{
    EventStreamStore<int> store(StringCRC("tap_virtual"));
    IStreamStore* istore = &store;

    EXPECT_EQ(istore->GetTapCount(), 0u);

    TapHandle h = istore->AttachTap([](const void*, unsigned int, const StringCRC&) {});
    EXPECT_EQ(istore->GetTapCount(), 1u);

    istore->DetachTap(h);
    EXPECT_EQ(istore->GetTapCount(), 0u);
}
