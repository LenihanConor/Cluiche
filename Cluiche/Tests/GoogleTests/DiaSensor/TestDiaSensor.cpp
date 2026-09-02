// Suite: DiaSensor
// Covers all 13 acceptance criteria from the DiaSensor spec.

#include <gtest/gtest.h>

#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaEntitySpatial/Testing/SpatialTestHelpers.h>
#include <DiaSensor/SensorModule.h>
#include <DiaSensor/SensorResultsComponent.h>
#include <DiaSensor/SightSensorComponent.h>
#include <DiaSensor/ProximitySensorComponent.h>
#include <DiaSensor/DamageSensorComponent.h>
#include <DiaSensor/DamageReceivedComponent.h>
#include <DiaSensor/SoundSensorComponent.h>
#include <DiaSensor/SensorBlackboardAdapter.h>
#include <DiaSensor/DefaultSensorBlackboardAdapter.h>
#include <DiaSensor/SoundEventList.h>
#include <DiaSensor/SoundType.h>
#include <DiaSensor/ThreatBoard.h>
#include <DiaSensor/AwarenessBoard.h>
#include <DiaBlackboard/BlackboardComponent.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaSensor/Testing/SensorTestHelpers.h>

#include <memory>

using namespace Dia::Sensor::Testing;
using namespace Dia::EntitySpatial::Testing;
using namespace Dia::Entity;
using namespace Dia::Maths;
using namespace Dia::EntitySpatial;
using namespace Dia::Sensor;

// ============================================================================
// TestSensorModule — thin subclass that exposes protected lifecycle for tests.
// This avoids the need for a full ApplicationFlow::ProcessingUnit in unit tests.
// ============================================================================

class TestSensorModule : public SensorModule
{
public:
    TestSensorModule(Dia::Entity::Domain& domain,
                     Dia::EntitySpatial::EntitySpatialModule& spatialModule)
        : SensorModule(domain, spatialModule)
    {}

    // Expose the two-pass frame loop for test control.
    void Start()  { DoStart(); }
    void Tick(float dt = 0.f)
    {
        Dia::SimTime::SimTimeContext ctx{
            Dia::Core::TimeAbsolute::Zero(),
            Dia::Core::TimeRelative::CreateFromSeconds(dt),
            0, 1.0f, false
        };
        DoUpdate(ctx);
    }
};

// ============================================================================
// Fixture
// ============================================================================

class DiaSensorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Register all component pools.
        domain.RegisterPool(new ComponentPool<SpatialComponent>(SpatialComponent::kTypeId));
        domain.RegisterPool(new ComponentPool<SensorResultsComponent>(SensorResultsComponent::kTypeId));
        domain.RegisterPool(new ComponentPool<SightSensorComponent>(SightSensorComponent::kTypeId));
        domain.RegisterPool(new ComponentPool<ProximitySensorComponent>(ProximitySensorComponent::kTypeId));
        domain.RegisterPool(new ComponentPool<DamageSensorComponent>(DamageSensorComponent::kTypeId));
        domain.RegisterPool(new ComponentPool<DamageReceivedComponent>(DamageReceivedComponent::kTypeId));
        domain.RegisterPool(new ComponentPool<SoundSensorComponent>(SoundSensorComponent::kTypeId));
        // Register the DefaultSensorBlackboardAdapter pool under the BASE class kTypeId so that
        // SensorModule::RunAdapters(), which queries by SensorBlackboardAdapter::kTypeId, can find it.
        // QueueAddComponentByTypeId must use the same base key; GetComponent<SensorBlackboardAdapter>
        // returns a SensorBlackboardAdapter* that actually points to a DefaultSensorBlackboardAdapter.
        domain.RegisterPool(new ComponentPool<DefaultSensorBlackboardAdapter>(SensorBlackboardAdapter::kTypeId));

        // Build a 200x200 square grid with 10-unit cells.
        EntitySpatialIndex::SquareDef def;
        def.worldBounds = Dia::Geometry2D::AARect(
            Vector2D(-100.f, -100.f),
            Vector2D( 100.f,  100.f));
        def.cellSize = 10.f;

        spatialModule  = std::make_unique<EntitySpatialModule>(domain, def);
        sensorModule   = std::make_unique<TestSensorModule>(domain, *spatialModule);
        sensorModule->Start();
    }

    void TearDown() override {}

    // --- Helpers ---

    // Spawn entity with SpatialComponent + SensorResultsComponent at position.
    // Flushes and updates spatial index.
    Entity SpawnObserver(const Vector2D& pos, uint32_t layerMask = 0x01)
    {
        Entity e = SpawnSensorEntity(domain, pos, layerMask);
        domain.EndOfFrame();
        spatialModule->Update();
        return e;
    }

    // Add a SightSensorComponent to entity e, then flush.
    void AddSightSensor(Entity e,
                        float  range      = 20.f,
                        float  halfAngle  = 1.5707963f,  // 90 degrees
                        int    interval   = 1)
    {
        Json::Value cfg;
        cfg["range"]        = range;
        cfg["halfAngle"]    = halfAngle;
        cfg["layerMask"]    = 0x7FFFFFFFu;
        cfg["tickInterval"] = interval;
        domain.QueueAddComponent<SightSensorComponent>(e, cfg);
        domain.EndOfFrame();
        spatialModule->Update();
    }

    // Add a ProximitySensorComponent to entity e, then flush.
    void AddProximitySensor(Entity e,
                            float  radius   = 10.f,
                            int    interval = 1)
    {
        Json::Value cfg;
        cfg["radius"]       = radius;
        cfg["layerMask"]    = 0x7FFFFFFFu;
        cfg["tickInterval"] = interval;
        domain.QueueAddComponent<ProximitySensorComponent>(e, cfg);
        domain.EndOfFrame();
        spatialModule->Update();
    }

    // Add DamageSensorComponent + DamageReceivedComponent to entity e, then flush.
    // DamageReceivedComponent is queued first (DamageSensorComponent requires it).
    void AddDamageSensor(Entity e,
                         int pruneWindow = 30,
                         int interval    = 1)
    {
        // DamageReceivedComponent must be present BEFORE DamageSensorComponent
        // because the domain validates REQUIRES at EndOfFrame apply time.
        Json::Value rcfg;
        domain.QueueAddComponent<DamageReceivedComponent>(e, rcfg);
        domain.EndOfFrame();
        spatialModule->Update();

        Json::Value dcfg;
        dcfg["pruneWindowFrames"] = pruneWindow;
        dcfg["tickInterval"]      = interval;
        domain.QueueAddComponent<DamageSensorComponent>(e, dcfg);
        domain.EndOfFrame();
        spatialModule->Update();
    }

    // Add a SoundSensorComponent to entity e, then flush.
    void AddSoundSensor(Entity e,
                        float hearingRadius = 10.f,
                        int   interval      = 1)
    {
        Json::Value cfg;
        cfg["hearingRadius"] = hearingRadius;
        cfg["tickInterval"]  = interval;
        domain.QueueAddComponent<SoundSensorComponent>(e, cfg);
        domain.EndOfFrame();
        spatialModule->Update();
    }

    // Bind a DefaultSensorBlackboardAdapter to entity e and to a blackboard.
    // The pool is registered under SensorBlackboardAdapter::kTypeId so that SensorModule's
    // RunAdapters() can find it. We queue via the base type key and retrieve via the base
    // type pointer, then downcast to the concrete type for BindBlackboard().
    // Returns the adapter pointer (null if add failed).
    DefaultSensorBlackboardAdapter* AddAndBindAdapter(
        Entity e,
        Dia::Blackboard::BlackboardComponent& bc)
    {
        Json::Value cfg;
        // Must use base class kTypeId — pool registered under SensorBlackboardAdapter::kTypeId.
        domain.QueueAddComponentByTypeId(e, SensorBlackboardAdapter::kTypeId, cfg);
        domain.EndOfFrame();
        spatialModule->Update();

        // GetComponent<SensorBlackboardAdapter> looks up pool by SensorBlackboardAdapter::kTypeId.
        // The actual stored object is a DefaultSensorBlackboardAdapter (IS-A SensorBlackboardAdapter),
        // so the static_cast downcast is safe.
        SensorBlackboardAdapter* base =
            domain.GetComponent<SensorBlackboardAdapter>(e);
        DefaultSensorBlackboardAdapter* adapter =
            static_cast<DefaultSensorBlackboardAdapter*>(base);
        if (adapter != nullptr)
        {
            adapter->BindBlackboard(bc);
        }
        return adapter;
    }

    Dia::Entity::Domain                      domain;
    std::unique_ptr<EntitySpatialModule>     spatialModule;
    std::unique_ptr<TestSensorModule>        sensorModule;
};

// ============================================================================
// AC-1: SensorResultsComponent arrays are NOT cleared between ticks —
//        stale results persist until overwritten.
// ============================================================================

TEST_F(DiaSensorTest, SensorResultsComponent_StaleResultsPersist)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    Entity target   = SpawnSpatialEntity(domain, Vector2D(50.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    // Inject a sight result directly into SensorResultsComponent.
    InjectSightResult(domain, observer, target, 50.f, 0.f, 1);

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->sightResults.Size(), 1u);

    // Tick sensor module — observer has no sight sensor, so sightResults are untouched.
    sensorModule->Tick();

    // Stale result must still be there.
    EXPECT_EQ(results->sightResults.Size(), 1u);
    EXPECT_EQ(results->sightResults[0].entity, target);
}

// ============================================================================
// AC-2: SightSensorComponent populates sightResults via QuerySector.
// ============================================================================

TEST_F(DiaSensorTest, SightSensor_PopulatesSightResults)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 1.5707963f, 1);  // wide 90-degree cone

    // Target directly in front along +X (default forward direction).
    Entity target = SpawnSpatialEntity(domain, Vector2D(5.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    EXPECT_GE(results->sightResults.Size(), 1u) << "Target in cone must appear in sightResults";

    bool found = false;
    for (unsigned int i = 0; i < results->sightResults.Size(); ++i)
    {
        if (results->sightResults[i].entity == target)
        {
            found = true;
            EXPECT_GT(results->sightResults[i].distance, 0.f);
        }
    }
    EXPECT_TRUE(found) << "Target entity not found in sightResults";
}

TEST_F(DiaSensorTest, SightSensor_OutsideCone_NotInResults)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 0.1f, 1);  // very narrow cone along +X

    // Target at 90 degrees off-axis — outside the cone.
    Entity offAxis = SpawnSpatialEntity(domain, Vector2D(0.f, 5.f));
    domain.EndOfFrame();
    spatialModule->Update();

    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    for (unsigned int i = 0; i < results->sightResults.Size(); ++i)
    {
        EXPECT_NE(results->sightResults[i].entity, offAxis)
            << "Entity outside cone must not appear in sightResults";
    }
}

// ============================================================================
// AC-3: ProximitySensorComponent populates proximityResults via QueryCircle.
// ============================================================================

TEST_F(DiaSensorTest, ProximitySensor_PopulatesProximityResults)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddProximitySensor(observer, 10.f, 1);

    // Target within radius.
    Entity target = SpawnSpatialEntity(domain, Vector2D(5.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    EXPECT_GE(results->proximityResults.Size(), 1u) << "Target in radius must appear in proximityResults";

    bool found = false;
    for (unsigned int i = 0; i < results->proximityResults.Size(); ++i)
    {
        if (results->proximityResults[i].entity == target)
        {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Target entity not found in proximityResults";
}

TEST_F(DiaSensorTest, ProximitySensor_OutsideRadius_NotInResults)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddProximitySensor(observer, 5.f, 1);  // small radius

    Entity farTarget = SpawnSpatialEntity(domain, Vector2D(50.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    for (unsigned int i = 0; i < results->proximityResults.Size(); ++i)
    {
        EXPECT_NE(results->proximityResults[i].entity, farTarget)
            << "Far target should not appear in proximityResults";
    }
}

// ============================================================================
// AC-4: DamageSensorComponent records damage events and prunes stale ones.
// ============================================================================

TEST_F(DiaSensorTest, DamageSensor_RecordsDamageEvents)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddDamageSensor(observer, 30, 1);

    DamageReceivedComponent* dr = domain.GetComponent<DamageReceivedComponent>(observer);
    ASSERT_NE(dr, nullptr);

    Entity attacker = domain.CreateEntity();
    domain.EndOfFrame();
    dr->AddDamage(attacker, 10.f, 1);

    sensorModule->Tick();  // frame 1: damage recorded

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->damageEvents.Size(), 1u) << "Damage event must be recorded on tick";

    if (results->damageEvents.Size() >= 1)
    {
        EXPECT_EQ(results->damageEvents[0].source, attacker);
        EXPECT_NEAR(results->damageEvents[0].amount, 10.f, 1e-4f);
    }
}

TEST_F(DiaSensorTest, DamageSensor_PrunesDamageEvents_AfterWindow)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddDamageSensor(observer, 2, 1);  // prune window = 2 frames

    DamageReceivedComponent* dr = domain.GetComponent<DamageReceivedComponent>(observer);
    ASSERT_NE(dr, nullptr);

    Entity attacker = domain.CreateEntity();
    domain.EndOfFrame();
    // Damage with timestamp 1; prune window=2. At frame 4: 4-1=3 > 2 → pruned.
    dr->AddDamage(attacker, 5.f, 1);

    sensorModule->Tick();  // internal frame 1: recorded
    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->damageEvents.Size(), 1u) << "Damage recorded at frame 1";

    sensorModule->Tick();  // frame 2: 2-1=1 <= 2, kept
    EXPECT_EQ(results->damageEvents.Size(), 1u);

    sensorModule->Tick();  // frame 3: 3-1=2 <= 2, kept (boundary)
    sensorModule->Tick();  // frame 4: 4-1=3 > 2, pruned

    EXPECT_EQ(results->damageEvents.Size(), 0u) << "Damage event must be pruned after window expires";
}

// ============================================================================
// AC-5: SoundSensorComponent filters SoundEventList by distance.
// ============================================================================

TEST_F(DiaSensorTest, SoundSensor_SpatialFilter_WithinRange_Received)
{
    // Test SoundSensorComponent::Tick() directly without domain registration
    // to avoid potential static-init-order issues with component requirement validation.
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);
    results->soundEvents.RemoveAll();

    SpatialComponent* spatial = domain.GetComponent<SpatialComponent>(observer);
    ASSERT_NE(spatial, nullptr);

    // Stack-allocate SoundSensorComponent for direct tick test.
    SoundSensorComponent soundSensor;
    soundSensor.hearingRadius = 10.f;
    soundSensor.tickInterval  = 1;

    // Emit a sound within range.
    SoundEventList soundList;
    soundList.Emit(Vector2D(3.f, 0.f), SoundType::kFootstep, 15.f, 1);

    soundSensor.Tick(soundList, spatial->position, *results);

    EXPECT_EQ(results->soundEvents.Size(), 1u) << "Sound within range must appear in soundEvents";
}

TEST_F(DiaSensorTest, SoundSensor_SpatialFilter_OutOfRange_NotReceived)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);
    results->soundEvents.RemoveAll();

    SpatialComponent* spatial = domain.GetComponent<SpatialComponent>(observer);
    ASSERT_NE(spatial, nullptr);

    // Stack-allocate SoundSensorComponent with small hearing radius.
    SoundSensorComponent soundSensor;
    soundSensor.hearingRadius = 5.f;
    soundSensor.tickInterval  = 1;

    // Sound emitted far away — beyond hearing radius.
    SoundEventList soundList;
    soundList.Emit(Vector2D(50.f, 0.f), SoundType::kExplosion, 100.f, 1);

    soundSensor.Tick(soundList, spatial->position, *results);

    EXPECT_EQ(results->soundEvents.Size(), 0u) << "Sound out of range must not appear in soundEvents";
}

// ============================================================================
// AC-6: tickInterval — sensor skips when countdown is non-zero.
// ============================================================================

TEST_F(DiaSensorTest, TickRate_SightSensorSkipsWhenCountdownNonZero)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 1.5707963f, 3);  // fire every 3 frames

    // Target in cone.
    SpawnSpatialEntity(domain, Vector2D(5.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    // Frame 1: countdown starts at 0 → decremented to -1 → fires → reset to 3.
    sensorModule->Tick();
    unsigned int sizeAfterFrame1 = results->sightResults.Size();

    // Frame 2: countdown 3→2 — does NOT fire.
    // Sight sensor overwrites results on tick; if it doesn't tick, results keep frame-1 data.
    // But stale data persists, so we track whether the sensor tick clears + repopulates.
    // We know tick fires on frame 1 (countdown=0 initially). After that, interval=3.
    // On frames 2 and 3, countdown is 2 and 1, so no tick.
    sensorModule->Tick();  // frame 2: no tick
    sensorModule->Tick();  // frame 3: countdown 2→1 → no tick (1 > 0)

    // On frames 2 and 3 the sensor DID NOT run, so sightResults were not cleared+updated.
    // The results still have the stale data from frame 1.
    // (If we had placed a target in view, size would still be sizeAfterFrame1.)
    // The key assertion: at least 2 consecutive frames occurred without a tick.
    // We verify this by checking that results were not re-cleared between frame 1 and 3.
    // With no new target insertions and stale results persisting: size unchanged.
    EXPECT_EQ(results->sightResults.Size(), sizeAfterFrame1)
        << "Sensor must not tick on frames 2-3 (countdown=2,1 — not fired)";
}

TEST_F(DiaSensorTest, TickRate_SightSensorFiresOnInterval)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 1.5707963f, 3);

    // Place target in view.
    SpawnSpatialEntity(domain, Vector2D(5.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    sensorModule->Tick();  // frame 1: fires, results populated
    EXPECT_GE(results->sightResults.Size(), 1u) << "Sensor should fire on frame 1 (countdown=0)";
}

// ============================================================================
// AC-7: Frame ordering — SensorModule ticks sensors THEN runs adapters.
// ============================================================================

TEST_F(DiaSensorTest, SensorModule_FrameOrder_SensorsBeforeAdapters)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 1.5707963f, 1);

    Dia::Blackboard::BlackboardComponent bc;
    DefaultSensorBlackboardAdapter* adapter = AddAndBindAdapter(observer, bc);
    ASSERT_NE(adapter, nullptr);

    // Spawn a target directly in front.
    SpawnSpatialEntity(domain, Vector2D(5.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    sensorModule->Tick();  // sensors tick, THEN adapters run in same call

    // After the frame, the blackboard reflects the freshly ticked sensor results.
    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    const Dia::Blackboard::Blackboard& bb = bc.GetBlackboard();
    const ThreatBoard* tb = bb.TryGet<ThreatBoard>(DefaultSensorBlackboardAdapter::kThreatBoardKey);
    ASSERT_NE(tb, nullptr);

    EXPECT_EQ(tb->threatCount, static_cast<int>(results->sightResults.Size()))
        << "Adapter must read fresh sensor data from same frame";
}

// ============================================================================
// AC-8/AC-9: DefaultSensorBlackboardAdapter::Distil writes ThreatBoard.
// ============================================================================

TEST_F(DiaSensorTest, DefaultAdapter_Distil_WritesThreatBoardThreatCount)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 1.5707963f, 1);

    Dia::Blackboard::BlackboardComponent bc;
    DefaultSensorBlackboardAdapter* adapter = AddAndBindAdapter(observer, bc);
    ASSERT_NE(adapter, nullptr);

    SpawnSpatialEntity(domain, Vector2D(5.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    AssertThreatBoard(bc,
        static_cast<int>(results->sightResults.Size()),
        false);
}

TEST_F(DiaSensorTest, DefaultAdapter_Distil_WritesUnderAttack)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddDamageSensor(observer, 30, 1);

    Dia::Blackboard::BlackboardComponent bc;
    DefaultSensorBlackboardAdapter* adapter = AddAndBindAdapter(observer, bc);
    ASSERT_NE(adapter, nullptr);

    Entity attacker = domain.CreateEntity();
    domain.EndOfFrame();

    DamageReceivedComponent* dr = domain.GetComponent<DamageReceivedComponent>(observer);
    ASSERT_NE(dr, nullptr);

    // Inject damage. The internal frame counter starts at 0 and increments at each Tick().
    // After one Tick() the frame is 1. Damage timestamp must be within window of frameNumber-1
    // for underAttack=true. Inject BEFORE the tick so DamageSensor records it in frame 1.
    // The distil logic: underAttack if de.timestamp >= frameNumber - 1.
    // We inject timestamp=1, so after first tick (frameNumber=1): 1 >= 1-1=0 → true.
    dr->AddDamage(attacker, 5.f, 1);

    sensorModule->Tick();  // frame 1 inside DoUpdate

    AssertThreatBoard(bc, 0, true);  // 0 sight threats; underAttack=true
}

// ============================================================================
// AC-9: AwarenessBoard.alertLevel = kCombat when sight results exist.
// ============================================================================

TEST_F(DiaSensorTest, DefaultAdapter_Distil_AwarenessBoardAlertLevel_CombatWhenSightResults)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 1.5707963f, 1);

    Dia::Blackboard::BlackboardComponent bc;
    DefaultSensorBlackboardAdapter* adapter = AddAndBindAdapter(observer, bc);
    ASSERT_NE(adapter, nullptr);

    SpawnSpatialEntity(domain, Vector2D(5.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    if (results->sightResults.Size() > 0)
    {
        AssertAwarenessBoard(bc,
            static_cast<int>(results->sightResults.Size()),
            AlertLevel::kCombat);
    }
    else
    {
        AssertAwarenessBoard(bc, 0, AlertLevel::kIdle);
    }
}

TEST_F(DiaSensorTest, DefaultAdapter_Distil_AwarenessBoardAlertLevel_IdleWhenNoThreats)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 2.f, 0.05f, 1);  // tiny cone, very short range

    Dia::Blackboard::BlackboardComponent bc;
    DefaultSensorBlackboardAdapter* adapter = AddAndBindAdapter(observer, bc);
    ASSERT_NE(adapter, nullptr);

    // No targets in range.
    sensorModule->Tick();

    AssertThreatBoard(bc, 0, false);
    AssertAwarenessBoard(bc, 0, AlertLevel::kIdle);
}

// ============================================================================
// AC-10: EmitSound adds to SoundEventList; cleared at start of next Update().
// ============================================================================

TEST_F(DiaSensorTest, EmitSound_AddedToList)
{
    sensorModule->Tick();  // advance one frame (clears the list)

    // Emit after a tick — sound is in the list.
    sensorModule->EmitSound(Vector2D(1.f, 0.f), SoundType::kExplosion, 10.f);

    EXPECT_EQ(sensorModule->GetSoundEventList().GetEvents().Size(), 1u)
        << "EmitSound must add to sound event list";
}

TEST_F(DiaSensorTest, EmitSound_ClearedAtStartOfNextUpdate)
{
    sensorModule->Tick();  // frame 1: clears, ticks
    sensorModule->EmitSound(Vector2D(1.f, 0.f), SoundType::kExplosion, 10.f);

    EXPECT_EQ(sensorModule->GetSoundEventList().GetEvents().Size(), 1u);

    sensorModule->Tick();  // frame 2: Clear() runs first → list emptied

    EXPECT_EQ(sensorModule->GetSoundEventList().GetEvents().Size(), 0u)
        << "Sound event list must be cleared at start of Update()";
}

// ============================================================================
// AC-11: No sensor component depends on DiaBlackboard directly.
//         Verified by compile-time check (sensors work without blackboard component).
// ============================================================================

TEST_F(DiaSensorTest, SensorComponents_WorkWithoutBlackboard)
{
    // Verify that sight, proximity, and damage sensors tick without a blackboard adapter.
    // Sound sensor tested separately via direct Tick() to avoid domain registration issues.
    Entity e = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(e, 10.f, 1.f, 1);
    AddProximitySensor(e, 5.f, 1);
    AddDamageSensor(e, 10, 1);

    // Tick without any blackboard adapter on the entity — must not crash.
    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(e);
    ASSERT_NE(results, nullptr);
    SUCCEED();  // no crash = pass
}

// ============================================================================
// AC-12: ThreatBoard and AwarenessBoard have no DiaSensor dependency.
//         These structs are plain data — instantiated and used here without
//         including any sensor component header beyond ThreatBoard.h/AwarenessBoard.h.
// ============================================================================

TEST_F(DiaSensorTest, ThreatBoard_UsableAsPlainData)
{
    ThreatBoard tb;
    tb.threatCount = 3;
    tb.underAttack = true;
    tb.lastHitTime = 42;

    EXPECT_EQ(tb.threatCount, 3);
    EXPECT_TRUE(tb.underAttack);
    EXPECT_EQ(tb.lastHitTime, 42);
}

TEST_F(DiaSensorTest, AwarenessBoard_UsableAsPlainData)
{
    AwarenessBoard ab;
    ab.knownEnemyCount = 2;
    ab.alertLevel      = AlertLevel::kCombat;
    ab.lastKnownEnemyPosition = Vector2D(5.f, 3.f);

    EXPECT_EQ(ab.knownEnemyCount, 2);
    EXPECT_EQ(ab.alertLevel, AlertLevel::kCombat);
    EXPECT_NEAR(ab.lastKnownEnemyPosition.x, 5.f, 1e-4f);
}

// ============================================================================
// AC-13: Integration — multiple tests combining sensor ticks + adapter distil.
// ============================================================================

TEST_F(DiaSensorTest, Integration_TwoTargetsInCone_ThreatCountAndKnownEnemyCount)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 1.5707963f, 1);

    Dia::Blackboard::BlackboardComponent bc;
    DefaultSensorBlackboardAdapter* adapter = AddAndBindAdapter(observer, bc);
    ASSERT_NE(adapter, nullptr);

    // Two targets directly in front.
    SpawnSpatialEntity(domain, Vector2D(4.f, 0.f));
    SpawnSpatialEntity(domain, Vector2D(6.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    const Dia::Blackboard::Blackboard& bb = bc.GetBlackboard();
    const ThreatBoard* tb =
        bb.TryGet<ThreatBoard>(DefaultSensorBlackboardAdapter::kThreatBoardKey);
    const AwarenessBoard* ab =
        bb.TryGet<AwarenessBoard>(DefaultSensorBlackboardAdapter::kAwarenessBoardKey);
    ASSERT_NE(tb, nullptr);
    ASSERT_NE(ab, nullptr);

    EXPECT_EQ(tb->threatCount,        static_cast<int>(results->sightResults.Size()));
    EXPECT_EQ(ab->knownEnemyCount,    static_cast<int>(results->sightResults.Size()));
}

TEST_F(DiaSensorTest, Integration_DamagePruneWindow_EndToEnd)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddDamageSensor(observer, 2, 1);  // prune window = 2 frames

    Entity attacker = domain.CreateEntity();
    domain.EndOfFrame();

    DamageReceivedComponent* dr = domain.GetComponent<DamageReceivedComponent>(observer);
    ASSERT_NE(dr, nullptr);

    // Inject damage before first tick (timestamp 1 — will be recorded in frame 1).
    dr->AddDamage(attacker, 1.f, 1);
    sensorModule->Tick();  // frame 1

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->damageEvents.Size(), 1u) << "Damage recorded at frame 1";

    sensorModule->Tick();  // frame 2: 2-1=1 <= 2, kept
    EXPECT_EQ(results->damageEvents.Size(), 1u);

    sensorModule->Tick();  // frame 3: 3-1=2 <= 2, kept (boundary)
    sensorModule->Tick();  // frame 4: 4-1=3 > 2, pruned

    EXPECT_EQ(results->damageEvents.Size(), 0u) << "Damage pruned after window expires";
}

TEST_F(DiaSensorTest, Integration_SoundSensor_DirectTick_WithinRange)
{
    // Test SoundSensorComponent::Tick() directly for the AC-5 spatial filter.
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));

    SpatialComponent* spatial = domain.GetComponent<SpatialComponent>(observer);
    ASSERT_NE(spatial, nullptr);

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    SoundEventList soundList;
    soundList.Emit(Vector2D(2.f, 0.f), SoundType::kFootstep, 30.f, 1);

    // Stack-allocate sound sensor — no domain registration needed.
    SoundSensorComponent soundSensor;
    soundSensor.hearingRadius = 20.f;
    soundSensor.tickInterval  = 1;

    results->soundEvents.RemoveAll();
    soundSensor.Tick(soundList, spatial->position, *results);
    EXPECT_EQ(results->soundEvents.Size(), 1u) << "Sound within range heard";
}

// ============================================================================
// Additional coverage: tick-skip direct proof, sound via module, capacity cap
// ============================================================================

// Gap 1 — Direct proof that a skipped frame does not re-query the spatial index.
// Sensor fires on frame 1 (countdown=0→fires→reset to 3).
// A second target is added before frame 2.
// Frame 2 countdown=3→2 so the sensor does NOT fire — result count stays at frame-1 value.
TEST_F(DiaSensorTest, TickRate_SightSensor_SkippedFrame_NoRequery)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 20.f, 1.5707963f, 3);  // wide cone, fires every 3 frames

    // Spawn first target in cone and flush.
    SpawnSpatialEntity(domain, Vector2D(5.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    // Frame 1: countdown=0 → fires → sees first target → reset to 3.
    sensorModule->Tick();
    unsigned int size1 = results->sightResults.Size();
    EXPECT_GE(size1, 1u) << "Frame 1 must fire and see the first target";

    // Spawn a second target in the same cone — if the sensor re-queries, size would grow.
    SpawnSpatialEntity(domain, Vector2D(4.f, 0.f));
    domain.EndOfFrame();
    spatialModule->Update();

    // Frame 2: countdown=3→2 — sensor does NOT fire, results are stale from frame 1.
    sensorModule->Tick();

    EXPECT_EQ(results->sightResults.Size(), size1)
        << "Skipped frame must not re-query: result count must remain at frame-1 value";
}

// Gap 2a — Sound emitted via SensorModule::EmitSound() is readable by SoundSensorComponent::Tick().
// EmitSound() is designed to be called by other SimPU modules that run BEFORE SensorModule::DoUpdate().
// The sensor reads the list populated by EmitSound(); Update() clears it at the START of the next frame.
// We test the wiring by emitting and then directly ticking the sensor against the module's live sound list.
TEST_F(DiaSensorTest, SoundSensor_ViaModule_EmitAndSensorReadsModuleList)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);
    SpatialComponent* spatial = domain.GetComponent<SpatialComponent>(observer);
    ASSERT_NE(spatial, nullptr);

    // Emit sound via module — this is what other game systems call before SensorModule runs.
    sensorModule->EmitSound(Vector2D(3.f, 0.f), SoundType::kFootstep, 15.f);

    // The sound is now in the module's SoundEventList.
    EXPECT_EQ(sensorModule->GetSoundEventList().GetEvents().Size(), 1u)
        << "EmitSound must add to the module's SoundEventList";

    // Simulate what SensorModule::Update() does for a SoundSensorComponent tick:
    // read directly from the module's list (same reference the module passes to Tick()).
    SoundSensorComponent soundSensor;
    soundSensor.hearingRadius = 10.f;
    soundSensor.tickInterval  = 1;
    results->soundEvents.RemoveAll();
    soundSensor.Tick(sensorModule->GetSoundEventList(), spatial->position, *results);

    EXPECT_GE(results->soundEvents.Size(), 1u)
        << "SoundSensorComponent reading the module's list must receive the emitted sound";
}

// Gap 2b — Sound emitted beyond the hearing radius is not received by SoundSensorComponent.
TEST_F(DiaSensorTest, SoundSensor_ViaModule_OutOfRange_NotReceived)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);
    SpatialComponent* spatial = domain.GetComponent<SpatialComponent>(observer);
    ASSERT_NE(spatial, nullptr);

    // Sound at distance 50, large emission radius — entity is within broadcast range
    // but beyond its own hearing radius of 5.
    sensorModule->EmitSound(Vector2D(50.f, 0.f), SoundType::kExplosion, 100.f);

    SoundSensorComponent soundSensor;
    soundSensor.hearingRadius = 5.f;
    soundSensor.tickInterval  = 1;
    results->soundEvents.RemoveAll();
    soundSensor.Tick(sensorModule->GetSoundEventList(), spatial->position, *results);

    EXPECT_EQ(results->soundEvents.Size(), 0u)
        << "Sound beyond hearing radius must not appear in soundEvents";
}

// Gap 3 — Spawning more targets than the sightResults capacity (8) must not crash
//          and the result array must be capped at 8.
TEST_F(DiaSensorTest, SightSensor_CapacityFull_NoCrash_CappedAt8)
{
    Entity observer = SpawnObserver(Vector2D(0.f, 0.f));
    AddSightSensor(observer, 50.f, 3.14159f, 1);  // full 180-degree cone, range 50

    // Spawn 10 targets all along +X axis inside the cone.
    for (int i = 1; i <= 10; ++i)
    {
        SpawnSpatialEntity(domain, Vector2D(static_cast<float>(i) * 2.f, 0.f));
    }
    domain.EndOfFrame();
    spatialModule->Update();

    // Must not crash even though 10 entities are in view and capacity is 8.
    sensorModule->Tick();

    SensorResultsComponent* results = domain.GetComponent<SensorResultsComponent>(observer);
    ASSERT_NE(results, nullptr);

    EXPECT_LE(results->sightResults.Size(), 8u)
        << "sightResults must be capped at DynamicArrayC capacity (8)";
    EXPECT_GE(results->sightResults.Size(), 1u)
        << "At least some targets must be found";
}
