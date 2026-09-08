// TestDiaMessageBusCore.cpp
//
// Covers the DiaMessageBus core-bus task: Bus wrapping Mailbox,
// MessageBusModule wrapping Bus, BroadcastRouter, and the two-pass flush
// (Primary sweep then Reaction sweep) + last-tick ledger.
//
// AC-10: the Reaction pass, and the Post/Broadcast re-entrancy guard that
// blocks enqueuing while the Reaction pass is executing.
//
// Per AC-13: assertions go through Bus::GetLastTickLedger() / handler
// call counts, never by reaching into Bus internals.

#include <gtest/gtest.h>
#include <vector>

#include <DiaCore/Core/Assert.h>
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
        void Start() { DoStart(); }
        void Update(float dt)
        {
            Dia::SimTime::SimTimeContext ctx{
                Dia::Core::TimeAbsolute::Zero(),
                Dia::Core::TimeRelative::CreateFromSeconds(dt),
                0, 1.0f, false
            };
            DoUpdate(ctx);
        }
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

    // A message already queued BEFORE Update() is consumed by that type's
    // Primary drain (Mailbox::Drain is a snapshot-and-consume — see
    // DiaMailbox), even though DispatchOne filters it out for a Reaction-only
    // subscriber. There is nothing left for the Reaction sweep to drain, so
    // the Reaction subscriber never fires for a message posted this way.
    // (Contrast with ReactionPass_DeliversMessagePostedDuringPrimaryHandler
    // below, where the message is posted DURING the Primary sweep — i.e.
    // after that type's Primary drain already ran/will run this tick — and
    // is therefore still in the queue when the Reaction sweep drains it.)
    TEST(DiaMessageBusCore, ReactionPassSubscriber_NotInvoked_ForMessageQueuedBeforeUpdate) {
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
    // AC-10: Reaction pass drains messages posted DURING Primary handlers to
    // Pass::Reaction subscribers, after Primary completes (same tick).
    // =========================================================================
    struct TestMsgReaction {
        static inline const Dia::Core::StringCRC kTypeId{ "TestMsgReaction" };
        int value = 0;
    };

    // Dedicated trigger type used only to get a Primary handler to
    // (re-)post TestMsgA DURING the Primary sweep, so the freshly-posted
    // TestMsgA is still queued when the Reaction sweep drains TestMsgA
    // (unlike a TestMsgA queued before Update() at all, which Primary's own
    // drain of TestMsgA would already have consumed).
    struct TestMsgReactionTrigger {
        static inline const Dia::Core::StringCRC kTypeId{ "TestMsgReactionTrigger" };
    };

    TEST(DiaMessageBusCore, ReactionPass_DeliversMessagePostedDuringPrimaryHandler_SameTick) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        // Registration order matters here: Update()'s Primary loop is one
        // forward pass over every registered type. TestMsgReaction must be
        // registered (and therefore Primary-drained) BEFORE TestMsgA so that
        // its Primary drain has already happened by the time TestMsgA's
        // Primary handler posts a fresh TestMsgReaction — otherwise that
        // post would land ahead of TestMsgReaction's Primary drain still
        // within the same Primary loop and be consumed there instead of
        // surviving to the Reaction sweep.
        ASSERT_TRUE((bus.RegisterType<TestMsgReaction, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int reactionCount = 0;
        auto reactionHandle = bus.Subscribe<TestMsgReaction>(
            Dia::Core::StringCRC("reaction-sub"),
            [&reactionCount](const TestMsgReaction&) { ++reactionCount; },
            Dia::MessageBus::Pass::Reaction);

        // Primary handler for TestMsgA posts a TestMsgReaction message.
        auto primaryHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("primary-sub"),
            [&bus](const TestMsgA&) { bus.Broadcast(TestMsgReaction{}); },
            Dia::MessageBus::Pass::Primary);

        bus.Broadcast(TestMsgA{});
        bus.Update();

        EXPECT_EQ(reactionCount, 1) << "Message posted by a Primary handler must be visible to the same tick's Reaction sweep";

        // Not delivered again on a later tick with nothing new posted.
        bus.Update();
        EXPECT_EQ(reactionCount, 1);
    }

    // A Primary subscriber and a Reaction subscriber on the SAME type: only
    // one of them fires per drained message, gated by which sweep is
    // currently running (a message is drained — and dispatched — exactly
    // once, under exactly one currentPass value).
    TEST(DiaMessageBusCore, PrimaryAndReactionSubscribers_SameType_NeverBothFireForSameMessage) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int primaryCount = 0, reactionCount = 0;
        auto primaryHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("primary-sub"),
            [&primaryCount](const TestMsgA&) { ++primaryCount; },
            Dia::MessageBus::Pass::Primary);
        auto reactionHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("reaction-sub"),
            [&reactionCount](const TestMsgA&) { ++reactionCount; },
            Dia::MessageBus::Pass::Reaction);

        bus.Broadcast(TestMsgA{});
        bus.Update();

        // The message existed before Update(): Primary's drain consumes it
        // and dispatches under currentPass=Primary, so only the Primary
        // subscriber fires. The Reaction sweep's drain finds nothing left.
        EXPECT_EQ(primaryCount, 1);
        EXPECT_EQ(reactionCount, 0);
    }

    // A Primary-pass handler's Post/Broadcast call must succeed normally —
    // regression check that the new re-entrancy guard only blocks Post while
    // the Reaction sweep is executing, not during Primary.
    TEST(DiaMessageBusCore, PrimaryPassHandler_PostSucceeds_NotBlockedByReactionGuard) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgReaction, 8>()));

        bool postReturnedTrue = false;
        auto primaryHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("primary-sub"),
            [&bus, &postReturnedTrue](const TestMsgA&) {
                postReturnedTrue = bus.Broadcast(TestMsgReaction{});
            },
            Dia::MessageBus::Pass::Primary);

        bus.Broadcast(TestMsgA{});
        bus.Update();

        EXPECT_TRUE(postReturnedTrue);
    }

    // Recorder installed as g_pAssertFunc for the two tests below, matching
    // the existing Core/Threading/TestJobSystem.cpp swap-and-restore pattern.
    // Defined unconditionally (not just under #ifdef DEBUG): DIA_ASSERT
    // compiles out entirely in Release, so installing this recorder there is
    // harmless (it is simply never invoked) and keeps this test file
    // building the same way in both configurations.
    namespace {
        int gBusReactionAssertCount = 0;
        void BusReactionAssertRecorder(const char*, const char*, int, const char*, ...) {
            ++gBusReactionAssertCount;
        }
    } // namespace

    // A Reaction-pass handler that itself calls Post/Broadcast must be
    // blocked: Post returns false and no delivery happens. In a Debug build
    // the real (default) assert handler would otherwise fire here too —
    // its default behavior is to break into a debugger, which crashes an
    // unattended test run — so this test installs the same no-op recorder
    // used by ReactionPassHandler_PostBlocked_FiresDIA_ASSERT_DebugOnly
    // below purely to keep the process alive; the actual assert-fires
    // assertion lives in that dedicated test.
    TEST(DiaMessageBusCore, ReactionPassHandler_PostBlocked_ReturnsFalseAndNoDelivery) {
        auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
        Dia::Core::g_pAssertFunc = BusReactionAssertRecorder;

        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgReaction, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgReactionTrigger, 8>()));

        int downstreamCount = 0;
        auto downstreamHandle = bus.Subscribe<TestMsgReaction>(
            Dia::Core::StringCRC("downstream-sub"),
            [&downstreamCount](const TestMsgReaction&) { ++downstreamCount; },
            Dia::MessageBus::Pass::Reaction);

        bool postReturnedFalse = true; // stays true only if Post is never called
        bool postWasCalled = false;
        auto reactionHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("reaction-sub"),
            [&bus, &postReturnedFalse, &postWasCalled](const TestMsgA&) {
                postWasCalled = true;
                postReturnedFalse = !bus.Broadcast(TestMsgReaction{});
            },
            Dia::MessageBus::Pass::Reaction);

        // A Primary handler posts TestMsgA DURING the Primary sweep, so it is
        // still queued when the Reaction sweep drains TestMsgA — that's what
        // drives the Reaction-pass TestMsgA handler above (see the
        // TestMsgReactionTrigger comment above its declaration).
        auto triggerHandle = bus.Subscribe<TestMsgReactionTrigger>(
            Dia::Core::StringCRC("trigger-sub"),
            [&bus](const TestMsgReactionTrigger&) { bus.Broadcast(TestMsgA{}); },
            Dia::MessageBus::Pass::Primary);
        bus.Broadcast(TestMsgReactionTrigger{});

        bus.Update();

        EXPECT_TRUE(postWasCalled) << "Reaction-pass TestMsgA handler must have run this tick";
        EXPECT_TRUE(postReturnedFalse) << "Post/Broadcast called from a Reaction-pass handler must return false";
        EXPECT_EQ(downstreamCount, 0) << "No delivery must occur for a Post blocked by the Reaction re-entrancy guard";

        Dia::Core::g_pAssertFunc = prevAssertFunc;
    }

#ifdef DEBUG
    TEST(DiaMessageBusCore, ReactionPassHandler_PostBlocked_FiresDIA_ASSERT_DebugOnly) {
        auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
        Dia::Core::g_pAssertFunc = BusReactionAssertRecorder;
        gBusReactionAssertCount = 0;

        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgReaction, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgReactionTrigger, 8>()));

        auto triggerHandle = bus.Subscribe<TestMsgReactionTrigger>(
            Dia::Core::StringCRC("trigger-sub"),
            [&bus](const TestMsgReactionTrigger&) { bus.Broadcast(TestMsgA{}); },
            Dia::MessageBus::Pass::Primary);
        auto reactionHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("reaction-sub"),
            [&bus](const TestMsgA&) { bus.Broadcast(TestMsgReaction{}); },
            Dia::MessageBus::Pass::Reaction);

        bus.Broadcast(TestMsgReactionTrigger{});
        bus.Update();

        EXPECT_GT(gBusReactionAssertCount, 0) << "Expected DIA_ASSERT to fire when Post is called during the Reaction pass";

        Dia::Core::g_pAssertFunc = prevAssertFunc;
    }
#endif

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

    // =========================================================================
    // Bug fix: Reaction-pass delivery must not depend on RegisterType
    // registration order between the posting type and the target type.
    //
    // Pre-fix, Update()'s Primary loop is a single forward sweep calling
    // drainFn(Pass::Primary) per type with NO shared snapshot boundary across
    // types. If A is registered before B and A's Primary handler posts B
    // DURING the sweep, B's slot (later in the same forward loop) sees the
    // freshly-posted message already sitting in its queue when the loop
    // reaches it, and Mailbox::Drain<B>() (which snapshots desc->count at
    // call time, with no upper bound) drains and dispatches it as
    // Pass::Primary right there — silently destroying it (B has no Primary
    // subscriber) before the Reaction sweep ever runs. The fix snapshots
    // every type's queued count ONCE, before any Primary handler in this
    // tick has run, and bounds each type's Primary drain to that snapshot —
    // so this must hold regardless of A/B registration order.
    // =========================================================================
    TEST(DiaMessageBusCore, ReactionPass_DeliversMessage_RegardlessOfRegistrationOrder_PosterBeforeTarget) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        // A registered before B: A's slot index < B's slot index in the
        // Primary loop — this is the ordering that exposes the bug.
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgB, 8>()));

        int reactionCount = 0;
        auto reactionHandle = bus.Subscribe<TestMsgB>(
            Dia::Core::StringCRC("reaction-sub"),
            [&reactionCount](const TestMsgB&) { ++reactionCount; },
            Dia::MessageBus::Pass::Reaction);

        auto primaryHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("primary-sub"),
            [&bus](const TestMsgA&) { bus.Broadcast(TestMsgB{}); },
            Dia::MessageBus::Pass::Primary);

        bus.Broadcast(TestMsgA{});
        bus.Update();

        EXPECT_EQ(reactionCount, 1)
            << "Reaction subscriber for B must receive a message posted by A's Primary "
               "handler, regardless of A/B registration order";
    }

    // Same scenario, opposite registration order (B before A). This ordering
    // does NOT expose the bug even pre-fix (B's Primary drain already ran by
    // the time A's handler posts to it), so this must pass both before and
    // after the fix — a regression guard for the other ordering direction.
    TEST(DiaMessageBusCore, ReactionPass_DeliversMessage_RegardlessOfRegistrationOrder_TargetBeforePoster) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgB, 8>()));
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int reactionCount = 0;
        auto reactionHandle = bus.Subscribe<TestMsgB>(
            Dia::Core::StringCRC("reaction-sub"),
            [&reactionCount](const TestMsgB&) { ++reactionCount; },
            Dia::MessageBus::Pass::Reaction);

        auto primaryHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("primary-sub"),
            [&bus](const TestMsgA&) { bus.Broadcast(TestMsgB{}); },
            Dia::MessageBus::Pass::Primary);

        bus.Broadcast(TestMsgA{});
        bus.Update();

        EXPECT_EQ(reactionCount, 1);
    }

    // Same-type mid-sweep re-entry: a Primary handler for A posts a NEW A
    // message during A's own Primary drain. The snapshot bound must also
    // exclude this from the same Primary drain call so it survives, untouched,
    // for the Reaction sweep — not just cross-type mid-sweep posts.
    TEST(DiaMessageBusCore, ReactionPass_DeliversMessage_PostedBySameTypePrimaryHandler_MidSweep) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int reactionCount = 0;
        auto reactionHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("reaction-sub"),
            [&reactionCount](const TestMsgA&) { ++reactionCount; },
            Dia::MessageBus::Pass::Reaction);

        bool reposted = false;
        auto primaryHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("primary-sub"),
            [&bus, &reposted](const TestMsgA& msg) {
                if (!reposted) {
                    reposted = true;
                    TestMsgA next;
                    next.value = msg.value + 1;
                    bus.Broadcast(next);
                }
            },
            Dia::MessageBus::Pass::Primary);

        TestMsgA seed;
        seed.value = 1;
        bus.Broadcast(seed);
        bus.Update();

        EXPECT_EQ(reactionCount, 1)
            << "A Primary handler for A that posts a new A message mid-sweep must not "
               "have that message drained again within the same Primary sweep";
    }

    // Regression guard: the snapshot bound must not under-deliver messages
    // that were genuinely queued BEFORE Update() was called at all (the
    // normal case) — only messages posted DURING the Primary sweep should be
    // deferred.
    TEST(DiaMessageBusCore, PrimaryPass_StillDeliversAllPreQueuedMessages_AfterSnapshotBoundFix) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<TestMsgA, 8>()));

        int primaryCount = 0;
        auto primaryHandle = bus.Subscribe<TestMsgA>(
            Dia::Core::StringCRC("primary-sub"),
            [&primaryCount](const TestMsgA&) { ++primaryCount; },
            Dia::MessageBus::Pass::Primary);

        const int N = 5;
        for (int i = 0; i < N; ++i) {
            bus.Broadcast(TestMsgA{});
        }
        bus.Update();

        EXPECT_EQ(primaryCount, N)
            << "Snapshot bound must not under-deliver messages queued before Update() was called";
    }

} // namespace Dia::MessageBus::Testing
