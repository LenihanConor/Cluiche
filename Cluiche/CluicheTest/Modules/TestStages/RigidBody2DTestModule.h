#pragma once

#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Architecture/Observer.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include "Modules/Physics2DModule.h"

namespace Dia::RigidBody2D { class RigidBody2D; class TriggerVolume2D; class IConstraint; }
namespace Dia::Observation::Metric { class Gauge; }

namespace CluicheTest {

// Integration test stage for DiaRigidBody2D. Unlike the original "drop 10
// circles" smoke test, this exercises the breadth of the module so the five
// visual-debugger drawers all light up and the major code paths are covered:
//   * a flat ConvexPolygon ground (polygon collision)
//   * dynamic circles that fall and settle (circle-vs-poly + sleeping)
//   * a thrown circle given an initial impulse (forces / restitution)
//   * a two-body distance-constraint pendulum (constraint solver)
//   * a trigger volume the thrown body passes through (trigger overlaps)
//   * a downward raycast that must hit the ground (spatial queries)
//   * a collision-event observer that must see at least one Enter (events)
//
// Passes when every dynamic body has come to rest AND the collision-event,
// trigger, and raycast checkpoints have all been satisfied.
class RigidBody2DTestModule : public TestStageModuleBase,
                              public Dia::Core::Observer
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "Validates RigidBody2D: polygon ground, circles, a joint, a trigger, collision events and a raycast";
    explicit RigidBody2DTestModule(const Dia::Core::StringCRC& instanceId);
    ~RigidBody2DTestModule() override;

    // Observer — receives collision events from PhysicsWorld.
    void ObserverNotification(const Dia::Core::ObserverSubject* subject, int message) override;

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
    void RunRaycast();
    void EmitMetrics();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Physics2DModule> mPhysics{this};

    static constexpr unsigned int kCircleCount = 6;
    Dia::RigidBody2D::RigidBody2D* mCircles[kCircleCount] = {};
    Dia::RigidBody2D::RigidBody2D* mGround = nullptr;

    // Thrown body + pendulum pair
    Dia::RigidBody2D::RigidBody2D* mThrown      = nullptr;
    Dia::RigidBody2D::RigidBody2D* mPendulumAnchor = nullptr;
    Dia::RigidBody2D::RigidBody2D* mPendulumBob    = nullptr;
    Dia::RigidBody2D::IConstraint* mDistanceJoint  = nullptr;
    Dia::RigidBody2D::TriggerVolume2D* mTrigger     = nullptr;

    // Owned shape/transform storage — pointers handed to the world.
    Dia::Geometry2D::Transform     mCircleTransforms[kCircleCount];
    Dia::Geometry2D::Circle        mCircleShapes[kCircleCount];
    Dia::Geometry2D::Transform     mGroundTransform;
    Dia::Geometry2D::ConvexPolygon mGroundShape;
    Dia::Geometry2D::Transform     mThrownTransform;
    Dia::Geometry2D::Circle        mThrownShape;
    Dia::Geometry2D::Transform     mAnchorTransform;
    Dia::Geometry2D::Circle        mAnchorShape;
    Dia::Geometry2D::Transform     mBobTransform;
    Dia::Geometry2D::Circle        mBobShape;
    Dia::Geometry2D::Transform     mTriggerTransform;
    Dia::Geometry2D::Circle        mTriggerShape;

    // Checkpoint state
    bool         mCollisionSeen = false;
    bool         mTriggerSeen   = false;
    bool         mRaycastHit    = false;
    unsigned int mSettleFrame   = 0;

    Dia::Observation::Metric::Gauge* mMetricSettleFrame = nullptr;
    Dia::Observation::Metric::Gauge* mMetricCollisions  = nullptr;
    unsigned int mCollisionEnterCount = 0;

    static constexpr unsigned int kBudgetFrames = 1800;
};

} // namespace CluicheTest
