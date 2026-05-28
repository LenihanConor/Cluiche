#pragma once

#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include "Modules/Physics2DModule.h"

#ifdef DIA_DEBUG
#include <DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.h>
#include <DiaRigidBody2DVisualDebugger/VelocityArrowsDrawer.h>
#include <DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.h>
#include <DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.h>
#include <DiaRigidBody2DVisualDebugger/ConstraintLinesDrawer.h>
#include <memory>
#endif

namespace Dia::RigidBody2D { class RigidBody2D; }

namespace CluicheTest {

class RigidBody2DTestModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Validates RigidBody2D: 10 circles settle under gravity";
    explicit RigidBody2DTestModule(const Dia::Core::StringCRC& instanceId);
    ~RigidBody2DTestModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return kBudgetFrames; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool AreDependenciesReady() override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void SetupScene();
    bool AreAllBodiesAsleep() const;
    void EmitMetrics();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Physics2DModule> mPhysics{this};

#ifdef DIA_DEBUG
    std::unique_ptr<Dia::RigidBody2D::PhysicsShapesDrawer>    mShapesDrawer;
    std::unique_ptr<Dia::RigidBody2D::VelocityArrowsDrawer>   mVelocityDrawer;
    std::unique_ptr<Dia::RigidBody2D::ContactNormalsDrawer>   mContactsDrawer;
    std::unique_ptr<Dia::RigidBody2D::PhysicsAABBDrawer>      mAABBDrawer;
    std::unique_ptr<Dia::RigidBody2D::ConstraintLinesDrawer>  mConstraintsDrawer;
#endif

    static constexpr unsigned int kCircleCount = 10;
    Dia::RigidBody2D::RigidBody2D* mCircles[kCircleCount] = {};
    Dia::RigidBody2D::RigidBody2D* mGround = nullptr;

    Dia::Geometry2D::Transform mCircleTransforms[kCircleCount];
    Dia::Geometry2D::Circle mCircleShapes[kCircleCount];
    Dia::Geometry2D::Transform mGroundTransform;
    Dia::Geometry2D::Circle mGroundShape;

    unsigned int mSettleFrame = 0;

    static constexpr unsigned int kBudgetFrames = 900;
};

} // namespace CluicheTest
