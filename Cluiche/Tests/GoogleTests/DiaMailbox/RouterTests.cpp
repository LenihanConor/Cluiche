#include <DiaMailbox/Testing/MailboxTestFixture.h>
#include <DiaMailbox/Testing/MockRouter.h>
#include <DiaMailbox/Testing/TestMessages.h>
#include <gtest/gtest.h>

// sCurrentFixture is defined in TypedQueueTests.cpp — do NOT redefine here.

// ---------------------------------------------------------------------------
// Local test message types (avoid clashing with other test files)
// ---------------------------------------------------------------------------
struct RouterMsg  { int   v = 0; };
struct RouterMsgA { int   x = 0; };
struct RouterMsgB { float y = 0.0f; };

using namespace Dia::Mailbox;

// ---------------------------------------------------------------------------
// AC1: RegisterRouter returns true; pointer is stored
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC1_RegisterRouter_ReturnsTrue) {
    Dia::Core::StringCRC id("router_a");
    Dia::Mailbox::Testing::MockRouter router(id);

    EXPECT_TRUE(mailbox.RegisterRouter(&router));
}

// ---------------------------------------------------------------------------
// AC2: RegisterRouter with duplicate ID returns false; original is kept
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC2_RegisterRouter_DuplicateReturnsFalse) {
    Dia::Core::StringCRC id("router_dup");
    Dia::Mailbox::Testing::MockRouter r1(id);
    Dia::Mailbox::Testing::MockRouter r2(id);

    ASSERT_TRUE(mailbox.RegisterRouter(&r1));
    EXPECT_FALSE(mailbox.RegisterRouter(&r2));
    // Original is still reachable
    EXPECT_EQ(mailbox.GetRouter(id), &r1);
}

// ---------------------------------------------------------------------------
// AC3: GetRouter by ID returns the registered pointer
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC3_GetRouter_ReturnsRegisteredPointer) {
    Dia::Core::StringCRC id("router_get");
    Dia::Mailbox::Testing::MockRouter router(id);

    mailbox.RegisterRouter(&router);
    EXPECT_EQ(mailbox.GetRouter(id), &router);
}

// ---------------------------------------------------------------------------
// AC4: GetRouter for unknown ID returns nullptr
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC4_GetRouter_UnknownId_ReturnsNullptr) {
    Dia::Core::StringCRC id("nonexistent");
    EXPECT_EQ(mailbox.GetRouter(id), nullptr);
}

// ---------------------------------------------------------------------------
// AC5 + AC6: Resolve calls router->Resolve once and fills outMatched correctly
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC5_AC6_Resolve_CallsRouter_FillsOutMatched) {
    ASSERT_TRUE((mailbox.RegisterType<RouterMsg, 8>()));

    Dia::Core::StringCRC routerId("entity_router");
    Dia::Mailbox::Testing::MockRouter router(routerId);

    SubscriberId idA{1};
    SubscriberId idB{2};
    mailbox.Subscribe<RouterMsg>(idA);
    mailbox.Subscribe<RouterMsg>(idB);

    // Rule: payload=100 matches only A
    SubscriberSet matchA;
    matchA.Add(idA);
    router.AddRule(100, matchA);

    ASSERT_TRUE(mailbox.RegisterRouter(&router));

    Address addr;
    addr.routerId = routerId;
    addr.payload  = 100;

    SubscriberSet outMatched;
    bool result = mailbox.Resolve<RouterMsg>(addr, outMatched);

    EXPECT_TRUE(result);
    EXPECT_EQ(router.ResolveCallCount(), 1);
    EXPECT_EQ(outMatched.Size(), 1u);
    EXPECT_EQ(outMatched[0], idA);
}

// ---------------------------------------------------------------------------
// AC7: Resolve with null routerId returns false, no warning, outMatched empty
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC7_Resolve_NullRouterId_ReturnsFalse_NoWarning) {
    ASSERT_TRUE((mailbox.RegisterType<RouterMsg, 8>()));

    Address addr;
    addr.routerId = Dia::Core::StringCRC{};   // zero/null
    addr.payload  = 42;

    SubscriberSet outMatched;
    // Pre-populate to verify it is cleared
    outMatched.Add(SubscriberId{99});

    bool result = mailbox.Resolve<RouterMsg>(addr, outMatched);

    EXPECT_FALSE(result);
    EXPECT_EQ(outMatched.Size(), 0u);
    EXPECT_TRUE(capturedWarnings.empty());
}

// ---------------------------------------------------------------------------
// AC8: Resolve with unregistered non-null routerId returns false, warning emitted
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC8_Resolve_UnregisteredRouterId_ReturnsFalse_EmitsWarning) {
    ASSERT_TRUE((mailbox.RegisterType<RouterMsg, 8>()));

    Address addr;
    addr.routerId = Dia::Core::StringCRC("unknown_router");
    addr.payload  = 10;

    SubscriberSet outMatched;
    bool result = mailbox.Resolve<RouterMsg>(addr, outMatched);

    EXPECT_FALSE(result);
    EXPECT_EQ(outMatched.Size(), 0u);
    EXPECT_TRUE(HasWarningContaining("Resolve"));
}

// ---------------------------------------------------------------------------
// AC9: Resolve picks per-type subscriber list — RouterMsgA and RouterMsgB
//      subscribers are separate; resolving RouterMsgA only sees RouterMsgA subs
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC9_Resolve_PerTypeSeparation) {
    ASSERT_TRUE((mailbox.RegisterType<RouterMsgA, 8>()));
    ASSERT_TRUE((mailbox.RegisterType<RouterMsgB, 8>()));

    Dia::Core::StringCRC routerId("type_sep_router");
    Dia::Mailbox::Testing::MockRouter router(routerId);
    ASSERT_TRUE(mailbox.RegisterRouter(&router));

    SubscriberId subA{10};
    SubscriberId subB{20};
    mailbox.Subscribe<RouterMsgA>(subA);
    mailbox.Subscribe<RouterMsgB>(subB);

    // Rule: payload=1 — router tries to return both subA and subB
    SubscriberSet ruleResult;
    ruleResult.Add(subA);
    ruleResult.Add(subB);
    router.AddRule(1, ruleResult);

    Address addr;
    addr.routerId = routerId;
    addr.payload  = 1;

    SubscriberSet outMatched;
    bool result = mailbox.Resolve<RouterMsgA>(addr, outMatched);

    EXPECT_TRUE(result);
    // Only subA is in RouterMsgA's live subscriber list; subB is not
    // MockRouter::Resolve filters by liveSubscribers, so only subA should appear
    EXPECT_EQ(outMatched.Size(), 1u);
    EXPECT_EQ(outMatched[0], subA);
}

// ---------------------------------------------------------------------------
// AC11: MockRouter AddRule causes Resolve to return configured subscribers
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC11_MockRouter_AddRule_ReturnsConfiguredSubscribers) {
    ASSERT_TRUE((mailbox.RegisterType<RouterMsg, 8>()));

    Dia::Core::StringCRC routerId("mock_rule_router");
    Dia::Mailbox::Testing::MockRouter router(routerId);
    ASSERT_TRUE(mailbox.RegisterRouter(&router));

    SubscriberId idX{5};
    SubscriberId idY{6};
    mailbox.Subscribe<RouterMsg>(idX);
    mailbox.Subscribe<RouterMsg>(idY);

    SubscriberSet ruleResult;
    ruleResult.Add(idX);
    ruleResult.Add(idY);
    router.AddRule(200, ruleResult);

    Address addr;
    addr.routerId = routerId;
    addr.payload  = 200;

    SubscriberSet outMatched;
    mailbox.Resolve<RouterMsg>(addr, outMatched);

    EXPECT_EQ(outMatched.Size(), 2u);
}

// ---------------------------------------------------------------------------
// AC12: MockRouter ResolveCallCount starts at 0, increments per call
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC12_MockRouter_ResolveCallCount) {
    ASSERT_TRUE((mailbox.RegisterType<RouterMsg, 8>()));

    Dia::Core::StringCRC routerId("count_router");
    Dia::Mailbox::Testing::MockRouter router(routerId);
    ASSERT_TRUE(mailbox.RegisterRouter(&router));

    EXPECT_EQ(router.ResolveCallCount(), 0);

    Address addr;
    addr.routerId = routerId;
    addr.payload  = 0;

    SubscriberSet out;
    mailbox.Resolve<RouterMsg>(addr, out);
    EXPECT_EQ(router.ResolveCallCount(), 1);

    mailbox.Resolve<RouterMsg>(addr, out);
    EXPECT_EQ(router.ResolveCallCount(), 2);
}

// ---------------------------------------------------------------------------
// AC13: MockRouter ClearRules causes empty outMatched on subsequent Resolve
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC13_MockRouter_ClearRules_EmptyResult) {
    ASSERT_TRUE((mailbox.RegisterType<RouterMsg, 8>()));

    Dia::Core::StringCRC routerId("clear_router");
    Dia::Mailbox::Testing::MockRouter router(routerId);
    ASSERT_TRUE(mailbox.RegisterRouter(&router));

    SubscriberId idZ{77};
    mailbox.Subscribe<RouterMsg>(idZ);

    SubscriberSet matched;
    matched.Add(idZ);
    router.AddRule(50, matched);

    Address addr;
    addr.routerId = routerId;
    addr.payload  = 50;

    // Verify rule works first
    SubscriberSet out;
    mailbox.Resolve<RouterMsg>(addr, out);
    EXPECT_EQ(out.Size(), 1u);

    // Clear and verify empty
    router.ClearRules();
    out.RemoveAll();
    mailbox.Resolve<RouterMsg>(addr, out);
    EXPECT_EQ(out.Size(), 0u);
    EXPECT_EQ(router.ResolveCallCount(), 1); // reset by ClearRules
}

// ---------------------------------------------------------------------------
// AC14: TestMessageA/B from Testing namespace — register, send, drain inline
// ---------------------------------------------------------------------------
TEST_F(MailboxFixture, AC14_TestMessages_RegisterSendDrain) {
    using namespace Dia::Mailbox::Testing;

    ASSERT_TRUE((mailbox.RegisterType<TestMessageA, 8>()));

    Address a;
    a.routerId = Dia::Core::StringCRC("test");
    a.payload  = 1;

    TestMessageA msg;
    msg.value = 42;
    ASSERT_TRUE(mailbox.Send(a, msg));

    int count = 0;
    mailbox.Drain<TestMessageA>([&](const Address&, const TestMessageA& m) {
        ++count;
        EXPECT_EQ(m.value, 42);
    });
    EXPECT_EQ(count, 1);
}
