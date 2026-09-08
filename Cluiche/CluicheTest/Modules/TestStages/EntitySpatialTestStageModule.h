#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include <DiaEntitySpatial/Adaptors/EntitySpatialOverlay.h>
#endif

namespace Dia::Observation::Metric { class Gauge; }

namespace CluicheTest {

class EntitySpatialTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Entity spatial queries: 5 query shapes, layer masks, dirty-flag re-index, destroy sweep";
    static constexpr unsigned int kMinDisplayFrames = 210;

    explicit EntitySpatialTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~EntitySpatialTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 600; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void SpawnAgents();
    void MoveAgents();
    void RunQueries();
    void UpdateMetrics();
    void CheckCheckpoints();
    void RegisterCheckpoints(Dia::Automation::AutomationService* service);
    void RegisterMetrics();

    Dia::Entity::Domain mDomain;
    std::unique_ptr<Dia::EntitySpatial::EntitySpatialModule> mSpatialModule;

    static constexpr int kAgentCount = 32;
    Dia::Entity::Entity mAgents[kAgentCount];

    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mCircleOut;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mRegionOut;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 8>  mKNearestOut;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mRayOut;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mSectorOut;

    Dia::Observation::Metric::Gauge* mMetricCircleHits    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricRegionHits    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricKNearestCount = nullptr;
    Dia::Observation::Metric::Gauge* mMetricRayHits       = nullptr;
    Dia::Observation::Metric::Gauge* mMetricSectorHits    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricActiveCount   = nullptr;

    bool mIndexInitialized  = false;
    bool mCircleQueryLive   = false;
    bool mSectorQueryLive   = false;
    bool mKNearestExact     = false;
    bool mLayerMaskFilter   = false;
    bool mDestroySweep      = false;
    bool mRayQueryLive      = false;
    bool mAllQueriesStable  = false;
    bool mAllPassed         = false;

    uint32_t mQueryMask          = 0x0Fu;
    int      mStableFrameCount   = 0;
    bool     mDestroyFired       = false;
    bool     mLayerMaskReduced   = false;
    int      mBaselineCircleHits = 0;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};

    std::unique_ptr<Dia::EntitySpatial::Adaptors::EntitySpatialGridOverlay>   mGridOverlay;
    std::unique_ptr<Dia::EntitySpatial::Adaptors::EntitySpatialEntityOverlay> mEntityOverlay;
    std::unique_ptr<Dia::EntitySpatial::Adaptors::EntitySpatialQueryOverlay>  mQueryOverlay;
#endif
};

} // namespace CluicheTest
