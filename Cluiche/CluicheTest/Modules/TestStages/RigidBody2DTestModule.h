#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include "Modules/AutomationModule.h"

namespace Dia::RigidBody2D { class PhysicsWorld; class RigidBody2D; }

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

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::AutomationModule> mAutomation{this};

    Dia::RigidBody2D::PhysicsWorld* mWorld = nullptr;

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
