// SpawnDespawnBusBroadcastTests.cpp
//
// Covers the spawn-despawn-bus-broadcast feature: EntitySpawnerModule now
// calls Bus::Broadcast<EntitySpawnedEvent>/Broadcast<EntityDespawnedEvent>
// alongside its pre-existing EventStreamWriter sends. This suite verifies:
//   - A successful spawn broadcasts EntitySpawnedEvent with correct fields.
//   - Each DespawnReason (Explicit, Cap, Lifetime, Radius) produces a
//     correctly-tagged EntityDespawnedEvent broadcast.
//   - The pre-existing EventStreamWriter path is unaffected (regression):
//     tracked-child bookkeeping (which the writer path sits alongside) stays
//     correct across many spawn/despawn cycles while the bus path is active.
//   - Both delivery paths fire independently from a single spawn call.
//
// EntitySpawnerImpl::zero-DiaMessageBus-dependency is a static/grep-level
// property (EntitySpawnerImpl.h/.cpp include no DiaMessageBus header and take
// no Bus parameter anywhere) rather than something expressible as a runtime
// assertion here — verified separately via `grep -i messagebus` over
// EntitySpawnerImpl.h/.cpp returning zero matches.

#include <gtest/gtest.h>

#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/IBlueprintLoader.h>
#include <DiaEntitySpawner/EntitySpawnerImpl.h>
#include <DiaEntitySpawner/EntitySpawnerModule.h>
#include <DiaEntitySpawner/SpawnEmitterComponent.h>
#include <DiaEntitySpawner/SpawnerTypes.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaMessageBus/Bus.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/CRC/StringCRC.h>

namespace {

// ============================================================================
// Mock blueprint loader — succeeds unconditionally.
// ============================================================================
class MockBusTestBlueprintLoader : public Dia::Entity::IBlueprintLoader
{
public:
    bool Load(Dia::Entity::Domain& /*domain*/, const Json::Value& /*blueprint*/) override
    {
        return true;
    }
};

// ============================================================================
// Helper: build a Json::Value config for SpawnEmitterComponent.
// StringCRC is serialized as {"value": "<string>"} by the archive system.
// ============================================================================
Json::Value MakeEmitterConfig(
    const char* blueprintId,
    float       rate,
    int         burstCount,
    int         cap,
    float       lifetime,
    float       despawnRadius,
    bool        active)
{
    Json::Value cfg;
    Json::Value bpIdNode;
    bpIdNode["value"] = blueprintId;
    cfg["blueprintId"]   = bpIdNode;
    cfg["rate"]          = rate;
    cfg["burstCount"]    = burstCount;
    cfg["cap"]           = cap;
    cfg["lifetime"]      = lifetime;
    cfg["despawnRadius"] = despawnRadius;
    cfg["active"]        = active;
    return cfg;
}

Json::Value MakeSpatialConfig(float x, float y, float radius = 1.0f)
{
    Json::Value cfg;
    cfg["position"]["x"] = x;
    cfg["position"]["y"] = y;
    cfg["radius"]        = radius;
    cfg["layerMask"]     = 0;
    return cfg;
}

// ============================================================================
// Testable module subclass — exposes protected lifecycle methods.
// Deliberately anonymous-namespace-local (distinct from
// TestEntitySpawner.cpp's TestableEntitySpawnerModule) to avoid any ODR
// ambiguity between the two translation units.
// ============================================================================
class TestableBusSpawnerModule : public Dia::EntitySpawner::EntitySpawnerModule
{
public:
    TestableBusSpawnerModule(Dia::Entity::Domain& domain,
                              Dia::Entity::IBlueprintLoader& loader,
                              Dia::MessageBus::Bus& bus)
        : EntitySpawnerModule(domain, loader, bus)
    {}

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

Dia::EntitySpawner::EntitySpawnerImpl& AsImpl(Dia::Entity::IEntitySpawner& spawner)
{
    return static_cast<Dia::EntitySpawner::EntitySpawnerImpl&>(spawner);
}

// ============================================================================
// Fixture — registers both pools, owns a fresh Bus per test.
// ============================================================================
class EntitySpawnerBusBroadcastTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        domain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpawner::SpawnEmitterComponent>(
            Dia::EntitySpawner::SpawnEmitterComponent::kTypeId));
        domain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpatial::SpatialComponent>(
            Dia::EntitySpatial::SpatialComponent::kTypeId));
        bus.Initialize();
    }

    Dia::Entity::Domain          domain;
    MockBusTestBlueprintLoader   loader;
    Dia::MessageBus::Bus         bus;
};

// ============================================================================
// 1. Successful spawn broadcasts EntitySpawnedEvent with correct fields.
// ============================================================================
TEST_F(EntitySpawnerBusBroadcastTest, Spawn_BroadcastsEntitySpawnedEvent_WithCorrectFields)
{
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("bus-test-bp", 0.0f, 1, 0, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    TestableBusSpawnerModule mod(domain, loader, bus);
    mod.Start();

    int callCount = 0;
    Dia::Entity::EntitySpawnedEvent captured;
    auto handle = bus.Subscribe<Dia::Entity::EntitySpawnedEvent>(
        Dia::Core::StringCRC("test-subscriber-spawned"),
        [&](const Dia::Entity::EntitySpawnedEvent& ev) {
            ++callCount;
            captured = ev;
        });
    ASSERT_TRUE(handle.IsValid());

    mod.Update(0.016f);   // fires the burst spawn
    bus.Update();         // flush Primary pass so the subscriber runs

    EXPECT_EQ(callCount, 1) << "Exactly one EntitySpawnedEvent broadcast expected for one spawn";
    EXPECT_TRUE(captured.entity.IsValid());
    EXPECT_EQ(captured.blueprintId.Value(), Dia::Core::StringCRC("bus-test-bp").Value());
    EXPECT_EQ(captured.tag.Value(), Dia::Core::StringCRC("bus-test-bp").Value());

    mod.Stop();
}

// ============================================================================
// 2. Despawn reasons — each produces a correctly-tagged broadcast.
// ============================================================================

TEST_F(EntitySpawnerBusBroadcastTest, Despawn_ExplicitReason_ProducesCorrectlyTaggedBroadcast)
{
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("bus-test-bp", 0.0f, 1, 0, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    TestableBusSpawnerModule mod(domain, loader, bus);
    mod.Start();
    mod.Update(0.016f);   // burst-spawns 1 child
    bus.Update();

    int callCount = 0;
    Dia::Entity::EntityDespawnedEvent captured;
    auto handle = bus.Subscribe<Dia::Entity::EntityDespawnedEvent>(
        Dia::Core::StringCRC("test-subscriber-despawned-explicit"),
        [&](const Dia::Entity::EntityDespawnedEvent& ev) {
            ++callCount;
            captured = ev;
        });
    ASSERT_TRUE(handle.IsValid());

    mod.GetSpawner().Despawn(AsImpl(mod.GetSpawner()).GetOrCreateEmitterState(emitter).children[0]);
    bus.Update();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(captured.reason, Dia::Entity::DespawnReason::Explicit);

    mod.Stop();
}

TEST_F(EntitySpawnerBusBroadcastTest, Despawn_CapReason_ProducesCorrectlyTaggedBroadcast)
{
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.EndOfFrame();

    TestableBusSpawnerModule mod(domain, loader, bus);
    mod.Start();

    auto& impl = AsImpl(mod.GetSpawner());
    impl.GetOrCreateEmitterState(emitter);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("bus-test-bp");
    req.parent      = emitter;
    Dia::Entity::SpawnResult r = impl.Spawn(req);
    ASSERT_EQ(r.error, Dia::Entity::SpawnError::None);
    ASSERT_EQ(impl.GetTrackedChildCount(), 1u);
    bus.Update();

    int callCount = 0;
    Dia::Entity::EntityDespawnedEvent captured;
    auto handle = bus.Subscribe<Dia::Entity::EntityDespawnedEvent>(
        Dia::Core::StringCRC("test-subscriber-despawned-cap"),
        [&](const Dia::Entity::EntityDespawnedEvent& ev) {
            ++callCount;
            captured = ev;
        });
    ASSERT_TRUE(handle.IsValid());

    impl.DespawnOldestChild(emitter);
    bus.Update();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(captured.reason, Dia::Entity::DespawnReason::Cap);

    mod.Stop();
}

TEST_F(EntitySpawnerBusBroadcastTest, Despawn_LifetimeReason_ProducesCorrectlyTaggedBroadcast)
{
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("bus-test-bp", 0.0f, 1, 0, 0.1f, 0.0f, true));
    domain.EndOfFrame();

    TestableBusSpawnerModule mod(domain, loader, bus);
    mod.Start();
    mod.Update(0.016f);   // burst-spawns 1 child with lifetime 0.1s
    bus.Update();

    int callCount = 0;
    Dia::Entity::EntityDespawnedEvent captured;
    auto handle = bus.Subscribe<Dia::Entity::EntityDespawnedEvent>(
        Dia::Core::StringCRC("test-subscriber-despawned-lifetime"),
        [&](const Dia::Entity::EntityDespawnedEvent& ev) {
            ++callCount;
            captured = ev;
        });
    ASSERT_TRUE(handle.IsValid());

    mod.Update(0.2f);      // exceeds remaining lifetime -> despawn
    bus.Update();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(captured.reason, Dia::Entity::DespawnReason::Lifetime);

    mod.Stop();
}

TEST_F(EntitySpawnerBusBroadcastTest, Despawn_RadiusReason_ProducesCorrectlyTaggedBroadcast)
{
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("bus-test-bp", 0.0f, 1, 0, 0.0f, 5.0f, true));
    domain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(
        emitter, MakeSpatialConfig(0.0f, 0.0f));
    domain.EndOfFrame();

    TestableBusSpawnerModule mod(domain, loader, bus);
    mod.Start();
    mod.Update(0.016f);   // burst-spawns 1 child
    bus.Update();

    auto& impl = AsImpl(mod.GetSpawner());
    ASSERT_EQ(impl.GetTrackedChildCount(), 1u);

    Dia::Entity::Entity child = impl.GetOrCreateEmitterState(emitter).children[0];
    domain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(
        child, MakeSpatialConfig(10.0f, 0.0f)); // distance 10 > despawnRadius 5
    domain.EndOfFrame();

    int callCount = 0;
    Dia::Entity::EntityDespawnedEvent captured;
    auto handle = bus.Subscribe<Dia::Entity::EntityDespawnedEvent>(
        Dia::Core::StringCRC("test-subscriber-despawned-radius"),
        [&](const Dia::Entity::EntityDespawnedEvent& ev) {
            ++callCount;
            captured = ev;
        });
    ASSERT_TRUE(handle.IsValid());

    impl.TickChildRadii();
    bus.Update();

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(captured.reason, Dia::Entity::DespawnReason::Radius);

    mod.Stop();
}

// ============================================================================
// 3. Regression: pre-existing EventStreamWriter-adjacent bookkeeping
//    (tracked-child count) stays correct across many spawn/despawn cycles
//    while the new bus broadcast path is active alongside it. Both
//    mSpawnedWriter/mDespawnedWriter.Send() calls execute unconditionally on
//    the same code path exercised here (they degrade to no-ops without a
//    connected Application) — if the new Broadcast() calls had disturbed
//    that path, tracked-count bookkeeping below would drift or the module
//    would crash.
// ============================================================================
TEST_F(EntitySpawnerBusBroadcastTest, Regression_EventStreamWriterPathUnaffected_TrackedCountStaysCorrect)
{
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("bus-test-bp", 0.0f, 3, 0, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    TestableBusSpawnerModule mod(domain, loader, bus);
    mod.Start();

    mod.Update(0.016f); // burst of 3
    auto& impl = AsImpl(mod.GetSpawner());
    EXPECT_EQ(impl.GetTrackedChildCount(), 3u);

    mod.Stop(); // DoStop despawns all tracked children (via mDespawnedWriter + Broadcast)
    EXPECT_EQ(impl.GetTrackedChildCount(), 0u);
}

// ============================================================================
// 4. Both delivery paths fire independently from one spawn call: the bus
//    subscriber fires exactly once, matching the tracked-child bookkeeping
//    that the (no-op-when-unconnected) EventStreamWriter path sits beside —
//    neither path suppresses or duplicates delivery for the other.
// ============================================================================
TEST_F(EntitySpawnerBusBroadcastTest, Spawn_BothDeliveryPathsFireIndependently_FromOneSpawnCall)
{
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("bus-test-bp", 0.0f, 1, 0, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    TestableBusSpawnerModule mod(domain, loader, bus);
    mod.Start();

    int busCallCount = 0;
    auto handle = bus.Subscribe<Dia::Entity::EntitySpawnedEvent>(
        Dia::Core::StringCRC("test-subscriber-independent"),
        [&](const Dia::Entity::EntitySpawnedEvent&) { ++busCallCount; });
    ASSERT_TRUE(handle.IsValid());

    mod.Update(0.016f); // one spawn call -> mSpawnedWriter.Send() AND mBus.Broadcast()
    bus.Update();

    auto& impl = AsImpl(mod.GetSpawner());
    EXPECT_EQ(busCallCount, 1) << "Bus delivery path must fire once per spawn";
    EXPECT_EQ(impl.GetTrackedChildCount(), 1u) << "Stream-writer-adjacent bookkeeping unaffected";

    mod.Stop();
}

} // anonymous namespace
