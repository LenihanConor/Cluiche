#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntitySpawner/EntitySpawnerModule.h>
#include <DiaMessageBus/Bus.h>
#include <memory>

namespace Dia { namespace Automation { class AutomationService; } }
namespace Dia { namespace Entity { class IBlueprintLoader; } }

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
namespace CluicheTest { class SpawnerTestDrawer; }
#endif

namespace CluicheTest {

class SpawnerTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Spawner integration stage: rate/burst/cap/explicit despawn emitters";
    explicit SpawnerTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~SpawnerTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 420; }
    static constexpr unsigned int kMinDisplayFrames = 300; // 10 s at 30 Hz
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    // Thin subclass to expose the protected lifecycle methods of EntitySpawnerModule.
    class TestableSpawnerModule : public Dia::EntitySpawner::EntitySpawnerModule
    {
    public:
        TestableSpawnerModule(Dia::Entity::Domain& domain, Dia::Entity::IBlueprintLoader& loader,
                              Dia::MessageBus::Bus& bus)
            : EntitySpawnerModule(domain, loader, bus)
        {}
        void Start()          { DoStart(); }
        void Update(float dt) { DoUpdate(dt); }
        void Stop()           { DoStop(); }
    };

    // Despawn callback (static, forwarded to instance via ctx pointer)
    static void OnDespawn(void* ctx,
                          Dia::Entity::Entity entity,
                          Dia::Entity::DespawnReason reason);

    // Checkpoint flags
    bool mCheckpoint1 = false;   // t>=1.0s: at least 1 entity spawned
    bool mCheckpoint2 = false;   // t>=3.0s: cap never exceeded
    bool mCheckpoint3 = false;   // t>=5.0s: at least 1 lifetime despawn
    bool mCheckpoint4 = false;   // t>=8.0s: explicit despawn triggered

    // Tracking state
    float        mElapsed              = 0.0f;
    unsigned int mPrevTrackedCount     = 0u;
    unsigned int mTotalSpawnedEver     = 0u;
    unsigned int mLifetimeDespawnCount = 0u;
    bool         mCapNeverExceeded     = true;
    bool         mExplicitFired        = false;
    bool         mAllPassed            = false;

    Dia::Entity::Domain                     mDomain;
    Dia::Entity::IBlueprintLoader*          mLoader  = nullptr;   // owned, deleted in dtor
    // Owned locally — this stage has no wider PU-level MessageBus wiring yet, so
    // it stands up its own Bus purely to satisfy EntitySpawnerModule's Bus&
    // dependency. Not shared with any other module in this stage.
    Dia::MessageBus::Bus                    mBus;
    std::unique_ptr<TestableSpawnerModule>  mSpawner;

    Dia::Entity::Entity mRateEmitter;
    Dia::Entity::Entity mBurstEmitter;
    Dia::Entity::Entity mCapEmitter;
    Dia::Entity::Entity mExplicitEmitter;

    // Sequential spawn counter forwarded to the drawer for direction spreading.
    unsigned int mSpawnCounterForDrawer = 0u;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<SpawnerTestDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
