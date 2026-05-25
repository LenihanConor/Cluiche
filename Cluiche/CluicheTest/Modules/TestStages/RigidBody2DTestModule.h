#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include "Modules/AutomationModule.h"
#include "Modules/Physics2DModule.h"

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include <DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.h>
#include <DiaRigidBody2DVisualDebugger/VelocityArrowsDrawer.h>
#include <DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.h>
#include <DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.h>
#include <DiaRigidBody2DVisualDebugger/ConstraintLinesDrawer.h>
#include <memory>
#endif

namespace Dia::RigidBody2D { class RigidBody2D; }

namespace CluicheTest {

class RigidBody2DTestModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit RigidBody2DTestModule(const Dia::Core::StringCRC& instanceId);
    ~RigidBody2DTestModule() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void SetupScene();
    void RegisterCheckpoints();
    bool AreAllBodiesAsleep() const;
    void EmitMetrics();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::AutomationModule>       mAutomation{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Physics2DModule>         mPhysics{this};

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule>    mVisualDebugger{this};
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

    unsigned int mFrameCount = 0;
    unsigned int mSettleFrame = 0;
    bool mSettled = false;

    static constexpr unsigned int kBudgetFrames = 200;
};

} // namespace CluicheTest
