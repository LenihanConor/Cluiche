////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreamPolicy.cpp
// GoogleTest suite — EventStreamStore overflow policy tests
//
// Covers:
//   kFailLoud — asserts in debug; returns kFailLoudRejected in release
//   kBlock — NotifyShutdown unblocks blocked Send
//   SendResult values for each policy path
//   kDropOldest and kDropNewest coverage via direct store (supplement)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Streams/Event.h>
#include <DiaApplicationFlow/Streams/SendResult.h>
#include <DiaApplicationFlow/Streams/OverflowPolicy.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <thread>
#include <chrono>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static Event<int> MakePolicyEvent(int payload)
{
    Event<int> ev;
    ev.payload = payload;
    return ev;
}

// ---------------------------------------------------------------------------
// kDropOldest
// ---------------------------------------------------------------------------

TEST(StreamPolicy, DropOldestSendResultKDeliveredWhenNotFull)
{
    EventStreamStore<int> store(StringCRC("po_dropoldest_ok"), Dia::Core::StringCRC::kZero,
        /*capacity=*/4u);
    store.RegisterReader();
    EXPECT_EQ(store.Send(MakePolicyEvent(1)), SendResult::kDelivered);
    EXPECT_EQ(store.Send(MakePolicyEvent(2)), SendResult::kDelivered);
}

TEST(StreamPolicy, DropOldestReturnSkDroppedOldestWhenFull)
{
    EventStreamStore<int> store(StringCRC("po_dropoldest_full"), Dia::Core::StringCRC::kZero,
        /*capacity=*/2u);
    store.RegisterReader();
    store.Send(MakePolicyEvent(1));
    store.Send(MakePolicyEvent(2));
    SendResult r = store.Send(MakePolicyEvent(3));
    EXPECT_EQ(r, SendResult::kDroppedOldest)
        << "Third send into capacity-2 store should return kDroppedOldest";
}

TEST(StreamPolicy, DropOldestPreservesMostRecent)
{
    EventStreamStore<int> store(StringCRC("po_dropoldest_recent"), Dia::Core::StringCRC::kZero,
        /*capacity=*/2u);
    int rIdx = store.RegisterReader();
    store.Send(MakePolicyEvent(10));  // will be dropped
    store.Send(MakePolicyEvent(20));
    store.Send(MakePolicyEvent(30));

    DynamicArrayC<Event<int>, 16> out;
    store.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 2u);
    EXPECT_EQ(out[0].payload, 20);
    EXPECT_EQ(out[1].payload, 30);
}

// ---------------------------------------------------------------------------
// kDropNewest
// ---------------------------------------------------------------------------

TEST(StreamPolicy, DropNewestReturnSkDroppedNewestWhenFull)
{
    EventStreamStore<int> store(StringCRC("po_dropnewest"), Dia::Core::StringCRC::kZero,
        /*capacity=*/2u,
        /*maxReaders=*/EventStreamStore<int>::kDefaultMaxReaders,
        OverflowPolicy::kDropNewest);
    store.RegisterReader();
    store.Send(MakePolicyEvent(1));
    store.Send(MakePolicyEvent(2));
    SendResult r = store.Send(MakePolicyEvent(3));
    EXPECT_EQ(r, SendResult::kDroppedNewest)
        << "Third send into capacity-2 drop-newest store should return kDroppedNewest";
}

TEST(StreamPolicy, DropNewestPreservesFirst)
{
    EventStreamStore<int> store(StringCRC("po_dropnewest_first"), Dia::Core::StringCRC::kZero,
        /*capacity=*/2u,
        /*maxReaders=*/EventStreamStore<int>::kDefaultMaxReaders,
        OverflowPolicy::kDropNewest);
    int rIdx = store.RegisterReader();
    store.Send(MakePolicyEvent(10));
    store.Send(MakePolicyEvent(20));
    store.Send(MakePolicyEvent(30));  // dropped

    DynamicArrayC<Event<int>, 16> out;
    store.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 2u);
    EXPECT_EQ(out[0].payload, 10);
    EXPECT_EQ(out[1].payload, 20);
}

// ---------------------------------------------------------------------------
// kBlock — NotifyShutdown unblocks a blocked Send
//
// We can't block the test thread indefinitely, so we use a short blockTimeout
// (1 ms) and verify that Send unblocks and returns kBlockedThenDropped when
// every reader buffer is full and the timeout fires.
// ---------------------------------------------------------------------------

TEST(StreamPolicy, BlockTimeoutReturnSkBlockedThenDropped)
{
    EventStreamStore<int> store(StringCRC("po_block_timeout"), Dia::Core::StringCRC::kZero,
        /*capacity=*/1u,
        /*maxReaders=*/EventStreamStore<int>::kDefaultMaxReaders,
        OverflowPolicy::kBlock,
        /*blockTimeoutMs=*/1u);
    store.RegisterReader();

    // Fill the single slot.
    store.Send(MakePolicyEvent(1));

    // This send should block, timeout, then drop-oldest.
    SendResult r = store.Send(MakePolicyEvent(2));
    EXPECT_EQ(r, SendResult::kBlockedThenDropped)
        << "Send on a full kBlock store should return kBlockedThenDropped after timeout";
}

TEST(StreamPolicy, BlockNotifyShutdownUnblocksThread)
{
    EventStreamStore<int> store(StringCRC("po_block_shutdown"), Dia::Core::StringCRC::kZero,
        /*capacity=*/1u,
        /*maxReaders=*/EventStreamStore<int>::kDefaultMaxReaders,
        OverflowPolicy::kBlock,
        /*blockTimeoutMs=*/5000u);  // long timeout — NotifyShutdown must cut it short
    store.RegisterReader();

    // Fill the single slot.
    store.Send(MakePolicyEvent(1));

    // Launch a thread that blocks on Send.
    SendResult threadResult = SendResult::kDelivered;  // sentinel
    std::thread sender([&]() {
        threadResult = store.Send(MakePolicyEvent(2));
    });

    // Give the sender thread a moment to enter the condvar wait.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Notify shutdown — should unblock the waiting sender.
    store.NotifyShutdown();
    sender.join();

    // After shutdown notification, Send on a full buffer returns kDroppedOldest
    // (the shutdown path discards silently via kDroppedOldest).
    EXPECT_EQ(threadResult, SendResult::kDroppedOldest)
        << "NotifyShutdown should unblock a kBlock sender and return kDroppedOldest";
}

// ---------------------------------------------------------------------------
// kFailLoud — in Release builds: returns kFailLoudRejected when full.
// In Debug builds the store DIA_ASSERTs — we skip the overflow test there
// to avoid crashing the test runner.  We test the not-full path in both.
// ---------------------------------------------------------------------------

TEST(StreamPolicy, FailLoudDeliveredWhenNotFull)
{
    EventStreamStore<int> store(StringCRC("po_failloud_ok"), Dia::Core::StringCRC::kZero,
        /*capacity=*/4u,
        /*maxReaders=*/EventStreamStore<int>::kDefaultMaxReaders,
        OverflowPolicy::kFailLoud);
    store.RegisterReader();

    EXPECT_EQ(store.Send(MakePolicyEvent(1)), SendResult::kDelivered)
        << "kFailLoud store should return kDelivered when buffer has space";
    EXPECT_EQ(store.Send(MakePolicyEvent(2)), SendResult::kDelivered);
}

#if defined(NDEBUG)
// Only safe to test in Release — Debug build DIA_ASSERTs on overflow.
TEST(StreamPolicy, FailLoudReturnSkFailLoudRejectedWhenFull)
{
    EventStreamStore<int> store(StringCRC("po_failloud_full"), Dia::Core::StringCRC::kZero,
        /*capacity=*/2u,
        /*maxReaders=*/EventStreamStore<int>::kDefaultMaxReaders,
        OverflowPolicy::kFailLoud);
    store.RegisterReader();
    store.Send(MakePolicyEvent(1));
    store.Send(MakePolicyEvent(2));

    SendResult r = store.Send(MakePolicyEvent(3));
    EXPECT_EQ(r, SendResult::kFailLoudRejected)
        << "kFailLoud store should return kFailLoudRejected on overflow in Release";
}
#endif
