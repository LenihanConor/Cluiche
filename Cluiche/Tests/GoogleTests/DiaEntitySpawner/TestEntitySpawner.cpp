// TestEntitySpawner.cpp
// Exhaustive GoogleTest suite for DiaEntitySpawner (Task 7).
//
// Coverage:
//   EntitySpawner_ImplAPI     — EntitySpawnerImpl public surface (9 tests)
//   EntitySpawner_LifetimeDespawn — TickChildAges lifetime logic (2 tests)
//   EntitySpawner_RadiusDespawn   — TickChildRadii radius logic   (2 tests)
//   EntitySpawner_EmitterModule   — EntitySpawnerModule lifecycle  (6 tests)
//   EntitySpawner_Events          — DespawnCallback reasons        (3 tests)

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

// ============================================================================
// Mock blueprint loader — succeeds unconditionally, counts Load() calls.
// ============================================================================

class MockBlueprintLoader : public Dia::Entity::IBlueprintLoader
{
public:
    int loadCallCount = 0;

    bool Load(Dia::Entity::Domain& /*domain*/, const Json::Value& /*blueprint*/) override
    {
        ++loadCallCount;
        return true;
    }
};

// ============================================================================
// Helper: build a Json::Value config for SpawnEmitterComponent.
//
// StringCRC is serialized as {"value": "<string>"} by the archive system, so
// blueprintId must be written as a nested object.
// ============================================================================

static Json::Value MakeEmitterConfig(
    const char* blueprintId,
    float       rate,
    int         burstCount,
    int         cap,
    float       lifetime,
    float       despawnRadius,
    bool        active)
{
    Json::Value cfg;
    // blueprintId → nested {"value": "…"} because StringCRC serialises via DiaCoreSerializers
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

// ============================================================================
// Helper: build a Json::Value config for SpatialComponent.
// ============================================================================

static Json::Value MakeSpatialConfig(float x, float y, float radius = 1.0f)
{
    Json::Value cfg;
    cfg["position"]["x"] = x;
    cfg["position"]["y"] = y;
    cfg["radius"]        = radius;
    cfg["layerMask"]     = 0;
    return cfg;
}

// ============================================================================
// Base fixture — registers both pools that all tests need.
// ============================================================================

class EntitySpawnerImplTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        domain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpawner::SpawnEmitterComponent>(
            Dia::EntitySpawner::SpawnEmitterComponent::kTypeId));
        domain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpatial::SpatialComponent>(
            Dia::EntitySpatial::SpatialComponent::kTypeId));
    }

    Dia::Entity::Domain    domain;
    MockBlueprintLoader    loader;
    Dia::MessageBus::Bus   bus;
};

// ============================================================================
// Testable module subclass — exposes protected lifecycle methods.
// OnConnectStreams() is NOT called in tests: it requires a full Application,
// and the event-stream writers degrade to no-ops when not connected.
// ============================================================================

class TestableEntitySpawnerModule : public Dia::EntitySpawner::EntitySpawnerModule
{
public:
    TestableEntitySpawnerModule(Dia::Entity::Domain& domain, Dia::Entity::IBlueprintLoader& loader,
                                 Dia::MessageBus::Bus& bus)
        : EntitySpawnerModule(domain, loader, bus)
    {}

    void Start()             { DoStart(); }
    void Update(float dt)
    {
        Dia::SimTime::SimTimeContext ctx{
            Dia::Core::TimeAbsolute::Zero(),
            Dia::Core::TimeRelative::CreateFromSeconds(dt),
            0, 1.0f, false
        };
        DoUpdate(ctx);
    }
    void Stop()              { DoStop(); }
};

// ============================================================================
// Helper: cast IEntitySpawner& back to EntitySpawnerImpl& to reach the
// GetTrackedChildCount() method (not on the interface).
// ============================================================================

static Dia::EntitySpawner::EntitySpawnerImpl& AsImpl(Dia::Entity::IEntitySpawner& spawner)
{
    return static_cast<Dia::EntitySpawner::EntitySpawnerImpl&>(spawner);
}

// ============================================================================
// Suite: EntitySpawner_ImplAPI
// ============================================================================

// 1. No loader → BlueprintNotFound, invalid entity
TEST_F(EntitySpawnerImplTest, SpawnWithNoLoader_ReturnsBlueprintNotFound)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    // Deliberately NOT calling SetBlueprintLoader.

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    Dia::Entity::SpawnResult result = impl.Spawn(req);

    EXPECT_EQ(result.error, Dia::Entity::SpawnError::BlueprintNotFound);
    EXPECT_FALSE(result.entity.IsValid());
}

// 2. With loader → SpawnError::None, valid entity
TEST_F(EntitySpawnerImplTest, SpawnWithLoader_ReturnsValidEntity)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    Dia::Entity::SpawnResult result = impl.Spawn(req);

    EXPECT_EQ(result.error, Dia::Entity::SpawnError::None);
    EXPECT_TRUE(result.entity.IsValid());
}

// 3. Spawn 3 entities → GetTrackedChildCount() == 3
TEST_F(EntitySpawnerImplTest, SpawnAddsToTrackedCount)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");

    impl.Spawn(req);
    impl.Spawn(req);
    impl.Spawn(req);

    EXPECT_EQ(impl.GetTrackedChildCount(), 3u);
}

// 4. Spawn 3, DespawnAll → tracked count drops to 0
TEST_F(EntitySpawnerImplTest, DespawnAll_ClearsTrackedCount)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");

    impl.Spawn(req);
    impl.Spawn(req);
    impl.Spawn(req);
    ASSERT_EQ(impl.GetTrackedChildCount(), 3u);

    impl.DespawnAll();

    EXPECT_EQ(impl.GetTrackedChildCount(), 0u);
}

// 5. Explicit Despawn(entity) removes from tracking
TEST_F(EntitySpawnerImplTest, ExplicitDespawn_RemovesFromTrackingTable)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");

    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);
    ASSERT_EQ(impl.GetTrackedChildCount(), 1u);

    impl.Despawn(result.entity);

    EXPECT_EQ(impl.GetTrackedChildCount(), 0u);
}

// 6. HandleExternalDestroy removes without crashing or double-destroying
TEST_F(EntitySpawnerImplTest, HandleExternalDestroy_RemovesWithoutDomainDestroy)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");

    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);
    ASSERT_EQ(impl.GetTrackedChildCount(), 1u);

    // External destroy: domain is already about to destroy the entity,
    // so the impl must only update its tables (no second QueueDestroy call).
    impl.HandleExternalDestroy(result.entity);

    EXPECT_EQ(impl.GetTrackedChildCount(), 0u);
}

// 7. DespawnOldestChild removes the first child of the given emitter
TEST_F(EntitySpawnerImplTest, DespawnOldestChild_RemovesFirstChild)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    // Create a parent emitter entity
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.EndOfFrame();

    // Create emitter state for the parent
    impl.GetOrCreateEmitterState(emitter);

    // Spawn 2 children that reference the same emitter parent
    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    req.parent      = emitter;

    Dia::Entity::SpawnResult r1 = impl.Spawn(req);
    Dia::Entity::SpawnResult r2 = impl.Spawn(req);
    ASSERT_EQ(r1.error, Dia::Entity::SpawnError::None);
    ASSERT_EQ(r2.error, Dia::Entity::SpawnError::None);
    ASSERT_EQ(impl.GetTrackedChildCount(), 2u);

    impl.DespawnOldestChild(emitter);

    EXPECT_EQ(impl.GetTrackedChildCount(), 1u);
}

// 8. DespawnCallback is invoked on explicit Despawn with Explicit reason
TEST_F(EntitySpawnerImplTest, DespawnCallback_CalledOnDespawn)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    struct CallbackCapture {
        int                        callCount = 0;
        Dia::Entity::DespawnReason lastReason = Dia::Entity::DespawnReason::Explicit;
        Dia::Entity::Entity        lastEntity;

        static void Callback(void* ctx, Dia::Entity::Entity entity, Dia::Entity::DespawnReason reason)
        {
            auto* self = static_cast<CallbackCapture*>(ctx);
            ++self->callCount;
            self->lastReason = reason;
            self->lastEntity = entity;
        }
    };

    CallbackCapture cap;
    impl.SetDespawnCallback(&CallbackCapture::Callback, &cap);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);

    impl.Despawn(result.entity);

    EXPECT_EQ(cap.callCount, 1);
    EXPECT_EQ(cap.lastReason, Dia::Entity::DespawnReason::Explicit);
    EXPECT_EQ(cap.lastEntity, result.entity);
}

// 9. Setting callback to nullptr and despawning does not crash
TEST_F(EntitySpawnerImplTest, DespawnCallback_NotCalledAfterClear)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    struct CallbackCapture {
        int callCount = 0;
        static void Callback(void* ctx, Dia::Entity::Entity, Dia::Entity::DespawnReason)
        {
            static_cast<CallbackCapture*>(ctx)->callCount++;
        }
    };

    CallbackCapture cap;
    impl.SetDespawnCallback(&CallbackCapture::Callback, &cap);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);

    // Clear the callback.
    impl.SetDespawnCallback(nullptr, nullptr);

    // Must not crash.
    impl.Despawn(result.entity);

    EXPECT_EQ(cap.callCount, 0);
}

// ============================================================================
// Suite: EntitySpawner_LifetimeDespawn
// ============================================================================

// 10. TickChildAges despawns child after cumulative age exceeds lifetime
TEST_F(EntitySpawnerImplTest, TickChildAges_DespawnsAfterLifetime)
{
    // Create emitter with lifetime = 1.0f
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 0.0f, 0, 0, 1.0f, 0.0f, true));
    domain.EndOfFrame();

    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    // Spawn a child with this emitter as parent.
    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    req.parent      = emitter;
    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);
    ASSERT_EQ(impl.GetTrackedChildCount(), 1u);

    // Tick 0.5s — child is at age 0.5, lifetime=1.0 → still alive.
    impl.TickChildAges(0.5f);
    EXPECT_EQ(impl.GetTrackedChildCount(), 1u) << "Child should still be alive at age 0.5";

    // Tick another 0.6s — cumulative age 1.1 > 1.0 → despawned.
    impl.TickChildAges(0.6f);
    EXPECT_EQ(impl.GetTrackedChildCount(), 0u) << "Child should have been despawned at age 1.1";
}

// 11. TickChildAges does NOT despawn when no lifetime is set (lifetime == 0)
TEST_F(EntitySpawnerImplTest, TickChildAges_NoDespawnWhenLifetimeIsZero)
{
    // Emitter with lifetime=0 means "no lifetime limit".
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 0.0f, 0, 0, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    req.parent      = emitter;
    impl.Spawn(req);
    ASSERT_EQ(impl.GetTrackedChildCount(), 1u);

    // Even a very large tick should not despawn when lifetime == 0.
    impl.TickChildAges(9999.0f);
    EXPECT_EQ(impl.GetTrackedChildCount(), 1u);
}

// ============================================================================
// Suite: EntitySpawner_RadiusDespawn
// ============================================================================

// 12. TickChildRadii despawns child that moves beyond the despawnRadius
TEST_F(EntitySpawnerImplTest, TickChildRadii_DespawnsWhenBeyondRadius)
{
    // Emitter at (0,0) with despawnRadius = 5.0f.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 0.0f, 0, 0, 0.0f, 5.0f, true));
    domain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(
        emitter, MakeSpatialConfig(0.0f, 0.0f));
    domain.EndOfFrame();

    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    // Spawn child, then manually add a SpatialComponent at (10,0) — beyond radius.
    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    req.parent      = emitter;
    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);
    ASSERT_EQ(impl.GetTrackedChildCount(), 1u);

    // Add the spatial component to the child (at distance 10 > 5).
    domain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(
        result.entity, MakeSpatialConfig(10.0f, 0.0f));
    domain.EndOfFrame();

    impl.TickChildRadii();

    EXPECT_EQ(impl.GetTrackedChildCount(), 0u) << "Child at distance 10 > radius 5 should be despawned";
}

// 13. TickChildRadii does NOT despawn a child within the despawnRadius
TEST_F(EntitySpawnerImplTest, TickChildRadii_NotDespawnedWithinRadius)
{
    // Emitter at (0,0) with despawnRadius = 5.0f.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 0.0f, 0, 0, 0.0f, 5.0f, true));
    domain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(
        emitter, MakeSpatialConfig(0.0f, 0.0f));
    domain.EndOfFrame();

    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    req.parent      = emitter;
    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);

    // Add the spatial component to the child (at distance 3 < 5).
    domain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(
        result.entity, MakeSpatialConfig(3.0f, 0.0f));
    domain.EndOfFrame();

    impl.TickChildRadii();

    EXPECT_EQ(impl.GetTrackedChildCount(), 1u) << "Child at distance 3 < radius 5 should not be despawned";
}

// ============================================================================
// Suite: EntitySpawner_EmitterModule
// ============================================================================

// 14. Module: DoStart + DoStop without any entities — no crash
TEST_F(EntitySpawnerImplTest, Module_DoStart_DoStop_NoCrash)
{
    TestableEntitySpawnerModule mod(domain, loader, bus);
    mod.Start();
    mod.Stop();
    // No assertions needed — the test passes if there is no crash.
}

// 15. Module: rate-based token accumulation spawns when tokens reach 1.0
TEST_F(EntitySpawnerImplTest, Module_RateSpawn_TokenAccumulation)
{
    // Create emitter entity with rate=2.0 (2 spawns per second), active=true.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 2.0f, 0, 0, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    TestableEntitySpawnerModule mod(domain, loader, bus);
    mod.Start();

    // Update 0.4s: tokens = 0.4*2 = 0.8 — no spawn yet.
    mod.Update(0.4f);
    EXPECT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 0u)
        << "No spawn expected — tokens (0.8) < 1.0";

    // Update 0.1s more: cumulative tokens = 0.8 + 0.1*2 = 1.0 → fires 1 spawn.
    mod.Update(0.1f);
    EXPECT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 1u)
        << "Exactly 1 spawn expected after tokens reach 1.0";

    mod.Stop();
}

// 16. Module: burst fires on first activation when burstCount > 0
TEST_F(EntitySpawnerImplTest, Module_BurstSpawn_FiresOnActivation)
{
    // burstCount=3, rate=0 → burst only, no rate-based spawning.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 0.0f, 3, 0, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    TestableEntitySpawnerModule mod(domain, loader, bus);
    mod.Start();

    // First update fires burst.
    mod.Update(0.016f);
    EXPECT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 3u)
        << "Burst of 3 should fire on first update";

    // Second update: burst already fired — no additional spawns.
    mod.Update(0.016f);
    EXPECT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 3u)
        << "Burst should not re-fire on subsequent updates";

    mod.Stop();
}

// 17. Module: cap enforcement via FIFO overflow keeps live count at cap
TEST_F(EntitySpawnerImplTest, Module_CapEnforcement_FIFOOverflow)
{
    // rate=10 spawns/s, cap=2 → never more than 2 live children at once.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 10.0f, 0, 2, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    TestableEntitySpawnerModule mod(domain, loader, bus);
    mod.Start();

    // 1.0s at rate 10 → 10 spawn attempts; cap=2 keeps rolling FIFO, final count = 2.
    mod.Update(1.0f);

    EXPECT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 2u)
        << "Cap=2 must keep live children at exactly 2 regardless of spawn rate";

    mod.Stop();
}

// 18. Module: child with finite lifetime is despawned during DoUpdate
TEST_F(EntitySpawnerImplTest, Module_LifetimeViaModule_ChildDespawnedAfterUpdate)
{
    // burstCount=1, lifetime=0.5s, rate=0 → spawn once, expires at 0.5s.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 0.0f, 1, 0, 0.5f, 0.0f, true));
    domain.EndOfFrame();

    TestableEntitySpawnerModule mod(domain, loader, bus);
    mod.Start();

    // First update fires the burst.
    mod.Update(0.016f);
    EXPECT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 1u)
        << "Burst child should exist after first update";

    // Second update with dt > remaining lifetime → despawn triggered.
    // Cumulative age = 0.016 + 0.5 = 0.516 > 0.5 → despawned.
    mod.Update(0.5f);
    EXPECT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 0u)
        << "Child should be despawned after lifetime exceeded";

    mod.Stop();
}

// 19. Module: DoStop despawns all tracked children
TEST_F(EntitySpawnerImplTest, Module_Shutdown_DespawnsAll)
{
    // burstCount=3 → spawn 3 on first update.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 0.0f, 3, 0, 0.0f, 0.0f, true));
    domain.EndOfFrame();

    TestableEntitySpawnerModule mod(domain, loader, bus);
    mod.Start();
    mod.Update(0.016f);
    ASSERT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 3u)
        << "3 children should exist before Stop";

    mod.Stop();

    EXPECT_EQ(AsImpl(mod.GetSpawner()).GetTrackedChildCount(), 0u)
        << "All children should be despawned by DoStop";
}

// ============================================================================
// Suite: EntitySpawner_Events
// ============================================================================

struct DespawnRecord
{
    int                        callCount = 0;
    Dia::Entity::DespawnReason lastReason = Dia::Entity::DespawnReason::Explicit;
    Dia::Entity::Entity        lastEntity;

    static void Callback(void* ctx, Dia::Entity::Entity entity, Dia::Entity::DespawnReason reason)
    {
        auto* self = static_cast<DespawnRecord*>(ctx);
        ++self->callCount;
        self->lastReason = reason;
        self->lastEntity = entity;
    }
};

// 20. Explicit despawn via Despawn() reports DespawnReason::Explicit
TEST_F(EntitySpawnerImplTest, DespawnCallback_ExplicitReason_RecordedCorrectly)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    DespawnRecord rec;
    impl.SetDespawnCallback(&DespawnRecord::Callback, &rec);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);

    impl.Despawn(result.entity);

    EXPECT_EQ(rec.callCount, 1);
    EXPECT_EQ(rec.lastReason, Dia::Entity::DespawnReason::Explicit);
}

// 21. FIFO cap overflow via DespawnOldestChild() reports DespawnReason::Cap
TEST_F(EntitySpawnerImplTest, DespawnCallback_CapReason_RecordedCorrectly)
{
    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    DespawnRecord rec;
    impl.SetDespawnCallback(&DespawnRecord::Callback, &rec);

    // Create an emitter entity and spawn one child under it.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.EndOfFrame();
    impl.GetOrCreateEmitterState(emitter);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    req.parent      = emitter;
    Dia::Entity::SpawnResult r = impl.Spawn(req);
    ASSERT_EQ(r.error, Dia::Entity::SpawnError::None);
    ASSERT_EQ(impl.GetTrackedChildCount(), 1u);

    impl.DespawnOldestChild(emitter);

    EXPECT_EQ(rec.callCount, 1);
    EXPECT_EQ(rec.lastReason, Dia::Entity::DespawnReason::Cap);
}

// 22. Lifetime expiry via TickChildAges() reports DespawnReason::Lifetime
TEST_F(EntitySpawnerImplTest, DespawnCallback_LifetimeReason_RecordedCorrectly)
{
    // Emitter with lifetime=0.1f.
    Dia::Entity::Entity emitter = domain.CreateEntity();
    domain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        emitter, MakeEmitterConfig("test-bp", 0.0f, 0, 0, 0.1f, 0.0f, true));
    domain.EndOfFrame();

    Dia::EntitySpawner::EntitySpawnerImpl impl(domain);
    impl.SetBlueprintLoader(&loader);

    DespawnRecord rec;
    impl.SetDespawnCallback(&DespawnRecord::Callback, &rec);

    Dia::Entity::SpawnRequest req;
    req.blueprintId = Dia::Core::StringCRC("test-bp");
    req.parent      = emitter;
    Dia::Entity::SpawnResult result = impl.Spawn(req);
    ASSERT_EQ(result.error, Dia::Entity::SpawnError::None);

    // Tick past the lifetime.
    impl.TickChildAges(0.2f);

    EXPECT_EQ(rec.callCount, 1);
    EXPECT_EQ(rec.lastReason, Dia::Entity::DespawnReason::Lifetime);
}
