#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include "Modules/TestStages/Entity/TransformComponent.h"
#include "Modules/TestStages/Entity/VisualTestRenderComponent.h"
#include "Modules/TestStages/Entity/PickableCircleComponent.h"
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Entity.h>
#include "Modules/EntityModule.h"

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/PickingModule.h"
#include <memory>
#endif

namespace CluicheTest {

#ifdef DIA_DEBUG
class EntityTestDrawer;
#endif

class EntityTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Validates DiaEntity: spawn/destroy/hierarchy/query/mailbox/lifecycle";
    explicit EntityTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~EntityTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 120; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool AreDependenciesReady() override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void SetupScene();
    void RegisterCheckpoints(Dia::Automation::AutomationService* service);
    bool AllCheckpointsPassed() const;

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::EntityModule> mEntityModule{this};

    static constexpr unsigned int kMaxEntities = 9;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::PickingModule>        mPickingRef{this};
    std::unique_ptr<EntityTestDrawer> mDrawer;
#endif

    // Entity handles
    Dia::Entity::Entity mParentEntity;
    Dia::Entity::Entity mChildA;
    Dia::Entity::Entity mChildB;
    Dia::Entity::Entity mChildC;
    Dia::Entity::Entity mQueryEntities[4];
    Dia::Entity::Entity mDoomedEntity;

    // Flat list for indexed lookup (picking, drawer)
    Dia::Entity::Entity  mAllEntities[kMaxEntities];
    unsigned int         mEntityCount = 0;

    // Lifecycle validation — module observes domain state
    unsigned int mSpawnedEntityCount = 0;
    int          mMailboxReceiveCount = 0;

    // State
    bool mSceneBuilt      = false;
    bool mDoomedDestroyed = false;

    // Checkpoint pass flags
    bool mSpawnComplete     = false;
    bool mQueryCorrect      = false;
    bool mHierarchyValid    = false;
    bool mDestroyCascade    = false;
    bool mMailboxReceived   = false;
    bool mLifecycleComplete = false;

    // Metrics
    Dia::Observation::Metric::Gauge* mMetricAliveCount    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricMailboxCount  = nullptr;
};

} // namespace CluicheTest
