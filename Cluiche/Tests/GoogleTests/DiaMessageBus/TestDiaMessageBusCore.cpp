// TestDiaMessageBusCore.cpp
//
// Covers the DiaMessageBus core-bus task: Bus wrapping Mailbox,
// MessageBusModule wrapping Bus, BroadcastRouter, and the single
// Primary-pass-only flush + last-tick ledger.
//
// Explicitly NOT covered here (separate, later task): Reaction-pass
// dispatch, re-entrancy guarding for a second sweep. Pass::Reaction is
// exercised only to prove it is currently a no-op sink.
//
// Per AC-13: assertions go through Bus::GetLastTickLedger() / handler
// call counts, never by reaching into Bus internals.

#include <gtest/gtest.h>
#include <vector>

#include <DiaMessageBus/Bus.h>
#include <DiaMessageBus/MessageBusModule.h>
#include <DiaMessageBus/BroadcastRouter.h>
#include <DiaMessageBus/IFlushAdapter.h>

namespace Dia::MessageBus::Testing {

    // -----------------------------------------------------------------------
    // Local test-only message types. Real generated message structs are a
    // separate, later task (diagamemessages-format / codegen).
    // -----------------------------------------------------------------------
    struct TestMsgA {
        static inline const Dia::Core::StringCRC kTypeId{ "TestMsgA" };
        int value = 0;
    };

    struct TestMsgB {
        static inline const Dia::Core::StringCRC kTypeId{ "TestMsgB" };
        float value = 0.0f;
    };

    // -----------------------------------------------------------------------
    // Testable module subclass — exposes protected lifecycle hooks, mirroring
    // Cluiche/Tests/GoogleTests/DiaEntitySpawner/TestEntitySpawner.cpp's
    // TestableEntitySpawnerModule pattern. OnConnectStreams() is not exercised
    // — MessageBusModule doesn't declare any stream writers.
    // -----------------------------------------------------------------------
    class TestableMessageBusModule : public Dia::MessageBus::MessageBusModule {
    public:
        void Start()          { DoStart(); }
        void Update(float dt) { DoUpdate(dt); }
        void Stop()           { DoStop(); }
    };

    // -----------------------------------------------------------------------
    // Flush adapters for AC-8 (pre-Primary ordering / timing).
    // -----------------------------------------------------------------------
    class RecordingFlushAdapter : public Dia::MessageBus::IFlushAdapter {
    public:
        RecordingFlushAdapter(int id, std::vector<int>& order) : mId(id), mOrder(order) {}
        void Flush(Dia::MessageBus::Bus& /*bus*/) override { mOrder.push_back(mId); }
    private:
        int mId;
        std::vector<int>& mOrder;
    };

    class PostingFlushAdapter : public Dia::MessageBus::IFlushAdapter {
    public:
        void Flush(Dia::MessageBus::Bus& bus) override { bus.Broadcast(TestMsgA{}); }
    };

    // =========================================================================
    // AC-1: RegisterType forwards to Mailbox::RegisterType; false on duplicate.
    // =========================================================================
    TEST(DiaMessageBusCore, RegisterType_SucceedsOnce_FalseOnDuplicate) {
        Dia::MessageBus::Bus bus;
        EXPECT_TRUE((bus.RegisterType<TestMsgA, 8>()));
        EXPECT_FALSE((bus.RegisterType<TestMsgA, 8>()));
    }

    // =========================================================================
    // AC-2: RegisterProducer is metadata-only — zero routing effect. A type
    // with a producer but zero subscribers: Post succeeds, ledger shows the
    // message counted but zero deliveries.
    // =========================================================================
    TEST(DiaMessageBusCore, RegisterProducer_NoRoutingEffect_ZeroSubscribers) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        bus.RegisterProducer<TestMsgA>(Dia::Core::StringCRC("some-producer"));

        TestMsgA msg;
        msg.value = 42;
        EXPECT_TRUE(bus.Broadcast(msg));

        bus.Update();

        const Dia::MessageBus::LedgerSnapshot& ledger = bus.GetLastTickLedger();
        bool found = false;
        for (uint32_t i = 0; i < ledger.entries.Size(); ++i) {
            if (ledger.entries[i].typeId == TestMsgA::kTypeId) {
                found = true;
                EXPECT_EQ(ledger.entries[i].count, 1u);
                EXPECT_EQ(ledger.entries[i].deliveries, 0u);
            }
        }
        EXPECT_TRUE(found);
    }

    // =========================================================================
    // AC-3: Subscribe installs a handler and returns a BusSubscriptionHandle;
    // destroying the handle unsubscribes (no further delivery).
    // =========================================================================
    TEST(DiaMessageBusCore, Subscribe_HandleDestruction_StopsDelivery) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int callCount = 0;
        {
            Dia::MessageBus::BusSubscriptionHandle handle = bus.Subscribe<TestMsgA>(
                Dia::Core::StringCRC("subA"),
                [&callCount](const TestMsgA&) { ++callCount; });
            EXPECT_TRUE(handle.IsValid());

            bus.Broadcast(TestMsgA{});
            bus.Update();
            EXPECT_EQ(callCount, 1);
        } // handle destructs here -> Bus-side slot removed + Mailbox unsubscribed

        bus.Broadcast(TestMsgA{});
        bus.Update();
        EXPECT_EQ(callCount, 1); // no further delivery after unsubscribe
    }

    TEST(DiaMessageBusCore, Subscribe_UnregisteredType_ReturnsInvalidHandle) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        Dia::MessageBus::BusSubscriptionHandle handle =
            bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s"), [](const TestMsgA&) {});
        EXPECT_FALSE(handle.IsValid());
    }

    // =========================================================================
    // AC-4: Post enqueues into the Mailbox ring; false if T unregistered.
    // =========================================================================
    TEST(DiaMessageBusCore, Post_UnregisteredType_ReturnsFalse) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        TestMsgA msg;
        EXPECT_FALSE(bus.Post<TestMsgA>(
            Dia::Mailbox::Address{ Dia::MessageBus::Bus::kBroadcastRouterId, 0 }, msg));
    }

    TEST(DiaMessageBusCore, Post_RegisteredType_Succeeds) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));
        TestMsgA msg;
        EXPECT_TRUE(bus.Post<TestMsgA>(
            Dia::Mailbox::Address{ Dia::MessageBus::Bus::kBroadcastRouterId, 0 }, msg));
    }

    // =========================================================================
    // AC-5: Broadcast<T>(msg) == Post<T>(Address{kBroadcastRouterId, 0}, msg)
    // Verified behaviourally: a Broadcast reaches subscribers exactly like an
    // explicit Post to the broadcast address.
    // =========================================================================
    TEST(DiaMessageBusCore, Broadcast_DeliversLikeExplicitBroadcastAddressedPost) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int viaBroadcast = 0;
        int viaExplicitPost = 0;
        auto h1 = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s1"),
            [&viaBroadcast](const TestMsgA&) { ++viaBroadcast; });

        bus.Broadcast(TestMsgA{});
        bus.Update();
        EXPECT_EQ(viaBroadcast, 1);

        auto h2 = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s2"),
            [&viaExplicitPost](const TestMsgA&) { ++viaExplicitPost; });
        bus.Post<TestMsgA>(Dia::Mailbox::Address{ Dia::MessageBus::Bus::kBroadcastRouterId, 0 }, TestMsgA{});
        bus.Update();
        EXPECT_EQ(viaExplicitPost, 1);
    }

    // =========================================================================
    // AC-6: BroadcastRouter::GetRouterId() == StringCRC{"broadcast"};
    // Resolve() performs full fan-out of every live subscriber.
    // =========================================================================
    TEST(DiaMessageBusCore, BroadcastRouter_RouterId) {
        Dia::MessageBus::BroadcastRouter router;
        EXPECT_TRUE(router.GetRouterId() == Dia::Core::StringCRC("broadcast"));
    }

    TEST(DiaMessageBusCore, BroadcastRouter_ResolveIsFullFanout) {
        Dia::MessageBus::BroadcastRouter router;

        Dia::Mailbox::SubscriberSet live;
        live.Add(Dia::Mailbox::SubscriberId{ 1 });
        live.Add(Dia::Mailbox::SubscriberId{ 2 });
        live.Add(Dia::Mailbox::SubscriberId{ 3 });

        Dia::Mailbox::SubscriberSet matched;
        Dia::Mailbox::Address addr{ Dia::Core::StringCRC("broadcast"), 0 };
        router.Resolve(addr, live, matched);

        ASSERT_EQ(matched.Size(), 3u);
        EXPECT_TRUE(matched[0] == live[0]);
        EXPECT_TRUE(matched[1] == live[1]);
        EXPECT_TRUE(matched[2] == live[2]);
    }

    // =========================================================================
    // AC-7: MessageBusModule wraps Bus; its start hook registers the
    // BroadcastRouter on the Bus.
    // =========================================================================
    TEST(DiaMessageBusCore, Bus_Initialize_RegistersBroadcastRouter) {
        Dia::MessageBus::Bus bus;
        EXPECT_FALSE(bus.IsRouterRegistered(Dia::MessageBus::Bus::kBroadcastRouterId));
        bus.Initialize();
        EXPECT_TRUE(bus.IsRouterRegistered(Dia::MessageBus::Bus::kBroadcastRouterId));
    }

    TEST(DiaMessageBusCore, MessageBusModule_DoStart_RegistersBroadcastRouterOnItsBus) {
        TestableMessageBusModule module;
        module.Start();
        EXPECT_TRUE(module.GetBus().IsRouterRegistered(Dia::MessageBus::Bus::kBroadcastRouterId));
    }

    TEST(DiaMessageBusCore, MessageBusModule_DoUpdate_DelegatesFlushToBus) {
        TestableMessageBusModule module;
        module.Start();
        ASSERT_TRUE((module.GetBus().RegisterType<TestMsgA, 8>()));

        int count = 0;
        auto handle = module.GetBus().Subscribe<TestMsgA>(
            Dia::Core::StringCRC("s"), [&count](const TestMsgA&) { ++count; });

        module.GetBus().Broadcast(TestMsgA{});
        module.Update(0.016f);

        EXPECT_EQ(count, 1);
    }

    // =========================================================================
    // AC-8: Pre-Primary step calls Flush(bus) on every registered
    // IFlushAdapter, in registration order, before Primary dispatch.
    // =========================================================================
    TEST(DiaMessageBusCore, FlushAdapters_RunInRegistrationOrder) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();

        std::vector<int> order;
        RecordingFlushAdapter a1(1, order);
        RecordingFlushAdapter a2(2, order);
        RecordingFlushAdapter a3(3, order);
        bus.RegisterFlushAdapter(&a1);
        bus.RegisterFlushAdapter(&a2);
        bus.RegisterFlushAdapter(&a3);

        bus.Update();

        ASSERT_EQ(order.size(), 3u);
        EXPECT_EQ(order[0], 1);
        EXPECT_EQ(order[1], 2);
        EXPECT_EQ(order[2], 3);
    }

    TEST(DiaMessageBusCore, FlushAdapter_PostedMessage_DeliveredSameTick_BeforePrimary) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int count = 0;
        auto handle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("s"), [&count](const TestMsgA&) { ++count; });

        PostingFlushAdapter adapter;
        bus.RegisterFlushAdapter(&adapter);

        bus.Update(); // adapter posts during pre-Primary; Primary pass must see it

        EXPECT_EQ(count, 1);
    }

    // =========================================================================
    // AC-9: Primary pass drains every registered type, resolves subscribers
    // via the addressed router, invokes each matched Primary-pass handler.
    // Reaction-pass subscribers are never invoked (out of scope this task).
    // =========================================================================
    TEST(DiaMessageBusCore, PrimaryPass_DeliversToAllMatchedSubscribers) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int c1 = 0, c2 = 0;
        auto h1 = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s1"), [&c1](const TestMsgA&) { ++c1; });
        auto h2 = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s2"), [&c2](const TestMsgA&) { ++c2; });

        bus.Broadcast(TestMsgA{});
        bus.Update();

        EXPECT_EQ(c1, 1);
        EXPECT_EQ(c2, 1);
    }

    TEST(DiaMessageBusCore, PrimaryPass_DrainsMultipleRegisteredTypesIndependently) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgB, 8>()));

        int countA = 0, countB = 0;
        auto hA = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("sa"), [&countA](const TestMsgA&) { ++countA; });
        auto hB = bus.Subscribe<TestMsgB>(Dia::Core::StringCRC("sb"), [&countB](const TestMsgB&) { ++countB; });

        bus.Broadcast(TestMsgA{});
        bus.Broadcast(TestMsgB{});
        bus.Update();

        EXPECT_EQ(countA, 1);
        EXPECT_EQ(countB, 1);
    }

    TEST(DiaMessageBusCore, ReactionPassSubscriber_NeverInvoked_ThisTask) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int count = 0;
        auto handle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("s"), [&count](const TestMsgA&) { ++count; }, Dia::MessageBus::Pass::Reaction);

        bus.Broadcast(TestMsgA{});
        bus.Update();

        EXPECT_EQ(count, 0);
    }

    // =========================================================================
    // AC-11: GetLastTickLedger() returns the just-completed tick's snapshot;
    // double-buffered.
    // =========================================================================
    TEST(DiaMessageBusCore, Ledger_DoubleBuffered_ReturnsPreviousCompleteTick) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));
        auto handle = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s"), [](const TestMsgA&) {});

        // Before any Update(), the exposed ledger is the empty default.
        EXPECT_EQ(bus.GetLastTickLedger().entries.Size(), 0u);

        bus.Broadcast(TestMsgA{});
        bus.Update();
        const Dia::MessageBus::LedgerSnapshot& afterTick0 = bus.GetLastTickLedger();
        EXPECT_EQ(afterTick0.tickIndex, 0u);
        EXPECT_EQ(afterTick0.entries.Size(), 1u);

        // Tick 1: nothing posted -> the newly-completed ledger has no entries,
        // proving the previous (tick 0) buffer was not mutated in place.
        bus.Update();
        const Dia::MessageBus::LedgerSnapshot& afterTick1 = bus.GetLastTickLedger();
        EXPECT_EQ(afterTick1.tickIndex, 1u);
        EXPECT_EQ(afterTick1.entries.Size(), 0u);
    }

    // =========================================================================
    // AC-12: Ledger tally correct: N posts of type T delivered to M
    // subscribers yields count == N, deliveries == N*M; droppedCount reflects
    // overflow drops this tick.
    // =========================================================================
    TEST(DiaMessageBusCore, Ledger_TallyCorrect_CountAndDeliveries) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int c1 = 0, c2 = 0, c3 = 0;
        auto h1 = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s1"), [&c1](const TestMsgA&) { ++c1; });
        auto h2 = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s2"), [&c2](const TestMsgA&) { ++c2; });
        auto h3 = bus.Subscribe<TestMsgA>(Dia::Core::StringCRC("s3"), [&c3](const TestMsgA&) { ++c3; });

        const uint32_t N = 4;
        for (uint32_t i = 0; i < N; ++i) {
            bus.Broadcast(TestMsgA{});
        }
        bus.Update();

        const Dia::MessageBus::LedgerSnapshot& ledger = bus.GetLastTickLedger();
        ASSERT_EQ(ledger.entries.Size(), 1u);
        EXPECT_EQ(ledger.entries[0].count, N);
        EXPECT_EQ(ledger.entries[0].deliveries, N * 3u);
        EXPECT_EQ(static_cast<uint32_t>(c1), N);
        EXPECT_EQ(static_cast<uint32_t>(c2), N);
        EXPECT_EQ(static_cast<uint32_t>(c3), N);
    }

    TEST(DiaMessageBusCore, Ledger_DroppedCount_ReflectsOverflowThisTick) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        // Capacity 2, DropOldest: posting 5 messages this tick drops 3.
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 2>(Dia::Mailbox::OverflowPolicy::DropOldest)));

        for (int i = 0; i < 5; ++i) {
            bus.Broadcast(TestMsgA{});
        }
        bus.Update();

        const Dia::MessageBus::LedgerSnapshot& ledger = bus.GetLastTickLedger();
        EXPECT_EQ(ledger.droppedCount, 3u);
    }

} // namespace Dia::MessageBus::Testing
