// EconomyBusAdapterTests.cpp
//
// Covers the economy-observer-bus-adapter feature spec's acceptance
// criteria: EconomyBusAdapter forwards each of the five IEconomyObserver
// notifications onto DiaMessageBus::Bus via Broadcast<T>(), a direct
// synchronous IEconomyObserver and the adapter both fire from the same
// EconomyObserverSubject::Notify* call, and a Bus::Subscribe<T> handler
// receives the forwarded event end-to-end after a real transaction.

#include <gtest/gtest.h>

#include <DiaEconomy/EconomySchema.h>
#include <DiaEconomy/EconomyInstance.h>
#include <DiaEconomy/EconomySystem.h>
#include <DiaEconomy/IEconomyObserver.h>
#include <DiaEconomy/EconomyBusAdapter.h>
#include <DiaEconomy/Messages/economy_messages.h>
#include <DiaEconomy/Testing/EconomyTestHelpers.h>
#include <DiaMessageBus/Bus.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Economy;
using Dia::Core::StringCRC;

namespace {

Json::Value ParseJson(const char* str)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(str, root);
    return root;
}

EconomySchema MakeSimpleSchema(const char* resource_name = "gold",
                                float min_val = 0.0f,
                                float max_val = 1000.0f,
                                float start   = 100.0f)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{ \"schema_name\": \"test_schema\","
        "  \"resources\": ["
        "    { \"resource_name\": \"%s\","
        "      \"minimum_value\": %.4f,"
        "      \"maximum_value\": %.4f,"
        "      \"starting_value\": %.4f }"
        "  ] }",
        resource_name, min_val, max_val, start);
    return EconomySchema::LoadFromJsonValue(ParseJson(buf));
}

} // namespace

// ===========================================================================
// 1. Each adapter method forwards to Bus::Broadcast with the correct payload
// ===========================================================================

TEST(EconomyBusAdapter, OnPoolChanged_ForwardsToBus)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    EconomyBusAdapter adapter(bus);

    bool receivedFlag = false;
    Messages::PoolChangedEvent received{};
    auto handle = bus.Subscribe<Messages::PoolChangedEvent>(
        StringCRC("test"),
        [&](const Messages::PoolChangedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    PoolChangedEvent ev;
    ev.instanceName = StringCRC("inst_a");
    ev.resourceName = StringCRC("gold");
    ev.newValue     = 150.0f;
    ev.delta        = 50.0f;
    adapter.OnPoolChanged(ev);

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.instanceName, StringCRC("inst_a"));
    EXPECT_EQ(received.resourceName, StringCRC("gold"));
    EXPECT_FLOAT_EQ(received.newValue, 150.0f);
    EXPECT_FLOAT_EQ(received.delta, 50.0f);
}

TEST(EconomyBusAdapter, OnTransactionClamped_ForwardsToBus)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    EconomyBusAdapter adapter(bus);

    bool receivedFlag = false;
    Messages::TransactionClampedEvent received{};
    auto handle = bus.Subscribe<Messages::TransactionClampedEvent>(
        StringCRC("test"),
        [&](const Messages::TransactionClampedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    TransactionClampedEvent ev;
    ev.instanceName    = StringCRC("inst_a");
    ev.resourceName    = StringCRC("gold");
    ev.requestedAmount = 50.0f;
    ev.actualAmount    = 20.0f;
    adapter.OnTransactionClamped(ev);

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.instanceName, StringCRC("inst_a"));
    EXPECT_EQ(received.resourceName, StringCRC("gold"));
    EXPECT_FLOAT_EQ(received.requestedAmount, 50.0f);
    EXPECT_FLOAT_EQ(received.actualAmount, 20.0f);
}

TEST(EconomyBusAdapter, OnTransferCompleted_ForwardsToBus)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    EconomyBusAdapter adapter(bus);

    bool receivedFlag = false;
    Messages::TransferCompletedEvent received{};
    auto handle = bus.Subscribe<Messages::TransferCompletedEvent>(
        StringCRC("test"),
        [&](const Messages::TransferCompletedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    TransferCompletedEvent ev;
    ev.fromInstanceName = StringCRC("inst_a");
    ev.toInstanceName   = StringCRC("inst_b");
    ev.resourceName     = StringCRC("gold");
    ev.amount           = 75.0f;
    adapter.OnTransferCompleted(ev);

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.fromInstanceName, StringCRC("inst_a"));
    EXPECT_EQ(received.toInstanceName, StringCRC("inst_b"));
    EXPECT_EQ(received.resourceName, StringCRC("gold"));
    EXPECT_FLOAT_EQ(received.amount, 75.0f);
}

TEST(EconomyBusAdapter, OnPoolReachedMaximum_ForwardsToBus)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    EconomyBusAdapter adapter(bus);

    bool receivedFlag = false;
    Messages::PoolReachedMaximumEvent received{};
    auto handle = bus.Subscribe<Messages::PoolReachedMaximumEvent>(
        StringCRC("test"),
        [&](const Messages::PoolReachedMaximumEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    PoolReachedMaximumEvent ev;
    ev.instanceName = StringCRC("inst_a");
    ev.resourceName = StringCRC("gold");
    adapter.OnPoolReachedMaximum(ev);

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.instanceName, StringCRC("inst_a"));
    EXPECT_EQ(received.resourceName, StringCRC("gold"));
}

TEST(EconomyBusAdapter, OnPoolReachedMinimum_ForwardsToBus)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    EconomyBusAdapter adapter(bus);

    bool receivedFlag = false;
    Messages::PoolReachedMinimumEvent received{};
    auto handle = bus.Subscribe<Messages::PoolReachedMinimumEvent>(
        StringCRC("test"),
        [&](const Messages::PoolReachedMinimumEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    PoolReachedMinimumEvent ev;
    ev.instanceName = StringCRC("inst_a");
    ev.resourceName = StringCRC("gold");
    adapter.OnPoolReachedMinimum(ev);

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.instanceName, StringCRC("inst_a"));
    EXPECT_EQ(received.resourceName, StringCRC("gold"));
}

// ===========================================================================
// 2. A direct synchronous IEconomyObserver and the adapter both fire from
//    the same Notify() call — registered as two separate subscribers.
// ===========================================================================

TEST(EconomyBusAdapter, DirectObserverAndAdapter_BothFireFromSameNotify)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    EconomyBusAdapter adapter(bus);

    Testing::EventCapture capture;
    capture.Subscribe(sys);
    sys.GetObserverSubject().Subscribe(&adapter);

    bool busGot = false;
    auto handle = bus.Subscribe<Messages::PoolChangedEvent>(
        StringCRC("test"),
        [&](const Messages::PoolChangedEvent&) { busGot = true; });
    ASSERT_TRUE(handle.IsValid());

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 50.0f);
    (void)r;

    // Direct observer fires synchronously, inside Notify() itself.
    EXPECT_EQ(capture.poolChangedCount, 1u);

    // Adapter's Broadcast() only lands in the Mailbox queue until the bus
    // is flushed — not visible to the subscriber until Update().
    EXPECT_FALSE(busGot);
    bus.Update();
    EXPECT_TRUE(busGot);

    sys.GetObserverSubject().Unsubscribe(&adapter);
    capture.Unsubscribe(sys);
}

// ===========================================================================
// 3. Bus::Subscribe<PoolChangedEvent> receives the event end-to-end after a
//    real transaction (EconomySystem::Earn), with correct field values.
// ===========================================================================

TEST(EconomyBusAdapter, RealTransaction_DeliversPoolChangedEventEndToEnd)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    EconomyBusAdapter adapter(bus);
    sys.GetObserverSubject().Subscribe(&adapter);

    bool receivedFlag = false;
    Messages::PoolChangedEvent received{};
    auto handle = bus.Subscribe<Messages::PoolChangedEvent>(
        StringCRC("test"),
        [&](const Messages::PoolChangedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 50.0f);
    (void)r;

    ASSERT_FALSE(receivedFlag); // not delivered until the bus is flushed
    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.instanceName, inst.GetInstanceName());
    EXPECT_EQ(received.resourceName, StringCRC("gold"));
    EXPECT_FLOAT_EQ(received.newValue, 150.0f);
    EXPECT_FLOAT_EQ(received.delta, 50.0f);

    sys.GetObserverSubject().Unsubscribe(&adapter);
}

// ===========================================================================
// 4. Regression proof: PoolChangedEvent is a snapshot taken at emit time, not
//    a live reference re-read at delivery time. Two Earn() calls queue two
//    independent Broadcast()-ed snapshots before either is delivered; only
//    once the bus is flushed must each event show the value it had at its
//    OWN emit time — never both showing the instance's final value at
//    delivery time. This is the specific invariant the PoolChangedEvent
//    redesign (dropping the raw EconomyInstance* for a StringCRC + snapshot
//    floats) exists to guarantee — a raw-pointer payload would silently
//    regress to both events reporting the post-second-Earn value.
// ===========================================================================

TEST(EconomyBusAdapter, PoolChangedEvent_QueuedThenDelivered_ValuesMatchEmitTimeNotDeliveryTime)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    EconomyBusAdapter adapter(bus);
    sys.GetObserverSubject().Subscribe(&adapter);

    Messages::PoolChangedEvent received[2];
    unsigned int receivedCount = 0;
    auto handle = bus.Subscribe<Messages::PoolChangedEvent>(
        StringCRC("test"),
        [&](const Messages::PoolChangedEvent& e) {
            if (receivedCount < 2) received[receivedCount] = e;
            ++receivedCount;
        });
    ASSERT_TRUE(handle.IsValid());

    // First Earn: 100 -> 150. Adapter broadcasts a snapshot with
    // newValue=150 — queued in the Mailbox, not yet delivered.
    TransactionResult r1 = sys.Earn(inst, StringCRC("gold"), 50.0f);
    (void)r1;

    // Second Earn, still before any bus.Update(): 150 -> 350. Mutates the
    // same live EconomyInstance a raw-pointer payload would have pointed
    // at, and queues a second, independent snapshot with newValue=350.
    TransactionResult r2 = sys.Earn(inst, StringCRC("gold"), 200.0f);
    (void)r2;

    ASSERT_EQ(receivedCount, 0u); // nothing delivered until the bus flushes

    bus.Update();

    ASSERT_EQ(receivedCount, 2u);
    // Each delivered event must show the value at ITS OWN emit time, not the
    // instance's final value (350) at delivery time. A dangling/live-pointer
    // regression would show 350 for both.
    EXPECT_FLOAT_EQ(received[0].newValue, 150.0f);
    EXPECT_FLOAT_EQ(received[1].newValue, 350.0f);
    EXPECT_NE(received[0].newValue, received[1].newValue);

    sys.GetObserverSubject().Unsubscribe(&adapter);
}
