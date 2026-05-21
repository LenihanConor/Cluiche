#include <DiaMailbox/Testing/MailboxTestFixture.h>
#include <gtest/gtest.h>

// Static member definition (one per binary)
Dia::Mailbox::MailboxFixture* Dia::Mailbox::MailboxFixture::sCurrentFixture = nullptr;

// ---------------------------------------------------------------------------
// Test message types
// ---------------------------------------------------------------------------
struct Msg {
    int value = 0;
};

struct LargeMsg {
    char     data[128];
    uint32_t sentinel1 = 0;
    uint32_t sentinel2 = 0;
};

// Convenience address helpers
static Dia::Mailbox::Address MakeAddr(const char* id, uint64_t payload = 0) {
    Dia::Mailbox::Address a;
    a.routerId = Dia::Core::StringCRC(id);
    a.payload  = payload;
    return a;
}

using namespace Dia::Mailbox;

// ===========================================================================
// AC1 — RegisterType returns true first time, false on duplicate
// ===========================================================================
TEST_F(MailboxFixture, AC1_RegisterType_DuplicateReturnsFalse) {
    EXPECT_TRUE((mailbox.RegisterType<Msg, 4>()));
    EXPECT_FALSE((mailbox.RegisterType<Msg, 4>()));
}

// ===========================================================================
// AC2 — Default policy is DropOldest; oldest message is absent after overflow
// ===========================================================================
TEST_F(MailboxFixture, AC2_DefaultPolicy_DropOldest) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 2>()));
    Address a = MakeAddr("r");

    Msg m0; m0.value = 10;
    Msg m1; m1.value = 20;
    Msg m2; m2.value = 30;

    mailbox.Send(a, m0);  // slot 0
    mailbox.Send(a, m1);  // slot 1 — queue full
    mailbox.Send(a, m2);  // overflow: m0 dropped, m2 enqueued

    int collected[3] = {};
    int count = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg& msg) {
        if (count < 3) { collected[count++] = msg.value; }
    });

    EXPECT_EQ(count, 2);
    EXPECT_EQ(collected[0], 20);   // m1 — oldest surviving
    EXPECT_EQ(collected[1], 30);   // m2
}

// ===========================================================================
// AC3 — Send for unregistered type returns false and emits a warning
// ===========================================================================
TEST_F(MailboxFixture, AC3_Send_UnregisteredTypeReturnsFalse) {
    Address a = MakeAddr("r");
    Msg m;

    bool result = mailbox.Send(a, m);
    EXPECT_FALSE(result);
    EXPECT_GE(capturedWarnings.size(), 1u);
}

// ===========================================================================
// AC4 — Register, send one, drain — visitor called once with correct data
// ===========================================================================
TEST_F(MailboxFixture, AC4_SendDrain_SingleMessage) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 4>()));
    Address sent = MakeAddr("router1", 42u);
    Msg m; m.value = 99;
    mailbox.Send(sent, m);

    int visitCount = 0;
    mailbox.Drain<Msg>([&](const Address& addr, const Msg& msg) {
        ++visitCount;
        EXPECT_EQ(addr.routerId, sent.routerId);
        EXPECT_EQ(addr.payload,  sent.payload);
        EXPECT_EQ(msg.value, 99);
    });

    EXPECT_EQ(visitCount, 1);
}

// ===========================================================================
// AC5 — Multiple sends drain in FIFO order
// ===========================================================================
TEST_F(MailboxFixture, AC5_Drain_FIFOOrder) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 8>()));
    Address a = MakeAddr("r");

    for (int i = 0; i < 3; ++i) {
        Msg m; m.value = i;
        mailbox.Send(a, m);
    }

    int order[3] = {};
    int idx = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg& msg) {
        if (idx < 3) { order[idx++] = msg.value; }
    });

    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);
}

// ===========================================================================
// AC6 — Drain on empty queue never calls visitor
// ===========================================================================
TEST_F(MailboxFixture, AC6_Drain_EmptyQueueNoVisit) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 4>()));

    int count = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg&) { ++count; });
    EXPECT_EQ(count, 0);

    // Second drain still empty
    mailbox.Drain<Msg>([&](const Address&, const Msg&) { ++count; });
    EXPECT_EQ(count, 0);
}

// ===========================================================================
// AC7 — After drain, subsequent drain finds nothing
// ===========================================================================
TEST_F(MailboxFixture, AC7_Drain_AfterDrainNothingLeft) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 4>()));
    Address a = MakeAddr("r");
    Msg m; m.value = 5;
    mailbox.Send(a, m);

    int firstCount = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg&) { ++firstCount; });
    EXPECT_EQ(firstCount, 1);

    int secondCount = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg&) { ++secondCount; });
    EXPECT_EQ(secondCount, 0);
}

// ===========================================================================
// AC8 — cap=2, send A B (full), send C (overflow) => drain gives B, C; A absent
// ===========================================================================
TEST_F(MailboxFixture, AC8_Overflow_OldestDropped) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 2>()));
    Address a = MakeAddr("r");

    Msg mA; mA.value = 1;
    Msg mB; mB.value = 2;
    Msg mC; mC.value = 3;

    mailbox.Send(a, mA);
    mailbox.Send(a, mB);
    mailbox.Send(a, mC); // overflows: drops A

    std::vector<int> vals;
    mailbox.Drain<Msg>([&](const Address&, const Msg& msg) {
        vals.push_back(msg.value);
    });

    ASSERT_EQ(vals.size(), 2u);
    EXPECT_EQ(vals[0], 2);  // B
    EXPECT_EQ(vals[1], 3);  // C
}

// ===========================================================================
// AC9 — cap=2, send 4 => exactly ONE warning after drain; warning mentions drop count
// ===========================================================================
TEST_F(MailboxFixture, AC9_DropWarning_EmittedOnce_AfterDrain) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 2>()));
    Address a = MakeAddr("r");

    for (int i = 0; i < 4; ++i) {
        Msg m; m.value = i;
        mailbox.Send(a, m);
    }

    // No warning yet (drops haven't been reported)
    EXPECT_EQ(capturedWarnings.size(), 0u);

    mailbox.Drain<Msg>([](const Address&, const Msg&) {});

    // Exactly one warning after drain
    EXPECT_EQ(capturedWarnings.size(), 1u);
    EXPECT_TRUE(HasWarningContaining("dropped"));
}

// ===========================================================================
// AC10 — cap=4, send 1, drain => zero warnings
// ===========================================================================
TEST_F(MailboxFixture, AC10_NoDropWarning_WhenNoOverflow) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 4>()));
    Address a = MakeAddr("r");
    Msg m; m.value = 1;
    mailbox.Send(a, m);

    mailbox.Drain<Msg>([](const Address&, const Msg&) {});

    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ===========================================================================
// AC11 — Assert policy: Release only test (no overflow in Debug)
// ===========================================================================
#ifndef DEBUG
TEST_F(MailboxFixture, AC11_AssertPolicy_Release_ReturnsFalse_DoesNotModifyQueue) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 2>(OverflowPolicy::Assert)));
    Address a = MakeAddr("r");

    Msg m0; m0.value = 10;
    Msg m1; m1.value = 20;

    EXPECT_TRUE(mailbox.Send(a, m0));
    EXPECT_TRUE(mailbox.Send(a, m1));  // queue full

    // This send should return false without modifying the queue
    Msg m2; m2.value = 30;
    EXPECT_FALSE(mailbox.Send(a, m2));

    // Original two messages should be intact
    std::vector<int> vals;
    mailbox.Drain<Msg>([&](const Address&, const Msg& msg) {
        vals.push_back(msg.value);
    });
    ASSERT_EQ(vals.size(), 2u);
    EXPECT_EQ(vals[0], 10);
    EXPECT_EQ(vals[1], 20);
}
#else
TEST_F(MailboxFixture, AC11_AssertPolicy_Debug_NormalSendWorks) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 4>(OverflowPolicy::Assert)));
    Address a = MakeAddr("r");

    Msg m0; m0.value = 10;
    Msg m1; m1.value = 20;

    EXPECT_TRUE(mailbox.Send(a, m0));
    EXPECT_TRUE(mailbox.Send(a, m1));

    int count = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg&) { ++count; });
    EXPECT_EQ(count, 2);
}
#endif

// ===========================================================================
// AC12 — Sending inside visitor defers delivery to next drain
// ===========================================================================
TEST_F(MailboxFixture, AC12_SendDuringDrain_DeferredToNextDrain) {
    ASSERT_TRUE((mailbox.RegisterType<Msg, 8>()));
    Address a = MakeAddr("r");

    Msg first; first.value = 1;
    mailbox.Send(a, first);

    int visitCount = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg& msg) {
        ++visitCount;
        // Send inside visitor
        Msg deferred; deferred.value = 2;
        mailbox.Send(a, deferred);
    });

    // Visitor was called exactly once for the original message
    EXPECT_EQ(visitCount, 1);

    // Second drain delivers the deferred message
    int secondVisitCount = 0;
    int deferredValue    = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg& msg) {
        ++secondVisitCount;
        deferredValue = msg.value;
    });

    EXPECT_EQ(secondVisitCount, 1);
    EXPECT_EQ(deferredValue, 2);
}

// ===========================================================================
// AC13 — Drain<T> for unregistered T: no crash, no warning, visitor never called
// ===========================================================================
TEST_F(MailboxFixture, AC13_Drain_UnregisteredType_NoCrashNoWarning) {
    // Do NOT register Msg — drain should be a silent no-op
    int count = 0;
    mailbox.Drain<Msg>([&](const Address&, const Msg&) { ++count; });
    EXPECT_EQ(count, 0);
    EXPECT_EQ(capturedWarnings.size(), 0u);
}

// ===========================================================================
// AC14 — Independent queues for T1 and T2 do not interfere
// ===========================================================================
struct MsgB { float x = 0.0f; };

TEST_F(MailboxFixture, AC14_TwoTypes_Independent) {
    ASSERT_TRUE((mailbox.RegisterType<Msg,  2>()));
    ASSERT_TRUE((mailbox.RegisterType<MsgB, 4>()));

    Address a = MakeAddr("r");

    // Overflow T1 (cap=2, send 3)
    for (int i = 0; i < 3; ++i) {
        Msg m; m.value = i;
        mailbox.Send(a, m);
    }

    // Send 2 to T2
    MsgB b0; b0.x = 1.0f;
    MsgB b1; b1.x = 2.0f;
    mailbox.Send(a, b0);
    mailbox.Send(a, b1);

    // Drain T1
    std::vector<int> t1vals;
    mailbox.Drain<Msg>([&](const Address&, const Msg& msg) {
        t1vals.push_back(msg.value);
    });
    ASSERT_EQ(t1vals.size(), 2u);

    // Drain T2
    std::vector<float> t2vals;
    mailbox.Drain<MsgB>([&](const Address&, const MsgB& msg) {
        t2vals.push_back(msg.x);
    });
    ASSERT_EQ(t2vals.size(), 2u);
    EXPECT_FLOAT_EQ(t2vals[0], 1.0f);
    EXPECT_FLOAT_EQ(t2vals[1], 2.0f);
}

// ===========================================================================
// AC15 — LargeMsg: send two, drain, all fields correct
// ===========================================================================
TEST_F(MailboxFixture, AC15_LargeMsg_FieldsPreserved) {
    ASSERT_TRUE((mailbox.RegisterType<LargeMsg, 4>()));
    Address a = MakeAddr("large_router", 0xDEADBEEFu);

    LargeMsg lm0;
    lm0.data[0]    = 'H';
    lm0.data[1]    = 'i';
    lm0.data[2]    = '\0';
    lm0.sentinel1  = 0xAABBCCDD;
    lm0.sentinel2  = 0x11223344;

    LargeMsg lm1;
    lm1.data[0]    = 'B';
    lm1.data[1]    = 'y';
    lm1.data[2]    = 'e';
    lm1.data[3]    = '\0';
    lm1.sentinel1  = 0xDEADBEEF;
    lm1.sentinel2  = 0xCAFEBABE;

    mailbox.Send(a, lm0);
    mailbox.Send(a, lm1);

    int count = 0;
    mailbox.Drain<LargeMsg>([&](const Address& addr, const LargeMsg& msg) {
        if (count == 0) {
            EXPECT_EQ(addr.routerId, Dia::Core::StringCRC("large_router"));
            EXPECT_EQ(addr.payload,  0xDEADBEEFu);
            EXPECT_STREQ(msg.data, "Hi");
            EXPECT_EQ(msg.sentinel1, 0xAABBCCDDu);
            EXPECT_EQ(msg.sentinel2, 0x11223344u);
        } else {
            EXPECT_STREQ(msg.data, "Bye");
            EXPECT_EQ(msg.sentinel1, 0xDEADBEEFu);
            EXPECT_EQ(msg.sentinel2, 0xCAFEBABEu);
        }
        ++count;
    });

    EXPECT_EQ(count, 2);
}
