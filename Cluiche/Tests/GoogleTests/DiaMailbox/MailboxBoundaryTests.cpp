#include <DiaMailbox/Testing/MailboxTestFixture.h>
#include <DiaMailbox/Testing/MockRouter.h>
#include <gtest/gtest.h>

// sCurrentFixture is defined in TypedQueueTests.cpp — do NOT redefine here.

using namespace Dia::Mailbox;

// ---------------------------------------------------------------------------
// 33 distinct types for registry-full test (kMaxTypes = 32)
// ---------------------------------------------------------------------------
struct BT0{};  struct BT1{};  struct BT2{};  struct BT3{};
struct BT4{};  struct BT5{};  struct BT6{};  struct BT7{};
struct BT8{};  struct BT9{};  struct BT10{}; struct BT11{};
struct BT12{}; struct BT13{}; struct BT14{}; struct BT15{};
struct BT16{}; struct BT17{}; struct BT18{}; struct BT19{};
struct BT20{}; struct BT21{}; struct BT22{}; struct BT23{};
struct BT24{}; struct BT25{}; struct BT26{}; struct BT27{};
struct BT28{}; struct BT29{}; struct BT30{}; struct BT31{};
struct BT32{}; // 33rd — must fail

// ---------------------------------------------------------------------------
// B1 — Registry full: 33rd RegisterType returns false (kMaxTypes = 32)
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, RegisterType_RegistryFull_ReturnsFalse) {
    ASSERT_TRUE((mailbox.RegisterType<BT0,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT1,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT2,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT3,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT4,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT5,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT6,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT7,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT8,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT9,  1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT10, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT11, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT12, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT13, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT14, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT15, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT16, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT17, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT18, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT19, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT20, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT21, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT22, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT23, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT24, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT25, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT26, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT27, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT28, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT29, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT30, 1>()));
    ASSERT_TRUE((mailbox.RegisterType<BT31, 1>()));  // 32nd — still fits
    EXPECT_FALSE((mailbox.RegisterType<BT32, 1>())); // 33rd — registry full
}

// ---------------------------------------------------------------------------
// B2 — RegisterRouter with null pointer returns false silently
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, RegisterRouter_NullPointer_ReturnsFalse) {
    EXPECT_FALSE(mailbox.RegisterRouter(nullptr));
    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ---------------------------------------------------------------------------
// B3 — Router table full: 17th RegisterRouter returns false + warning
//      kMaxRouters = 16
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, RegisterRouter_TableFull_ReturnsFalse_EmitsWarning) {
    static constexpr int kCount = 17;
    char nameBuf[32];
    Dia::Mailbox::Testing::MockRouter* routers[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        sprintf_s(nameBuf, sizeof(nameBuf), "tbl_router_%02d", i);
        routers[i] = new Dia::Mailbox::Testing::MockRouter(Dia::Core::StringCRC(nameBuf));
    }

    for (int i = 0; i < 16; ++i) {
        ASSERT_TRUE(mailbox.RegisterRouter(routers[i])) << "slot " << i;
    }

    capturedWarnings.clear();
    EXPECT_FALSE(mailbox.RegisterRouter(routers[16]));
    EXPECT_GE(capturedWarnings.size(), 1u);

    for (int i = 0; i < kCount; ++i) { delete routers[i]; }
}

// ---------------------------------------------------------------------------
// B4 — Ring wrap-around: partial drain advances head; subsequent sends
//      wrap correctly and are delivered in FIFO order
// ---------------------------------------------------------------------------
struct WrapMsg { int v = 0; };

TEST_F(MailboxFixture, RingBuffer_WrapAround_CorrectFIFO) {
    // cap=3: send A,B → drain (head moves to 2) → send C,D,E (wraps) → drain gives C,D,E
    ASSERT_TRUE((mailbox.RegisterType<WrapMsg, 3>()));
    Address a; a.routerId = Dia::Core::StringCRC("wrap");

    WrapMsg m0; m0.v = 10;
    WrapMsg m1; m1.v = 20;
    mailbox.Send(a, m0);
    mailbox.Send(a, m1);
    mailbox.Drain<WrapMsg>([](const Address&, const WrapMsg&) {}); // head → 2, count = 0

    for (int i = 0; i < 3; ++i) {
        WrapMsg m; m.v = 100 + i;
        mailbox.Send(a, m); // slots 2, 0, 1 (wraps around)
    }

    std::vector<int> vals;
    mailbox.Drain<WrapMsg>([&](const Address&, const WrapMsg& m) { vals.push_back(m.v); });

    ASSERT_EQ(vals.size(), 3u);
    EXPECT_EQ(vals[0], 100);
    EXPECT_EQ(vals[1], 101);
    EXPECT_EQ(vals[2], 102);
    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ---------------------------------------------------------------------------
// B5 — Send exactly at capacity does not overflow and emits no warning
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, Send_ExactlyAtCapacity_NoOverflow_NoWarning) {
    ASSERT_TRUE((mailbox.RegisterType<WrapMsg, 4>()));
    Address a; a.routerId = Dia::Core::StringCRC("fence");

    for (int i = 0; i < 4; ++i) {
        WrapMsg m; m.v = i;
        EXPECT_TRUE(mailbox.Send(a, m));
    }

    int count = 0;
    mailbox.Drain<WrapMsg>([&](const Address&, const WrapMsg&) { ++count; });
    EXPECT_EQ(count, 4);
    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ---------------------------------------------------------------------------
// B6 — Resolve on a type with zero live subscribers: returns true, outMatched empty
// ---------------------------------------------------------------------------
struct ResolveEmptySubMsg { int v = 0; };

TEST_F(MailboxFixture, Resolve_RegisteredTypeNoSubscribers_ReturnsTrue_EmptyResult) {
    ASSERT_TRUE((mailbox.RegisterType<ResolveEmptySubMsg, 4>()));

    Dia::Core::StringCRC routerId("empty_sub_router");
    Dia::Mailbox::Testing::MockRouter router(routerId);
    ASSERT_TRUE(mailbox.RegisterRouter(&router));

    Address addr;
    addr.routerId = routerId;
    addr.payload  = 1;

    SubscriberSet out;
    bool result = mailbox.Resolve<ResolveEmptySubMsg>(addr, out);

    EXPECT_TRUE(result);
    EXPECT_EQ(out.Size(), 0u);
    EXPECT_EQ(router.ResolveCallCount(), 1);
    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ---------------------------------------------------------------------------
// B7 — Two Mailbox instances share no state
// ---------------------------------------------------------------------------
struct IndepMsg { int v = 0; };

TEST_F(MailboxFixture, TwoMailboxes_AreIndependent) {
    Dia::Mailbox::Mailbox other;
    ASSERT_TRUE((mailbox.RegisterType<IndepMsg, 4>()));
    ASSERT_TRUE((other.RegisterType<IndepMsg, 4>()));

    Address a; a.routerId = Dia::Core::StringCRC("r");
    IndepMsg m; m.v = 42;
    mailbox.Send(a, m);

    int mainCount = 0, otherCount = 0;
    mailbox.Drain<IndepMsg>([&](const Address&, const IndepMsg&) { ++mainCount; });
    other.Drain<IndepMsg>([&](const Address&, const IndepMsg&)   { ++otherCount; });

    EXPECT_EQ(mainCount,  1);
    EXPECT_EQ(otherCount, 0);
}

// ---------------------------------------------------------------------------
// B8 — HandlePool slots are reused after Unsubscribe
// ---------------------------------------------------------------------------
struct ReuseMsg { int v = 0; };

TEST_F(MailboxFixture, SubscribeUnsubscribe_PoolSlotsReused) {
    ASSERT_TRUE((mailbox.RegisterType<ReuseMsg, 1>()));

    SubscriptionHandle handles[10];
    for (uint32_t i = 0; i < 10; ++i) {
        handles[i] = mailbox.Subscribe<ReuseMsg>(SubscriberId{i + 1});
        ASSERT_TRUE(handles[i].IsValid()) << "initial subscribe " << i;
    }
    for (uint32_t i = 0; i < 10; ++i) {
        mailbox.Unsubscribe(handles[i]);
    }

    // Re-subscribe 10 more — pool must reuse freed slots
    capturedWarnings.clear();
    for (uint32_t i = 0; i < 10; ++i) {
        SubscriptionHandle h = mailbox.Subscribe<ReuseMsg>(SubscriberId{i + 100});
        EXPECT_TRUE(h.IsValid()) << "re-subscribe slot " << i;
    }
    EXPECT_EQ(capturedWarnings.size(), 0u);
}
