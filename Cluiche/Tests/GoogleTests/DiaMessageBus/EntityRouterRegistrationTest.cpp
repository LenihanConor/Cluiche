// EntityRouterRegistrationTest.cpp
//
// Integration test for Bus::RegisterRouter + entity-addressed Post/Update
// delivery, proven end-to-end via a test-local MockEntityRouter standing in
// for the real EntityRouter (owned by diaentitytemplate, built separately,
// in a later task). Covers:
//   AC-1: Bus::RegisterRouter(router) delegates to mMailbox.RegisterRouter —
//         the mock's Resolve() runs during flush of an entity-addressed post.
//   AC-2: Bus::kEntityRouterId == StringCRC{"entity"},
//         Bus::kBroadcastRouterId == StringCRC{"broadcast"}.
//   AC-3: an entity-addressed Post + Update reaches only the subscriber
//         whose underlying Mailbox SubscriberId matches the address payload.
//   AC-4: Broadcast<T> fan-out to type subscribers is unaffected by
//         registering the mock entity router.
//   AC-5: GetLastTickLedger() records an entry with routerId ==
//         kEntityRouterId (and count >= 1) for the entity-addressed post.
//
// HitEvent and MockEntityRouter are test-local only, per this task's brief —
// they intentionally do not live in any shared header.

#include <gtest/gtest.h>

#include <DiaMessageBus/Bus.h>
#include <DiaMailbox/IMailboxRouter.h>

namespace Dia::MessageBus::Testing {

    // -----------------------------------------------------------------------
    // Test-local message type. Real generated message structs are a separate,
    // later task (diagamemessages-format / codegen) — see the sibling
    // TestDiaMessageBusCore.cpp, which follows the same local-message-type
    // pattern (TestMsgA/TestMsgB) with `static inline const StringCRC
    // kTypeId{...}` — StringCRC's constructor is not constexpr in this
    // codebase, so kTypeId cannot be `static constexpr`.
    // -----------------------------------------------------------------------
    struct HitEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "HitEvent" };
        uint32_t targetId = 0;
    };

    // -----------------------------------------------------------------------
    // Test-local mock entity router. Pure payload matching against live
    // subscriber ids — no allocation, no STL (PD-004). Stands in for the
    // real EntityRouter.
    // -----------------------------------------------------------------------
    class MockEntityRouter : public Dia::Mailbox::IMailboxRouter {
    public:
        uint32_t resolveCallCount = 0;

        Dia::Core::StringCRC GetRouterId() const override {
            return Dia::MessageBus::Bus::kEntityRouterId;
        }

        void Resolve(const Dia::Mailbox::Address& addr,
                     const Dia::Mailbox::SubscriberSet& liveSubscribers,
                     Dia::Mailbox::SubscriberSet& outMatched) override {
            ++resolveCallCount;
            for (uint32_t i = 0; i < liveSubscribers.Size(); ++i) {
                if (liveSubscribers[i].value == addr.payload) {
                    outMatched.Add(liveSubscribers[i]);
                }
            }
        }
    };

    // =========================================================================
    // AC-2: router-id constants.
    // =========================================================================
    TEST(EntityRouterRegistrationTest, RouterIdConstants_MatchExpectedStrings) {
        EXPECT_TRUE(Dia::MessageBus::Bus::kEntityRouterId == Dia::Core::StringCRC("entity"));
        EXPECT_TRUE(Dia::MessageBus::Bus::kBroadcastRouterId == Dia::Core::StringCRC("broadcast"));
    }

    // =========================================================================
    // AC-1: RegisterRouter delegates to the owned Mailbox — the mock's
    // Resolve() is invoked during flush of an entity-addressed post, not at
    // Post-time.
    // =========================================================================
    TEST(EntityRouterRegistrationTest, RegisterRouter_DelegatesToMailbox_ResolveInvokedOnFlush) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<HitEvent, 8>()));

        MockEntityRouter router;
        bus.RegisterRouter(&router);
        EXPECT_TRUE(bus.IsRouterRegistered(Dia::MessageBus::Bus::kEntityRouterId));

        HitEvent msg;
        msg.targetId = 1;
        EXPECT_TRUE(bus.Post<HitEvent>(
            Dia::Mailbox::Address{ Dia::MessageBus::Bus::kEntityRouterId, 0 }, msg));

        EXPECT_EQ(router.resolveCallCount, 0u) << "Resolve must not run before flush";
        bus.Update();
        EXPECT_GE(router.resolveCallCount, 1u) << "Resolve must run during flush of an entity-addressed post";
    }

    // =========================================================================
    // AC-3 / AC-5: entity-addressed delivery reaches only the matching
    // subscriber, and the ledger records the entity router for that entry.
    //
    // AC-3's illustrative SubscriberId values (42 / 99) can't be dictated
    // literally through Bus::Subscribe<T>: Bus derives the underlying
    // Mailbox SubscriberId from StringCRC(subscriberId).Value() (see
    // Bus::Subscribe<T> in Bus.h), not a raw settable integer. Instead we
    // compute up front the exact value Bus will assign to each named
    // subscriber — the same StringCRC::Value() call Bus performs internally
    // — and address the Post at the target subscriber's value. This proves
    // the identical behaviour the AC describes: the addressed subscriber
    // fires exactly once, the other subscriber never fires.
    // =========================================================================
    TEST(EntityRouterRegistrationTest, EntityAddressedPost_DeliversOnlyToMatchingSubscriber_AndLedgersEntityRouter) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<HitEvent, 8>()));

        MockEntityRouter router;
        bus.RegisterRouter(&router);

        const Dia::Core::StringCRC kTargetSubscriberId("entity-target-42");
        const Dia::Core::StringCRC kOtherSubscriberId("entity-other-99");
        const uint64_t targetValue = static_cast<uint64_t>(kTargetSubscriberId.Value());
        const uint64_t otherValue  = static_cast<uint64_t>(kOtherSubscriberId.Value());
        ASSERT_NE(targetValue, otherValue);

        int targetCallCount = 0;
        int otherCallCount  = 0;
        auto targetHandle = bus.Subscribe<HitEvent>(
            kTargetSubscriberId, [&targetCallCount](const HitEvent&) { ++targetCallCount; });
        auto otherHandle = bus.Subscribe<HitEvent>(
            kOtherSubscriberId, [&otherCallCount](const HitEvent&) { ++otherCallCount; });
        ASSERT_TRUE(targetHandle.IsValid());
        ASSERT_TRUE(otherHandle.IsValid());

        HitEvent msg;
        msg.targetId = 42;
        EXPECT_TRUE(bus.Post<HitEvent>(
            Dia::Mailbox::Address{ Dia::MessageBus::Bus::kEntityRouterId, targetValue }, msg));

        bus.Update();

        EXPECT_EQ(targetCallCount, 1) << "Addressed subscriber must fire exactly once";
        EXPECT_EQ(otherCallCount, 0) << "Unaddressed subscriber must never fire";

        // AC-5: ledger entry for HitEvent records the entity router.
        const Dia::MessageBus::LedgerSnapshot& ledger = bus.GetLastTickLedger();
        bool found = false;
        for (uint32_t i = 0; i < ledger.entries.Size(); ++i) {
            if (ledger.entries[i].typeId == HitEvent::kTypeId) {
                found = true;
                EXPECT_TRUE(ledger.entries[i].routerId == Dia::MessageBus::Bus::kEntityRouterId);
                EXPECT_GE(ledger.entries[i].count, 1u);
            }
        }
        EXPECT_TRUE(found) << "Ledger must contain an entry for HitEvent";
    }

    // =========================================================================
    // AC-4: Broadcast<T> still fans out to all type subscribers, unchanged by
    // registering the mock entity router.
    // =========================================================================
    TEST(EntityRouterRegistrationTest, Broadcast_StillFansOutToAllSubscribers_WithMockRouterRegistered) {
        Dia::MessageBus::Bus bus;
        bus.Initialize();
        ASSERT_TRUE((bus.RegisterType<HitEvent, 8>()));

        MockEntityRouter router;
        bus.RegisterRouter(&router);

        int c1 = 0, c2 = 0;
        auto h1 = bus.Subscribe<HitEvent>(Dia::Core::StringCRC("bsub1"), [&c1](const HitEvent&) { ++c1; });
        auto h2 = bus.Subscribe<HitEvent>(Dia::Core::StringCRC("bsub2"), [&c2](const HitEvent&) { ++c2; });

        HitEvent msg;
        msg.targetId = 7;
        EXPECT_TRUE(bus.Broadcast(msg));
        bus.Update();

        EXPECT_EQ(c1, 1);
        EXPECT_EQ(c2, 1);
        EXPECT_EQ(router.resolveCallCount, 0u)
            << "Broadcast must resolve via the BroadcastRouter, not the entity router";
    }

} // namespace Dia::MessageBus::Testing
