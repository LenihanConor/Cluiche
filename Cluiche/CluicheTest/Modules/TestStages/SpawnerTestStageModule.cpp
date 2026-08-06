#include "Modules/TestStages/SpawnerTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/IBlueprintLoader.h>
#include <DiaEntitySpawner/EntitySpawnerImpl.h>
#include <DiaEntitySpawner/SpawnEmitterComponent.h>
#include <DiaEntitySpawner/SpawnerTypes.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// MockBlueprintLoader — unconditionally returns true, no-op load.
// ---------------------------------------------------------------------------
namespace {

class MockBlueprintLoader : public Dia::Entity::IBlueprintLoader
{
public:
    bool Load(Dia::Entity::Domain& /*domain*/, const Json::Value& /*blueprint*/) override
    {
        return true;
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Helper: build a Json::Value config for SpawnEmitterComponent.
// StringCRC is serialized as {"value": "<string>"} by the archive system.
// ---------------------------------------------------------------------------
static Json::Value MakeEmitterConfig(
    const char* blueprintId,
    float       rate,
    int         burstCount,
    int         cap,
    float       lifetime,
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
    cfg["despawnRadius"] = 0.0f;
    cfg["active"]        = active;
    return cfg;
}

namespace CluicheTest {

const Dia::Core::StringCRC SpawnerTestStageModule::kTypeId("SpawnerTestStageModule");

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

SpawnerTestStageModule::SpawnerTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{
}

SpawnerTestStageModule::~SpawnerTestStageModule()
{
    delete mLoader;
    mLoader = nullptr;
}

// ---------------------------------------------------------------------------
// TestStageModuleBase overrides
// ---------------------------------------------------------------------------

Dia::Core::StringCRC SpawnerTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("SpawnerTestStage");
}

const Dia::Core::StringCRC* SpawnerTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("spawner.checkpoint_1_entities_spawned"),
        Dia::Core::StringCRC("spawner.checkpoint_2_cap_respected"),
        Dia::Core::StringCRC("spawner.checkpoint_3_lifetime_recycled"),
        Dia::Core::StringCRC("spawner.checkpoint_4_explicit_despawn"),
    };
    outCount = 4;
    return names;
}

// ---------------------------------------------------------------------------
// OnDespawn (static callback)
// ---------------------------------------------------------------------------

void SpawnerTestStageModule::OnDespawn(void* ctx,
                                       Dia::Entity::Entity /*entity*/,
                                       Dia::Entity::DespawnReason reason)
{
    SpawnerTestStageModule* self = static_cast<SpawnerTestStageModule*>(ctx);
    if (reason == Dia::Entity::DespawnReason::Lifetime)
    {
        ++self->mLifetimeDespawnCount;
    }
}

// ---------------------------------------------------------------------------
// OnStart
// ---------------------------------------------------------------------------

void SpawnerTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // Reset all state for re-entry.
    mElapsed              = 0.0f;
    mPrevTrackedCount     = 0u;
    mTotalSpawnedEver     = 0u;
    mLifetimeDespawnCount = 0u;
    mCapNeverExceeded     = true;
    mExplicitFired        = false;
    mAllPassed            = false;
    mCheckpoint1          = false;
    mCheckpoint2          = false;
    mCheckpoint3          = false;
    mCheckpoint4          = false;

    // Register component pool.
    mDomain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpawner::SpawnEmitterComponent>(
        Dia::EntitySpawner::SpawnEmitterComponent::kTypeId));

    // Create the mock loader (owned by this module).
    mLoader = new MockBlueprintLoader();

    // Create the spawner module.
    mSpawner = std::make_unique<TestableSpawnerModule>(mDomain, *mLoader);
    mSpawner->Start();

    // Override DespawnCallback so we can count lifetime despawns.
    // The module's own callback was set in Start(); we replace it here.
    auto& impl = static_cast<Dia::EntitySpawner::EntitySpawnerImpl&>(mSpawner->GetSpawner());
    impl.SetDespawnCallback(&SpawnerTestStageModule::OnDespawn, this);

    // --- Create 4 emitter entities ---

    // 1. RateEmitter — rate=2.0f/s, cap=0 (unlimited), lifetime=3.0f, active=true
    mRateEmitter = mDomain.CreateEntity();
    mDomain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        mRateEmitter,
        MakeEmitterConfig("test-bp", 2.0f, 0, 0, 3.0f, true));

    // 2. BurstEmitter — burstCount=5, rate=0, lifetime=5.0f, active=true
    mBurstEmitter = mDomain.CreateEntity();
    mDomain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        mBurstEmitter,
        MakeEmitterConfig("test-bp", 0.0f, 5, 0, 5.0f, true));

    // 3. CapEmitter — rate=5.0f/s, cap=3, lifetime=0 (infinite), active=true
    mCapEmitter = mDomain.CreateEntity();
    mDomain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        mCapEmitter,
        MakeEmitterConfig("test-bp", 5.0f, 0, 3, 0.0f, true));

    // 4. ExplicitEmitter — burstCount=2, rate=0, lifetime=0, active=true
    mExplicitEmitter = mDomain.CreateEntity();
    mDomain.QueueAddComponent<Dia::EntitySpawner::SpawnEmitterComponent>(
        mExplicitEmitter,
        MakeEmitterConfig("test-bp", 0.0f, 2, 0, 0.0f, true));

    // Commit all queued component additions before first update.
    mDomain.EndOfFrame();

    // Register checkpoints with the AutomationService.
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("spawner.checkpoint_1_entities_spawned"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckpoint1, mCheckpoint1 ? "At least 1 entity spawned" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("spawner.checkpoint_2_cap_respected"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckpoint2, mCheckpoint2 ? "Cap=3 never exceeded" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("spawner.checkpoint_3_lifetime_recycled"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckpoint3, mCheckpoint3 ? "At least 1 lifetime despawn" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("spawner.checkpoint_4_explicit_despawn"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckpoint4, mCheckpoint4 ? "Explicit despawn removed children" : "pending", 0.0f };
        });

    DIA_LOG_INFO("CluicheTest", "SpawnerTestStageModule::OnStart — 4 emitters created");
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------

void SpawnerTestStageModule::OnUpdate(float deltaTime)
{
    if (!mSpawner)
        return;

    mElapsed += deltaTime;

    // Tick the spawner module.
    mSpawner->Update(deltaTime);

    // Track spawn count by observing change in GetTrackedChildCount().
    auto& impl = static_cast<Dia::EntitySpawner::EntitySpawnerImpl&>(mSpawner->GetSpawner());
    const unsigned int currentCount = impl.GetTrackedChildCount();
    if (currentCount > mPrevTrackedCount)
    {
        mTotalSpawnedEver += (currentCount - mPrevTrackedCount);
    }
    mPrevTrackedCount = currentCount;

    // Check cap constraint: CapEmitter's emitter state should never have >3 children.
    // Cap enforcement is inside EntitySpawnerModule, so if the impl is correct,
    // mCapNeverExceeded stays true throughout.
    {
        Dia::EntitySpawner::EmitterState& capState = impl.GetOrCreateEmitterState(mCapEmitter);
        if (static_cast<int>(capState.children.Size()) > 3)
        {
            mCapNeverExceeded = false;
        }
    }

    // Checkpoint 1 — t>=1.0s: at least 1 entity spawned
    if (!mCheckpoint1 && mElapsed >= 1.0f && mTotalSpawnedEver >= 1u)
    {
        mCheckpoint1 = true;
        DIA_LOG_INFO("CluicheTest", "SpawnerTestStageModule: checkpoint 1 passed (spawned=%u)", mTotalSpawnedEver);
    }

    // Checkpoint 2 — t>=3.0s: cap never exceeded
    if (!mCheckpoint2 && mElapsed >= 3.0f && mCapNeverExceeded)
    {
        mCheckpoint2 = true;
        DIA_LOG_INFO("CluicheTest", "SpawnerTestStageModule: checkpoint 2 passed (cap respected)");
    }

    // Checkpoint 3 — t>=5.0s: at least 1 lifetime despawn
    if (!mCheckpoint3 && mElapsed >= 5.0f && mLifetimeDespawnCount >= 1u)
    {
        mCheckpoint3 = true;
        DIA_LOG_INFO("CluicheTest", "SpawnerTestStageModule: checkpoint 3 passed (lifetimeDespawn=%u)", mLifetimeDespawnCount);
    }

    // Checkpoint 4 — trigger explicit despawn once at t>=8.0s, then verify
    if (!mExplicitFired && mElapsed >= 8.0f)
    {
        impl.DespawnAll();
        mExplicitFired = true;
        DIA_LOG_INFO("CluicheTest", "SpawnerTestStageModule: explicit DespawnAll() triggered at t=%.2f", mElapsed);
    }

    if (!mCheckpoint4 && mExplicitFired && impl.GetTrackedChildCount() == 0u)
    {
        mCheckpoint4 = true;
        DIA_LOG_INFO("CluicheTest", "SpawnerTestStageModule: checkpoint 4 passed (all children despawned)");
    }

    // All 4 checkpoints done → report passed.
    if (!mAllPassed && mCheckpoint1 && mCheckpoint2 && mCheckpoint3 && mCheckpoint4)
    {
        mAllPassed = true;
        if (!IsResolved())
            ReportPassed();
    }
}

// ---------------------------------------------------------------------------
// OnStop
// ---------------------------------------------------------------------------

void SpawnerTestStageModule::OnStop()
{
    if (mSpawner)
    {
        mSpawner->Stop();
        mSpawner.reset();
    }

    delete mLoader;
    mLoader = nullptr;

    // Reset flags
    mElapsed              = 0.0f;
    mPrevTrackedCount     = 0u;
    mTotalSpawnedEver     = 0u;
    mLifetimeDespawnCount = 0u;
    mCapNeverExceeded     = true;
    mExplicitFired        = false;
    mAllPassed            = false;
    mCheckpoint1          = false;
    mCheckpoint2          = false;
    mCheckpoint3          = false;
    mCheckpoint4          = false;

    DIA_LOG_INFO("CluicheTest", "SpawnerTestStageModule::OnStop");
}

} // namespace CluicheTest

namespace { using SpawnerTestStageModule_ = CluicheTest::SpawnerTestStageModule; }
DIA_MODULE(SpawnerTestStageModule_);
DIA_DESCRIBE(SpawnerTestStageModule_::kTypeId, "Spawner integration stage: rate/burst/cap/explicit despawn emitters");
