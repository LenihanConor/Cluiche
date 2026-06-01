#pragma once

#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include "Modules/Physics2DModule.h"

namespace Dia::RigidBody2D { class RigidBody2D; }

namespace CluicheTest {

class RigidBody2DTestModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
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
