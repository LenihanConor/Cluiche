#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include "Modules/TestStages/Entity/TransformComponent.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaScene2D/SceneLoader2D.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>

#ifdef DIA_DEBUG
#include <DiaApplicationFlow/ModuleRefV2.h>
#include "Modules/VisualDebuggerModule.h"
#include <memory>
#endif

namespace CluicheTest {

#ifdef DIA_DEBUG
class Scene2DTestDrawer;
#endif

class Scene2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Validates DiaScene2D: .diascene load, registry hydration, entity spawn";
    explicit Scene2DTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 60; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void LoadScene();
    bool ValidateCameras() const;
    bool ValidateLights() const;
    bool ValidateEntities() const;
    bool ValidateLayers() const;

    Dia::Scene2D::SceneLoader2D          mSceneLoader;
    Dia::Scene2D::LayerTable             mLayerTable;
    Dia::Camera2D::CameraRegistry2D      mCameraRegistry;
    Dia::Lighting2D::LightRegistry2D     mLightRegistry;
    Dia::Entity::Domain                  mEntityDomain;

    bool mLoadSucceeded = false;

    // Metrics
    Dia::Observation::Metric::Gauge* mMetricEntityCount = nullptr;
    Dia::Observation::Metric::Gauge* mMetricLayerCount  = nullptr;
    Dia::Observation::Metric::Gauge* mMetricLoadTimeMs  = nullptr;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<Scene2DTestDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
