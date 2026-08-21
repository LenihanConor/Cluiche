// BehaviourTreeBusAdapterTests.cpp
//
// Covers the behaviourtree-observer-bus-adapter feature: BehaviourTreeBusAdapter
// forwards each of the three IBehaviourTreeEventListener callbacks onto
// DiaMessageBus::Bus via Post<T>(Dia::Entity::MakeEntityAddress(owner), ...) —
// never Broadcast<T>() — because a BehaviourTreeComponent is scoped to exactly
// one entity (unlike DiaEconomy's EconomyBusAdapter, which broadcasts because
// an EconomyInstance is not entity-scoped).
//
// No real attach-time call site exists yet for BehaviourTreeComponent (it is
// still attached generically via the component-registration/blueprint
// pipeline, with no BT-specific game wiring that owns both the component and
// the entity handle at the same point) — see
// Dia/DiaBehaviourTree/BehaviourTreeBusAdapter.h's header comment. So this
// file exercises the adapter directly: constructing it with an explicit
// Dia::Entity::Entity obtained from a real Dia::Entity::Domain, exactly as
// Cluiche/Tests/GoogleTests/DiaEntity/EntityRouterBusWiringTests.cpp does for
// generic entity-addressed Bus::Post/Subscribe.

#include <gtest/gtest.h>

#include <DiaBehaviourTree/BehaviourTreeBusAdapter.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>
#include <DiaBehaviourTree/Messages/behaviourtree_messages.h>
#include <DiaBehaviourTree/NodeResult.h>

#include <DiaEntity/Domain.h>
#include <DiaEntity/EntityAddress.h>

#include <DiaMessageBus/Bus.h>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::BehaviourTree;
using namespace Dia::Entity;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

namespace {

// See EntityRouterBusWiringTests.cpp's file header for why this trick is
// needed: Bus::Subscribe<T> only accepts a StringCRC subscriber id and
// narrows it via StringCRC::Value() (a 32-bit unsigned int), so a Bus-side
// subscription must be built from a StringCRC whose Value() equals the exact
// numeric encoding MakeEntitySubscriberId() produces.
StringCRC ToBusSubscriberId(Dia::Mailbox::SubscriberId subId)
{
    StringCRC result;
    static_cast<Dia::Core::CRC&>(result) = static_cast<unsigned int>(subId.value);
    return result;
}

BehaviourTreeAsset LoadFromJsonString(const char* json)
{
    Json::Value root;
    Json::Reader().parse(json, root);
    DynamicArrayC<const char*, 32> errors;
    return BehaviourTreeAsset::LoadFromJson(root, errors);
}

NodeResult AlwaysSucceed(void*, const DynamicArrayC<StringCRC, 8>&)
{
    return NodeResult::kSuccess;
}

// Minimal direct listener used to prove the bus adapter is additive, not
// exclusive — a second, independently-registered listener on the same
// component.
struct SpyListener : public IBehaviourTreeEventListener
{
    int  enteredCount   = 0;
    int  completedCount = 0;
    bool treeCompleted  = false;

    void OnNodeEntered(StringCRC) override { ++enteredCount; }
    void OnNodeCompleted(StringCRC, NodeResult) override { ++completedCount; }
    void OnTreeCompleted(NodeResult) override { treeCompleted = true; }
};

// Fixture: real Domain + Bus + EntityRouter registered on the Bus, mirroring
// EntityRouterBusWiringTests.cpp's composition-root pattern.
struct BehaviourTreeBusAdapterFixture : public ::testing::Test
{
    Dia::Entity::Domain   domain;
    Dia::MessageBus::Bus  bus;

    void SetUp() override
    {
        bus.Initialize();
        bus.RegisterRouter(&domain.GetEntityRouter());
    }
};

} // namespace

// ===========================================================================
// 1. Each callback forwards entity-addressed to Bus::Post with correct payload
// ===========================================================================

TEST_F(BehaviourTreeBusAdapterFixture, OnNodeEntered_PostsEntityAddressedWithCorrectPayload)
{
    Entity owner = domain.CreateEntity();
    ASSERT_TRUE(owner.IsValid());
    BehaviourTreeBusAdapter adapter(bus, owner);

    bool receivedFlag = false;
    Messages::NodeEnteredEvent received{};
    auto handle = bus.Subscribe<Messages::NodeEnteredEvent>(
        ToBusSubscriberId(MakeEntitySubscriberId(owner)),
        [&](const Messages::NodeEnteredEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    adapter.OnNodeEntered(StringCRC{"nodeA"});

    ASSERT_FALSE(receivedFlag) << "Post must not deliver before Bus::Update()";
    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.nodeId, StringCRC{"nodeA"});
}

TEST_F(BehaviourTreeBusAdapterFixture, OnNodeCompleted_PostsEntityAddressedWithCorrectPayload)
{
    Entity owner = domain.CreateEntity();
    ASSERT_TRUE(owner.IsValid());
    BehaviourTreeBusAdapter adapter(bus, owner);

    bool receivedFlag = false;
    Messages::NodeCompletedEvent received{};
    auto handle = bus.Subscribe<Messages::NodeCompletedEvent>(
        ToBusSubscriberId(MakeEntitySubscriberId(owner)),
        [&](const Messages::NodeCompletedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    adapter.OnNodeCompleted(StringCRC{"nodeB"}, NodeResult::kFailure);

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.nodeId, StringCRC{"nodeB"});
    EXPECT_EQ(received.result, NodeResult::kFailure);
}

TEST_F(BehaviourTreeBusAdapterFixture, OnTreeCompleted_PostsEntityAddressedWithCorrectPayload)
{
    Entity owner = domain.CreateEntity();
    ASSERT_TRUE(owner.IsValid());
    BehaviourTreeBusAdapter adapter(bus, owner);

    bool receivedFlag = false;
    Messages::TreeCompletedEvent received{};
    auto handle = bus.Subscribe<Messages::TreeCompletedEvent>(
        ToBusSubscriberId(MakeEntitySubscriberId(owner)),
        [&](const Messages::TreeCompletedEvent& e) { received = e; receivedFlag = true; });
    ASSERT_TRUE(handle.IsValid());

    adapter.OnTreeCompleted(NodeResult::kSuccess);

    bus.Update();

    ASSERT_TRUE(receivedFlag);
    EXPECT_EQ(received.result, NodeResult::kSuccess);
}

// ===========================================================================
// 2. Never Broadcast — the ledger's routerId for a posted message is the
//    entity router, not the broadcast router.
// ===========================================================================

TEST_F(BehaviourTreeBusAdapterFixture, OnNodeEntered_NeverBroadcast_LedgerRouterIsEntityRouter)
{
    Entity owner = domain.CreateEntity();
    ASSERT_TRUE(owner.IsValid());
    BehaviourTreeBusAdapter adapter(bus, owner);

    adapter.OnNodeEntered(StringCRC{"nodeA"});
    bus.Update();

    const Dia::MessageBus::LedgerSnapshot& ledger = bus.GetLastTickLedger();
    bool foundEntry = false;
    for (uint32_t i = 0; i < ledger.entries.Size(); ++i)
    {
        if (ledger.entries[i].typeId == Messages::NodeEnteredEvent::kTypeId)
        {
            foundEntry = true;
            EXPECT_EQ(ledger.entries[i].routerId, Dia::Entity::kEntityRouterId);
            EXPECT_NE(ledger.entries[i].routerId, Dia::MessageBus::Bus::kBroadcastRouterId);
        }
    }
    EXPECT_TRUE(foundEntry) << "Ledger should record the posted NodeEnteredEvent";
}

// ===========================================================================
// 3. A direct listener and the bus adapter both fire from the same Tick —
//    additive, not exclusive.
// ===========================================================================

TEST_F(BehaviourTreeBusAdapterFixture, DirectListenerAndBusAdapter_BothFireFromSameTick)
{
    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SuccessAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"SuccessAction"}, AlwaysSucceed);

    Entity owner = domain.CreateEntity();
    ASSERT_TRUE(owner.IsValid());

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    SpyListener directListener;
    BehaviourTreeBusAdapter busAdapter(bus, owner);

    comp.AddEventListener(&directListener);
    comp.AddEventListener(&busAdapter);

    int busTreeCompletedCount = 0;
    auto handle = bus.Subscribe<Messages::TreeCompletedEvent>(
        ToBusSubscriberId(MakeEntitySubscriberId(owner)),
        [&](const Messages::TreeCompletedEvent&) { ++busTreeCompletedCount; });
    ASSERT_TRUE(handle.IsValid());

    comp.Tick(0.0f);

    // Direct listener fires synchronously, inside Tick() itself.
    EXPECT_GT(directListener.enteredCount, 0);
    EXPECT_GT(directListener.completedCount, 0);
    EXPECT_TRUE(directListener.treeCompleted);

    // Bus adapter's Post() only lands in the Mailbox queue until flushed.
    EXPECT_EQ(busTreeCompletedCount, 0);
    bus.Update();
    EXPECT_EQ(busTreeCompletedCount, 1);

    comp.RemoveEventListener(&directListener);
    comp.RemoveEventListener(&busAdapter);
}

// ===========================================================================
// 4. End-to-end: Bus::Subscribe on the correct entity's derived subscriber id
//    receives the event; a DIFFERENT entity's subscriber does not.
// ===========================================================================

TEST_F(BehaviourTreeBusAdapterFixture, EntityAddressedPost_WrongEntityHandle_NotDelivered)
{
    Entity a = domain.CreateEntity();
    Entity b = domain.CreateEntity();
    ASSERT_TRUE(a.IsValid());
    ASSERT_TRUE(b.IsValid());

    BehaviourTreeBusAdapter adapterForA(bus, a);

    int callCountForB = 0;
    auto handleB = bus.Subscribe<Messages::NodeEnteredEvent>(
        ToBusSubscriberId(MakeEntitySubscriberId(b)),
        [&callCountForB](const Messages::NodeEnteredEvent&) { ++callCountForB; });
    ASSERT_TRUE(handleB.IsValid());

    int callCountForA = 0;
    auto handleA = bus.Subscribe<Messages::NodeEnteredEvent>(
        ToBusSubscriberId(MakeEntitySubscriberId(a)),
        [&callCountForA](const Messages::NodeEnteredEvent&) { ++callCountForA; });
    ASSERT_TRUE(handleA.IsValid());

    adapterForA.OnNodeEntered(StringCRC{"nodeA"});
    bus.Update();

    EXPECT_EQ(callCountForA, 1) << "A's own subscriber must receive the event";
    EXPECT_EQ(callCountForB, 0) << "B's subscriber must never fire for an event addressed to A";
}
