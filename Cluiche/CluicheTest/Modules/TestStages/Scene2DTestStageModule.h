#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include "Modules/Scene2DModule.h"
#include "Modules/EntityModule.h"
#include "Modules/Camera2DModule.h"
#include "Modules/Light2DModule.h"
#include "Modules/TestStages/Entity/TransformComponent.h"

#ifdef DIA_DEBUG
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
    bool ValidateCameras() const;
    bool ValidateLights() const;
    bool ValidateEntities() const;
    bool ValidateLayers() const;

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Scene2DModule>   mSceneRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::EntityModule>    mEntityRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Camera2DModule>  mCameraRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Light2DModule>   mLightRef{this};

    // Metrics
    Dia::Observation::Metric::Gauge* mMetricEntityCount = nullptr;
    Dia::Observation::Metric::Gauge* mMetricLayerCount  = nullptr;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<Scene2DTestDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
