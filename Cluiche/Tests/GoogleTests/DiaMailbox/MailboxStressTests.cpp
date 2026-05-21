#include <DiaMailbox/Testing/MailboxTestFixture.h>
#include <gtest/gtest.h>

// sCurrentFixture is defined in TypedQueueTests.cpp — do NOT redefine here.

using namespace Dia::Mailbox;

// ---------------------------------------------------------------------------
// S1 — Large overflow: cap=256, send 512 → exactly one drop warning, 256 delivered
// ---------------------------------------------------------------------------
struct StressMsg { int v = 0; };

TEST_F(MailboxFixture, Stress_LargeOverflow_SingleWarning) {
    ASSERT_TRUE((mailbox.RegisterType<StressMsg, 256>()));
    Address a; a.routerId = Dia::Core::StringCRC("stress");

    for (int i = 0; i < 512; ++i) {
        StressMsg m; m.v = i;
        mailbox.Send(a, m);
    }

    // No warning yet — drops fire on Drain, not on Send
    EXPECT_EQ(capturedWarnings.size(), 0u);

    int count = 0;
    mailbox.Drain<StressMsg>([&](const Address&, const StressMsg&) { ++count; });

    EXPECT_EQ(count, 256);
    EXPECT_EQ(capturedWarnings.size(), 1u);
    EXPECT_TRUE(HasWarningContaining("dropped"));
}

// ---------------------------------------------------------------------------
// S2 — Drop accumulates across consecutive overflowing sends;
//      then drains twice: warning only on first non-empty drain
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, Stress_DropAccumulates_WarningOnFirstDrainOnly) {
    ASSERT_TRUE((mailbox.RegisterType<StressMsg, 4>()));
    Address a; a.routerId = Dia::Core::StringCRC("acc");

    // Overflow by 8 sends into cap=4 (4 drops)
    for (int i = 0; i < 8; ++i) {
        StressMsg m; m.v = i;
        mailbox.Send(a, m);
    }
    EXPECT_EQ(capturedWarnings.size(), 0u);

    // First drain: delivers 4 msgs, emits 1 drop warning
    int count = 0;
    mailbox.Drain<StressMsg>([&](const Address&, const StressMsg&) { ++count; });
    EXPECT_EQ(count, 4);
    EXPECT_EQ(capturedWarnings.size(), 1u);

    // Second drain on now-empty queue: no additional warning
    capturedWarnings.clear();
    count = 0;
    mailbox.Drain<StressMsg>([&](const Address&, const StressMsg&) { ++count; });
    EXPECT_EQ(count, 0);
    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ---------------------------------------------------------------------------
// S3 — 1000 fill/drain cycles: ring stays stable, values correct
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, Stress_1000DrainCycles_RingStable) {
    ASSERT_TRUE((mailbox.RegisterType<StressMsg, 4>()));
    Address a; a.routerId = Dia::Core::StringCRC("cycle");

    for (int cycle = 0; cycle < 1000; ++cycle) {
        for (int i = 0; i < 4; ++i) {
            StressMsg m; m.v = cycle * 4 + i;
            ASSERT_TRUE(mailbox.Send(a, m)) << "cycle " << cycle << " send " << i;
        }

        int expected = cycle * 4;
        mailbox.Drain<StressMsg>([&](const Address&, const StressMsg& m) {
            EXPECT_EQ(m.v, expected) << "cycle " << cycle;
            ++expected;
        });
    }

    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ---------------------------------------------------------------------------
// S4 — 4 interleaved types: each type's queue is independent under concurrent sends
// ---------------------------------------------------------------------------
struct IST1 { int  a = 0; };
struct IST2 { int  b = 0; };
struct IST3 { int  c = 0; };
struct IST4 { int  d = 0; };

TEST_F(MailboxFixture, Stress_MultiType_Interleaved_AllDelivered) {
    ASSERT_TRUE((mailbox.RegisterType<IST1, 32>()));
    ASSERT_TRUE((mailbox.RegisterType<IST2, 32>()));
    ASSERT_TRUE((mailbox.RegisterType<IST3, 32>()));
    ASSERT_TRUE((mailbox.RegisterType<IST4, 32>()));

    Address a; a.routerId = Dia::Core::StringCRC("interleaved");

    for (int i = 0; i < 32; ++i) {
        IST1 m1; m1.a = i; mailbox.Send(a, m1);
        IST2 m2; m2.b = i; mailbox.Send(a, m2);
        IST3 m3; m3.c = i; mailbox.Send(a, m3);
        IST4 m4; m4.d = i; mailbox.Send(a, m4);
    }

    int c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    mailbox.Drain<IST1>([&](const Address&, const IST1& m) { EXPECT_EQ(m.a, c1); ++c1; });
    mailbox.Drain<IST2>([&](const Address&, const IST2& m) { EXPECT_EQ(m.b, c2); ++c2; });
    mailbox.Drain<IST3>([&](const Address&, const IST3& m) { EXPECT_EQ(m.c, c3); ++c3; });
    mailbox.Drain<IST4>([&](const Address&, const IST4& m) { EXPECT_EQ(m.d, c4); ++c4; });

    EXPECT_EQ(c1, 32);
    EXPECT_EQ(c2, 32);
    EXPECT_EQ(c3, 32);
    EXPECT_EQ(c4, 32);
    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ---------------------------------------------------------------------------
// S5 — Fill per-type subscriber list to 64, unsubscribe all, verify list empty
// ---------------------------------------------------------------------------
struct SubFillMsg { int v = 0; };

TEST_F(MailboxFixture, Stress_Fill64Subscribers_UnsubscribeAll_ListEmpty) {
    ASSERT_TRUE((mailbox.RegisterType<SubFillMsg, 1>()));

    SubscriptionHandle handles[64];
    for (uint32_t i = 0; i < 64; ++i) {
        handles[i] = mailbox.Subscribe<SubFillMsg>(SubscriberId{i + 1});
        ASSERT_TRUE(handles[i].IsValid()) << "slot " << i;
    }

    EXPECT_EQ(mailbox.GetSubscribersForType<SubFillMsg>().Size(), 64u);

    for (uint32_t i = 0; i < 64; ++i) {
        mailbox.Unsubscribe(handles[i]);
    }

    EXPECT_EQ(mailbox.GetSubscribersForType<SubFillMsg>().Size(), 0u);
    EXPECT_EQ(capturedWarnings.size(), 0u);
}
