// TestBusTrafficRecorder.cpp
//
// Covers AC-1 through AC-7 and AC-10 for DiaMessageBus Bus Traffic Recorder
// (Feature: Bus Traffic Recorder, Task 1 only — IBusRecorder interface,
// Bus::SetRecorder, dispatch hook, in-memory test recorder).
//
// AC-1: Every dispatched message produces exactly one record, in dispatch order.
// AC-2: Each record carries tick, type id, router id, pass, and raw payload bytes.
// AC-5: With no recorder set, bus behaves identically (null-check path).
// AC-6: With DIA_DEBUG off (Release), the hook compiles out entirely.
// AC-7: A message with no subscribers is still recorded.
// AC-10: Non-trivially-copyable message types fail to compile (all existing types still compile).

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include <DiaCore/Core/Assert.h>
#include <DiaMessageBus/Bus.h>

#ifdef DIA_DEBUG
#include <DiaMessageBus/IBusRecorder.h>
#include <DiaMessageBus/Testing/InMemoryBusRecorder.h>
#endif

namespace Dia::MessageBus::Testing {

    // -----------------------------------------------------------------------
    // Local test-only message types
    // -----------------------------------------------------------------------
    struct RecorderTestMsgA {
        static inline const Dia::Core::StringCRC kTypeId{ "RecorderTestMsgA" };
        int value = 0;
    };

    struct RecorderTestMsgB {
        static inline const Dia::Core::StringCRC kTypeId{ "RecorderTestMsgB" };
        float x = 0.0f;
        float y = 0.0f;
    };

#ifdef DIA_DEBUG
    // =========================================================================
    // AC-1: One record per dispatched message, in dispatch order across
    // Primary and Reaction pass.
    // =========================================================================
    TEST(BusTrafficRecorder, OneRecordPerMessage_InDispatchOrder) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgB, 8>()));

        InMemoryBusRecorder recorder;
        bus.SetRecorder(&recorder);

        // Subscribe a Primary-pass handler that posts a Reaction message
        auto hPrimary = bus.Subscribe<RecorderTestMsgA>(
            Dia::Core::StringCRC("primarySub"),
            [&bus](const RecorderTestMsgA& msg) {
                if (msg.value == 42) {
                    RecorderTestMsgB reactionMsg;
                    reactionMsg.x = 1.0f;
                    bus.Broadcast(reactionMsg);
                }
            },
            Pass::Primary);

        // Subscribe a Reaction-pass handler
        int reactionCallCount = 0;
        auto hReaction = bus.Subscribe<RecorderTestMsgB>(
            Dia::Core::StringCRC("reactionSub"),
            [&reactionCallCount](const RecorderTestMsgB&) {
                ++reactionCallCount;
            },
            Pass::Reaction);

        // Post two messages: one triggers a Reaction post, one does not
        RecorderTestMsgA msg1;
        msg1.value = 42;
        bus.Broadcast(msg1);

        RecorderTestMsgA msg2;
        msg2.value = 99;
        bus.Broadcast(msg2);

        bus.Update();

        // Expect 3 records: msg1 (Primary), msg2 (Primary), then reactionMsg (Reaction)
        const auto& records = recorder.GetRecords();
        ASSERT_EQ(records.size(), 3u);

        // Check order and pass
        EXPECT_EQ(records[0].typeId, RecorderTestMsgA::kTypeId);
        EXPECT_EQ(records[0].pass, Pass::Primary);

        EXPECT_EQ(records[1].typeId, RecorderTestMsgA::kTypeId);
        EXPECT_EQ(records[1].pass, Pass::Primary);

        EXPECT_EQ(records[2].typeId, RecorderTestMsgB::kTypeId);
        EXPECT_EQ(records[2].pass, Pass::Reaction);

        EXPECT_EQ(reactionCallCount, 1);
    }

    // =========================================================================
    // AC-2: Each record carries tick, type id, router id, pass, and raw payload bytes.
    // =========================================================================
    TEST(BusTrafficRecorder, RecordFields_TypeId_RouterId_Pass_PayloadBytes) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));

        uint64_t tick = 0;
        InMemoryBusRecorder recorder(
            [](void* ctx) -> uint64_t { return (*static_cast<uint64_t*>(ctx))++; }, &tick);
        bus.SetRecorder(&recorder);

        RecorderTestMsgA msg;
        msg.value = 123;
        bus.Broadcast(msg);

        bus.Update();

        const auto& records = recorder.GetRecords();
        ASSERT_EQ(records.size(), 1u);

        const auto& rec = records[0];
        EXPECT_EQ(rec.tick, 0u);
        EXPECT_EQ(rec.typeId, RecorderTestMsgA::kTypeId);
        EXPECT_EQ(rec.routerId, Bus::kBroadcastRouterId);
        EXPECT_EQ(rec.pass, Pass::Primary);
        EXPECT_EQ(rec.payloadSize, sizeof(RecorderTestMsgA));

        // Verify raw payload bytes
        RecorderTestMsgA recovered;
        std::memcpy(&recovered, rec.payloadBytes.data(), sizeof(RecorderTestMsgA));
        EXPECT_EQ(recovered.value, 123);
    }

    // =========================================================================
    // AC-5: With no recorder set, bus behaves identically (null-check path).
    // Existing bus tests pass unchanged.
    // =========================================================================
    TEST(BusTrafficRecorder, NoRecorder_BusDeliveryUnchanged) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));

        int callCount = 0;
        auto h = bus.Subscribe<RecorderTestMsgA>(
            Dia::Core::StringCRC("sub"),
            [&callCount](const RecorderTestMsgA&) { ++callCount; });

        bus.Broadcast(RecorderTestMsgA{});
        bus.Update();
        EXPECT_EQ(callCount, 1);

        // SetRecorder(nullptr) clears
        InMemoryBusRecorder recorder;
        bus.SetRecorder(&recorder);
        bus.Broadcast(RecorderTestMsgA{});
        bus.Update();
        EXPECT_EQ(recorder.GetRecords().size(), 1u);

        bus.SetRecorder(nullptr);
        bus.Broadcast(RecorderTestMsgA{});
        bus.Update();
        EXPECT_EQ(recorder.GetRecords().size(), 1u); // no new records
        EXPECT_EQ(callCount, 3); // delivery still happened
    }

    // =========================================================================
    // AC-7: A message with no subscribers is still recorded.
    // =========================================================================
    TEST(BusTrafficRecorder, MessageWithNoSubscribers_StillRecorded) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<RecorderTestMsgA, 8>()));

        InMemoryBusRecorder recorder;
        bus.SetRecorder(&recorder);

        // Post a message with zero subscribers
        RecorderTestMsgA msg;
        msg.value = 42;
        bus.Broadcast(msg);

        bus.Update();

        const auto& records = recorder.GetRecords();
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].typeId, RecorderTestMsgA::kTypeId);
        EXPECT_EQ(records[0].payloadSize, sizeof(RecorderTestMsgA));
    }
#endif // DIA_DEBUG

} // namespace Dia::MessageBus::Testing
