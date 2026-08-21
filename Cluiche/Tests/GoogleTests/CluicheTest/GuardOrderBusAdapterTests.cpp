// GuardOrderBusAdapterTests.cpp
//
// Covers the order-observer-bus-adapter feature spec
// (docs/specs/applications/dia/systems/diaorder/order-observer-bus-adapter.md):
// GuardOrderBusAdapter forwards each of the four
// IOrderQueueObserver<GuardOrderContext> callbacks onto DiaMessageBus::Bus
// via Bus::Broadcast<T>() — never forwarding the IOrder<GuardOrderContext>&
// itself — because GuardAgent (declared in BehaviourTreeTestStageModule.h,
// the only real OrderQueue<GuardOrderContext> consumer in the codebase
// today) is plain per-guard state, not a Dia::Entity::Entity, so there is
// no entity handle to Post() against. This mirrors DiaEconomy's
// EconomyBusAdapter, which broadcasts for the same reason (an
// EconomyInstance is likewise not entity-scoped) — contrast with
// DiaBehaviourTree's entity-scoped BehaviourTreeBusAdapter, which Posts.
//
// BehaviourTreeTestStageModule is a Done, checkpoint-gated E2E stage that
// must not be touched (per the feature's constraints) — this file never
// constructs a BehaviourTreeTestStageModule and never registers anything on
// its real GuardAgent/GuardObserver instances. It builds its own, fully
// isolated GuardAgent + OrderQueue<GuardOrderContext> + GuardMoveOrder
// directly (all three are the real types declared in
// BehaviourTreeTestStageModule.h, reused via #include), and registers the
// bus adapter alongside a second, test-local direct observer (SpyObserver,
// deliberately NOT the real GuardOrderObserver, which is tightly coupled to
// the stage module's internals) to prove additive registration.

#include <gtest/gtest.h>

#include <Messages/GuardOrderBusAdapter.h>
#include <Messages/order_messages.h>
#include <Modules/TestStages/BehaviourTreeTestStageModule.h>

#include <DiaOrder/IOrder.h>
#include <DiaOrder/IOrderQueueObserver.h>
#include <DiaOrder/OrderQueue.h>
#include <DiaMessageBus/Bus.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace CluicheTest;
using Dia::Core::StringCRC;

namespace {

// Minimal direct observer used to prove the bus adapter is additive, not
// exclusive — a second, independently-registered observer on the same
// queue. Deliberately NOT the real GuardOrderObserver (tightly coupled to
// BehaviourTreeTestStageModule's internals) — see file header.
struct SpyObserver : public Dia::Order::IOrderQueueObserver<GuardOrderContext>
{
    int startedCount   = 0;
    int finishedCount  = 0;
    int cancelledCount = 0;
    int emptyCount     = 0;

    void OnOrderStarted(const Dia::Order::IOrder<GuardOrderContext>&) override   { ++startedCount; }
    void OnOrderFinished(const Dia::Order::IOrder<GuardOrderContext>&) override  { ++finishedCount; }
    void OnOrderCancelled(const Dia::Order::IOrder<GuardOrderContext>&) override { ++cancelledCount; }
    void OnQueueEmpty() override                                                { ++emptyCount; }
};

// Fixture: a fresh Bus + a fresh, isolated GuardAgent/OrderQueue/GuardMoveOrder
// triple — entirely separate from any real BehaviourTreeTestStageModule
// instance (none is ever constructed in this file).
struct GuardOrderBusAdapterFixture : public ::testing::Test
{
    Dia::MessageBus::Bus bus;
    GuardAgent            guard;
    Dia::Order::OrderQueue<GuardOrderContext> queue;
    GuardMoveOrder         order;

    void SetUp() override
    {
        bus.Initialize();
        guard.position = Dia::Maths::Vector2D(0.f, 0.f);
    }
};

} // namespace

// ===========================================================================
// 1. OnOrderStarted broadcasts the correct orderId and GuardMoveOrder's
//    concrete target/speed fields — recovered via dynamic_cast, never by
//    forwarding the IOrder<GuardOrderContext>& itself.
// ===========================================================================

TEST_F(GuardOrderBusAdapterFixture, OnOrderStarted_BroadcastsOrderIdAndConcreteFields)
{
    order.target = Dia::Maths::Vector2D(5.f, 0.f);
    order.speed  = 2.5f;

    GuardOrderBusAdapter adapter(bus);
    queue.AddObserver(adapter);

    bool receivedFlag = false;
    Messages::OrderStartedEvent received{};
    auto handle = bus.Subscribe<Messages::OrderStartedEvent>(
        StringCRC("test"),
        [&](const Messages::OrderStartedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    queue.Enqueue(&order);
    GuardOrderContext ctx{ &guard };
    queue.Update(ctx, 0.0f);   // promotes + starts `order` -> OnOrderStarted fires

    ASSERT_FALSE(receivedFlag) << "Broadcast must not deliver before Bus::Update()";
    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.orderId, StringCRC("guard.move"));
    EXPECT_FLOAT_EQ(received.target.x, 5.f);
    EXPECT_FLOAT_EQ(received.target.y, 0.f);
    EXPECT_FLOAT_EQ(received.speed, 2.5f);

    queue.RemoveObserver(adapter);
}

// ===========================================================================
// 2. OnOrderFinished broadcasts the correct orderId when the real
//    GuardMoveOrder::Update() reports completion (guard already at target).
// ===========================================================================

TEST_F(GuardOrderBusAdapterFixture, OnOrderFinished_BroadcastsOrderId)
{
    // Guard starts exactly at the order's target, so GuardMoveOrder::Update()
    // returns true (distance < 0.3f) on the very first tick.
    guard.position = Dia::Maths::Vector2D(2.f, 2.f);
    order.target   = Dia::Maths::Vector2D(2.f, 2.f);
    order.speed    = 1.0f;

    GuardOrderBusAdapter adapter(bus);
    queue.AddObserver(adapter);

    bool receivedFlag = false;
    Messages::OrderFinishedEvent received{};
    auto handle = bus.Subscribe<Messages::OrderFinishedEvent>(
        StringCRC("test"),
        [&](const Messages::OrderFinishedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    queue.Enqueue(&order);
    GuardOrderContext ctx{ &guard };
    queue.Update(ctx, 0.0f);   // starts AND finishes `order` on this same tick

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.orderId, StringCRC("guard.move"));

    queue.RemoveObserver(adapter);
}

// ===========================================================================
// 3. OnOrderCancelled broadcasts the correct orderId when EnqueueFront
//    cancels the in-flight order (OrderQueue's own Cancel path — see
//    DiaOrderQueue.EnqueueFrontObserverReceivesCancelledForCurrentOrder in
//    TestOrderQueue.cpp for the underlying queue semantics being relied on).
// ===========================================================================

TEST_F(GuardOrderBusAdapterFixture, OnOrderCancelled_BroadcastsOrderId)
{
    // Far target so the order stays in-flight (never auto-finishes) until
    // EnqueueFront cancels it.
    guard.position = Dia::Maths::Vector2D(0.f, 0.f);
    order.target   = Dia::Maths::Vector2D(100.f, 100.f);
    order.speed    = 1.0f;

    GuardMoveOrder urgentOrder;
    urgentOrder.target = Dia::Maths::Vector2D(0.f, 0.f);
    urgentOrder.speed  = 1.0f;

    GuardOrderBusAdapter adapter(bus);
    queue.AddObserver(adapter);

    bool receivedFlag = false;
    Messages::OrderCancelledEvent received{};
    auto handle = bus.Subscribe<Messages::OrderCancelledEvent>(
        StringCRC("test"),
        [&](const Messages::OrderCancelledEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    queue.Enqueue(&order);
    GuardOrderContext ctx{ &guard };
    queue.Update(ctx, 0.0f);          // `order` becomes current
    queue.EnqueueFront(&urgentOrder); // cancels `order` -> OnOrderCancelled fires

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.orderId, StringCRC("guard.move"));

    queue.RemoveObserver(adapter);
}

// ===========================================================================
// 4. OnQueueEmpty broadcasts a payload-less OrderQueueEmptyEvent when the
//    queue drains to empty (design note: GuardAgent/OrderQueue expose no
//    queue-identifying handle usable outside the owning stage, so — per the
//    spec's open question 3 — this event intentionally carries no payload;
//    a future consumer that needs to disambiguate multiple guards' queues
//    would need its own per-adapter-instance identity, not something
//    OrderQueue<GuardOrderContext> itself can supply).
// ===========================================================================

TEST_F(GuardOrderBusAdapterFixture, OnQueueEmpty_BroadcastsPayloadLessEvent)
{
    guard.position = Dia::Maths::Vector2D(3.f, 3.f);
    order.target   = Dia::Maths::Vector2D(3.f, 3.f); // finishes immediately
    order.speed    = 1.0f;

    GuardOrderBusAdapter adapter(bus);
    queue.AddObserver(adapter);

    int emptyCount = 0;
    auto handle = bus.Subscribe<Messages::OrderQueueEmptyEvent>(
        StringCRC("test"),
        [&](const Messages::OrderQueueEmptyEvent&) { ++emptyCount; });
    ASSERT_TRUE(handle.IsValid());

    queue.Enqueue(&order);
    GuardOrderContext ctx{ &guard };
    queue.Update(ctx, 0.0f);   // starts + finishes `order`, queue drains to empty

    ASSERT_EQ(emptyCount, 0) << "not delivered until Bus::Update()";
    bus.Update();

    EXPECT_EQ(emptyCount, 1);

    queue.RemoveObserver(adapter);
}

// ===========================================================================
// 5. Additive registration: a direct SpyObserver and the bus adapter are
//    both registered on the same queue and both fire from the same ticks —
//    the adapter does not replace or interfere with direct observers.
// ===========================================================================

TEST_F(GuardOrderBusAdapterFixture, BusAdapterAndDirectObserver_BothFireAdditively)
{
    guard.position = Dia::Maths::Vector2D(1.f, 1.f);
    order.target   = Dia::Maths::Vector2D(1.f, 1.f); // finishes immediately
    order.speed    = 1.0f;

    SpyObserver spy;
    GuardOrderBusAdapter adapter(bus);

    queue.AddObserver(spy);
    queue.AddObserver(adapter);

    int busFinishedCount = 0;
    auto handle = bus.Subscribe<Messages::OrderFinishedEvent>(
        StringCRC("test"),
        [&](const Messages::OrderFinishedEvent&) { ++busFinishedCount; });
    ASSERT_TRUE(handle.IsValid());

    queue.Enqueue(&order);
    GuardOrderContext ctx{ &guard };
    queue.Update(ctx, 0.0f);

    // Direct observer fires synchronously, inside Update() itself.
    EXPECT_EQ(spy.startedCount, 1);
    EXPECT_EQ(spy.finishedCount, 1);
    EXPECT_EQ(spy.emptyCount, 1);

    // Adapter's Broadcast() only lands in the Mailbox queue until flushed.
    EXPECT_EQ(busFinishedCount, 0);
    bus.Update();
    EXPECT_EQ(busFinishedCount, 1);

    queue.RemoveObserver(spy);
    queue.RemoveObserver(adapter);
}

// ===========================================================================
// 6. When the started order is NOT a GuardMoveOrder, OrderStartedEvent still
//    carries the correct orderId with target/speed left zero-initialized
//    (the dynamic_cast<const GuardMoveOrder*> branch fails gracefully rather
//    than asserting or crashing).
// ===========================================================================

TEST_F(GuardOrderBusAdapterFixture, OnOrderStarted_NonGuardMoveOrder_LeavesTargetSpeedZero)
{
    struct OtherOrder : Dia::Order::IOrder<GuardOrderContext>
    {
        StringCRC GetOrderId() const override { return StringCRC("other.order"); }
        void Start(GuardOrderContext&) override {}
        bool Update(GuardOrderContext&, float) override { return false; }
        void Finish(GuardOrderContext&) override {}
        void Cancel(GuardOrderContext&) override {}
    };

    OtherOrder otherOrder;

    GuardOrderBusAdapter adapter(bus);
    queue.AddObserver(adapter);

    bool receivedFlag = false;
    Messages::OrderStartedEvent received{};
    auto handle = bus.Subscribe<Messages::OrderStartedEvent>(
        StringCRC("test"),
        [&](const Messages::OrderStartedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    queue.Enqueue(&otherOrder);
    GuardOrderContext ctx{ &guard };
    queue.Update(ctx, 0.0f);

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.orderId, StringCRC("other.order"));
    EXPECT_FLOAT_EQ(received.target.x, 0.f);
    EXPECT_FLOAT_EQ(received.target.y, 0.f);
    EXPECT_FLOAT_EQ(received.speed, 0.f);

    queue.RemoveObserver(adapter);
}
