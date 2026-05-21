#include <DiaMailbox/Testing/MailboxTestFixture.h>
#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Test message types
// ---------------------------------------------------------------------------
struct SubMsg  { int value = 0; };
struct SubMsgA { int x = 0; };
struct SubMsgB { float y = 0.0f; };

// Pool-exhaustion test types (52+52+52+52+48 = 256 subscriptions; no per-type list is full)
struct PoolT1 {};
struct PoolT2 {};
struct PoolT3 {};
struct PoolT4 {};
struct PoolT5 {};

using namespace Dia::Mailbox;

// ===========================================================================
// AC1 — Subscribe returns a valid handle
// ===========================================================================
TEST_F(MailboxFixture, Subscribe_ReturnsValidHandle) {
    ASSERT_TRUE((mailbox.RegisterType<SubMsg, 4>()));
    SubscriptionHandle h = mailbox.Subscribe<SubMsg>(SubscriberId{1});
    EXPECT_TRUE(h.IsValid());
}

// ===========================================================================
// AC2 — Subscribed id appears in GetSubscribersForType
// ===========================================================================
TEST_F(MailboxFixture, Subscribe_IdAppearsInSubscriberList) {
    ASSERT_TRUE((mailbox.RegisterType<SubMsg, 4>()));
    mailbox.Subscribe<SubMsg>(SubscriberId{42});
    const SubscriberSet& subs = mailbox.GetSubscribersForType<SubMsg>();
    bool found = false;
    for (uint32_t i = 0; i < subs.Size(); ++i) {
        if (subs[i] == SubscriberId{42}) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ===========================================================================
// AC3 — Two different subscribers, both appear in list
// ===========================================================================
TEST_F(MailboxFixture, Subscribe_TwoSubscribers_BothInList) {
    ASSERT_TRUE((mailbox.RegisterType<SubMsg, 4>()));
    mailbox.Subscribe<SubMsg>(SubscriberId{10});
    mailbox.Subscribe<SubMsg>(SubscriberId{20});
    const SubscriberSet& subs = mailbox.GetSubscribersForType<SubMsg>();
    EXPECT_EQ(subs.Size(), 2u);
}

// ===========================================================================
// AC4 — Subscribing to MsgA does not populate MsgB list
// ===========================================================================
TEST_F(MailboxFixture, Subscribe_TypeIsolation) {
    ASSERT_TRUE((mailbox.RegisterType<SubMsgA, 4>()));
    ASSERT_TRUE((mailbox.RegisterType<SubMsgB, 4>()));
    mailbox.Subscribe<SubMsgA>(SubscriberId{1});
    const SubscriberSet& subsB = mailbox.GetSubscribersForType<SubMsgB>();
    EXPECT_EQ(subsB.Size(), 0u);
}

// ===========================================================================
// AC5 — After Unsubscribe, handle.IsValid() returns false
// ===========================================================================
TEST_F(MailboxFixture, Unsubscribe_HandleBecomesInvalid) {
    ASSERT_TRUE((mailbox.RegisterType<SubMsg, 4>()));
    SubscriptionHandle h = mailbox.Subscribe<SubMsg>(SubscriberId{1});
    ASSERT_TRUE(h.IsValid());
    mailbox.Unsubscribe(h);
    EXPECT_FALSE(h.IsValid());
}

// ===========================================================================
// AC6 — After Unsubscribe, id absent from GetSubscribersForType
// ===========================================================================
TEST_F(MailboxFixture, Unsubscribe_IdRemovedFromList) {
    ASSERT_TRUE((mailbox.RegisterType<SubMsg, 4>()));
    SubscriptionHandle h = mailbox.Subscribe<SubMsg>(SubscriberId{99});
    mailbox.Unsubscribe(h);
    const SubscriberSet& subs = mailbox.GetSubscribersForType<SubMsg>();
    bool found = false;
    for (uint32_t i = 0; i < subs.Size(); ++i) {
        if (subs[i] == SubscriberId{99}) { found = true; break; }
    }
    EXPECT_FALSE(found);
}

// ===========================================================================
// AC7 — Double-unsubscribe is a no-op; no crash
// ===========================================================================
TEST_F(MailboxFixture, Unsubscribe_DoubleCancelIsNoOp) {
    ASSERT_TRUE((mailbox.RegisterType<SubMsg, 4>()));
    SubscriptionHandle h = mailbox.Subscribe<SubMsg>(SubscriberId{5});
    mailbox.Unsubscribe(h);
    // Second call on the now-stale handle must not crash.
    EXPECT_NO_THROW(mailbox.Unsubscribe(h));
}

// ===========================================================================
// AC8 — Default-constructed handle is invalid and safe to unsubscribe
// ===========================================================================
TEST_F(MailboxFixture, DefaultHandle_IsInvalidAndSafeToUnsubscribe) {
    SubscriptionHandle h;
    EXPECT_FALSE(h.IsValid());
    EXPECT_NO_THROW(mailbox.Unsubscribe(h));
}

// ===========================================================================
// AC9 — Pool exhaustion (52+52+52+52+48 = 256 subs across 5 types); 257th
//        returns invalid + warning. No per-type list is saturated (all < 64),
//        so the pool-full guard is reached, not the per-type-full guard.
// ===========================================================================
TEST_F(MailboxFixture, Subscribe_PoolFull_ReturnsInvalidWithWarning) {
    ASSERT_TRUE((mailbox.RegisterType<PoolT1, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<PoolT2, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<PoolT3, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<PoolT4, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<PoolT5, 1>()));

    // 52 × 4 types + 48 × 1 type = 256 (fills the pool; no list reaches 64).
    for (uint32_t i = 0; i < 52; ++i) {
        SubscriptionHandle h = mailbox.Subscribe<PoolT1>(SubscriberId{uint64_t(0 * 100 + i + 1)});
        ASSERT_TRUE(h.IsValid()) << "T1 slot " << i;
    }
    for (uint32_t i = 0; i < 52; ++i) {
        SubscriptionHandle h = mailbox.Subscribe<PoolT2>(SubscriberId{uint64_t(1 * 100 + i + 1)});
        ASSERT_TRUE(h.IsValid()) << "T2 slot " << i;
    }
    for (uint32_t i = 0; i < 52; ++i) {
        SubscriptionHandle h = mailbox.Subscribe<PoolT3>(SubscriberId{uint64_t(2 * 100 + i + 1)});
        ASSERT_TRUE(h.IsValid()) << "T3 slot " << i;
    }
    for (uint32_t i = 0; i < 52; ++i) {
        SubscriptionHandle h = mailbox.Subscribe<PoolT4>(SubscriberId{uint64_t(3 * 100 + i + 1)});
        ASSERT_TRUE(h.IsValid()) << "T4 slot " << i;
    }
    for (uint32_t i = 0; i < 48; ++i) {
        SubscriptionHandle h = mailbox.Subscribe<PoolT5>(SubscriberId{uint64_t(4 * 100 + i + 1)});
        ASSERT_TRUE(h.IsValid()) << "T5 slot " << i;
    }

    // 257th subscription must fail with pool-full warning.
    // PoolT5 has 48 entries (list not full), so the per-type check passes
    // and the pool-full guard fires.
    capturedWarnings.clear();
    SubscriptionHandle overflow = mailbox.Subscribe<PoolT5>(SubscriberId{99999});
    EXPECT_FALSE(overflow.IsValid());
    EXPECT_GE(capturedWarnings.size(), 1u);
    EXPECT_TRUE(HasWarningContaining("pool full"));
}

// ===========================================================================
// AC10 — Subscribe to unregistered type returns invalid + warning
// ===========================================================================
TEST_F(MailboxFixture, Subscribe_UnregisteredType_ReturnsInvalidWithWarning) {
    // SubMsg is deliberately NOT registered here.
    SubscriptionHandle h = mailbox.Subscribe<SubMsg>(SubscriberId{1});
    EXPECT_FALSE(h.IsValid());
    EXPECT_GE(capturedWarnings.size(), 1u);
    EXPECT_TRUE(HasWarningContaining("not registered"));
}

// ===========================================================================
// AC11 — GetSubscribersForType for unregistered type returns empty, no crash
// ===========================================================================
TEST_F(MailboxFixture, GetSubscribersForType_UnregisteredType_ReturnsEmpty) {
    // SubMsg is deliberately NOT registered here.
    const SubscriberSet& subs = mailbox.GetSubscribersForType<SubMsg>();
    EXPECT_EQ(subs.Size(), 0u);
}

// ===========================================================================
// AC12 — Subscribe same id twice; unsubscribe one; id still in list once
// ===========================================================================
TEST_F(MailboxFixture, Subscribe_SameIdTwice_UnsubscribeOne_IdRemainsOnce) {
    ASSERT_TRUE((mailbox.RegisterType<SubMsg, 8>()));
    SubscriptionHandle h1 = mailbox.Subscribe<SubMsg>(SubscriberId{7});
    SubscriptionHandle h2 = mailbox.Subscribe<SubMsg>(SubscriberId{7});
    ASSERT_TRUE(h1.IsValid());
    ASSERT_TRUE(h2.IsValid());

    mailbox.Unsubscribe(h1);

    const SubscriberSet& subs = mailbox.GetSubscribersForType<SubMsg>();
    uint32_t count = 0;
    for (uint32_t i = 0; i < subs.Size(); ++i) {
        if (subs[i] == SubscriberId{7}) { ++count; }
    }
    EXPECT_EQ(count, 1u);
}

// ===========================================================================
// AC13 — Per-type list full (64 subs); 65th returns invalid + warning
// ===========================================================================
TEST_F(MailboxFixture, Subscribe_PerTypeListFull_ReturnsInvalidWithWarning) {
    // Register with capacity 1 (queue capacity; subscriber list capacity is always 64).
    ASSERT_TRUE((mailbox.RegisterType<SubMsg, 1>()));

    // Fill the per-type subscriber list (capacity 64).
    for (uint32_t i = 0; i < 64; ++i) {
        SubscriptionHandle h = mailbox.Subscribe<SubMsg>(SubscriberId{i + 1});
        ASSERT_TRUE(h.IsValid()) << "Slot " << i << " should be valid";
    }

    // 65th should fail with per-type-list-full warning.
    capturedWarnings.clear();
    SubscriptionHandle overflow = mailbox.Subscribe<SubMsg>(SubscriberId{65});
    EXPECT_FALSE(overflow.IsValid());
    EXPECT_GE(capturedWarnings.size(), 1u);
    EXPECT_TRUE(HasWarningContaining("subscriber list full"));
}
