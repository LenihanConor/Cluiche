#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Transform/Transform.h>

namespace Dia::RigidBody2D { class RigidBody2D; }
namespace Dia::SoftBody2D  { class SoftBodyWorld; class Rope; class Cloth; }
namespace Dia::Observation::Metric { class Gauge; }

#include "Modules/Physics2DModule.h"

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include <DiaSoftBody2DVisualDebugger/SoftParticlesDrawer.h>
#include <DiaSoftBody2DVisualDebugger/SoftConstraintsDrawer.h>
#include <DiaSoftBody2DVisualDebugger/SoftVelocityDrawer.h>
#include <DiaSoftBody2DVisualDebugger/SoftAnchorLinksDrawer.h>
#include <memory>
#endif

namespace CluicheTest {

class SoftBody2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Validates DiaSoftBody2D: rope + cloth settle under gravity with rigid body anchor";

    explicit SoftBody2DTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~SoftBody2DTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return kBudgetFrames; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;

    bool AreDependenciesReady() override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void SetupRope(Dia::RigidBody2D::RigidBody2D* anchorBody);
    void SetupCloth();
    bool IsRopeSettled() const;
    bool IsClothSettled() const;
    void EmitMetrics();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Physics2DModule> mPhysics{this};

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebugger{this};

    std::unique_ptr<Dia::SoftBody2D::SoftParticlesDrawer>    mParticlesDrawer;
    std::unique_ptr<Dia::SoftBody2D::SoftConstraintsDrawer>  mConstraintsDrawer;
    std::unique_ptr<Dia::SoftBody2D::SoftVelocityDrawer>     mVelocityDrawer;
    std::unique_ptr<Dia::SoftBody2D::SoftAnchorLinksDrawer>  mAnchorLinksDrawer;
#endif

    Dia::SoftBody2D::SoftBodyWorld* mWorld = nullptr;
    Dia::SoftBody2D::Rope*          mRope  = nullptr;
    Dia::SoftBody2D::Cloth*         mCloth = nullptr;

    // Static RB anchor for the rope — owned values, pointer registered with PhysicsWorld
    Dia::Geometry2D::Transform      mAnchorTransform;
    Dia::Geometry2D::Circle         mAnchorShape;
    Dia::RigidBody2D::RigidBody2D*  mRopeAnchor = nullptr;

    unsigned int mRopeSettleFrame = 0;
    unsigned int mClothSettleFrame = 0;
    bool mRopeSettled  = false;
    bool mClothSettled = false;

    Dia::Observation::Metric::Gauge* mMetricRopeFrame      = nullptr;
    Dia::Observation::Metric::Gauge* mMetricClothFrame     = nullptr;
    Dia::Observation::Metric::Gauge* mMetricConstraintIters = nullptr;

    static constexpr float        kVelocityEpsilon = 0.001f;
    static constexpr float        kFixedDt         = 1.0f / 30.0f;
    static constexpr unsigned int kBudgetFrames    = 1200;
};

} // namespace CluicheTest
