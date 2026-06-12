#include "Modules/TestStages/RigidBody2DTestModule.h"

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Constraints/DistanceConstraint.h>
#include <DiaRigidBody2D/Triggers/TriggerVolume2D.h>
#include <DiaRigidBody2D/Events/CollisionEvent.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace CluicheTest {

const Dia::Core::StringCRC RigidBody2DTestModule::kTypeId("RigidBody2DTestModule");

// World gravity is owned by Physics2DModule (-9.81). The scene works in a
// compact coordinate band so it stays inside the shared broadphase bounds.
namespace {
    constexpr float kGroundTopY = 0.0f;
}

RigidBody2DTestModule::RigidBody2DTestModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

RigidBody2DTestModule::~RigidBody2DTestModule() = default;

Dia::Core::StringCRC RigidBody2DTestModule::GetStageName() const
{
    return Dia::Core::StringCRC("RigidBody2DTestStage");
}

const Dia::Core::StringCRC* RigidBody2DTestModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.rigid_body.all_settled"),
        Dia::Core::StringCRC("test.rigid_body.collision_events"),
        Dia::Core::StringCRC("test.rigid_body.trigger_overlap"),
        Dia::Core::StringCRC("test.rigid_body.raycast_hit"),
    };
    outCount = 4;
    return names;
}

bool RigidBody2DTestModule::AreDependenciesReady()
{
    auto* physicsModule = mPhysics.Get();
    return physicsModule && physicsModule->GetWorld();
}

void RigidBody2DTestModule::ObserverNotification(const Dia::Core::ObserverSubject* /*subject*/, int message)
{
    if (static_cast<Dia::RigidBody2D::CollisionEventType>(message) == Dia::RigidBody2D::CollisionEventType::kEnter)
    {
        ++mCollisionEnterCount;
        mCollisionSeen = true;
    }
}

void RigidBody2DTestModule::OnStart(Dia::Automation::AutomationService* service)
{
    SetupScene();

    mPhysics.Get()->GetWorld()->GetCollisionEvents().AttachToObserver(this);

    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricSettleFrame)
        mMetricSettleFrame = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.rigid_body.settle_frame"));
    if (!mMetricCollisions)
        mMetricCollisions = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.rigid_body.collision_enter_count"));

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.rigid_body.all_settled"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool settled = AreAllBodiesAsleep();
            return { settled, settled ? "all dynamic bodies at rest" : "bodies still moving", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.rigid_body.collision_events"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCollisionSeen,
                     mCollisionSeen ? "collision Enter events observed" : "no collision events yet",
                     0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.rigid_body.trigger_overlap"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mTriggerSeen,
                     mTriggerSeen ? "body overlapped trigger volume" : "no trigger overlap yet",
                     0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.rigid_body.raycast_hit"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRaycastHit,
                     mRaycastHit ? "downward raycast hit the ground" : "raycast not yet run",
                     0.0f };
        });

    DIA_LOG_INFO("CluicheTest", "RigidBody2DTestModule::OnStart — scene built, 4 checkpoints registered");
}

void RigidBody2DTestModule::OnUpdate(float /*deltaTime*/)
{
    // Trigger overlap is polled from the world's per-step event list.
    if (!mTriggerSeen)
    {
        const auto& triggerEvents = mPhysics.Get()->GetWorld()->GetLastTriggerEvents();
        if (triggerEvents.Size() > 0)
            mTriggerSeen = true;
    }

    // Run the raycast once, after a few frames so bodies have entered the world.
    if (!mRaycastHit && GetFrameCount() > 3)
        RunRaycast();

    if (!IsResolved() &&
        AreAllBodiesAsleep() && mCollisionSeen && mTriggerSeen && mRaycastHit)
    {
        mSettleFrame = GetFrameCount();
        EmitMetrics();
        ReportPassed();
    }
}

void RigidBody2DTestModule::OnStop()
{
    auto* world = mPhysics.Get() ? mPhysics.Get()->GetWorld() : nullptr;
    if (world)
    {
        world->GetCollisionEvents().DetachFromObserver(this);

        if (mDistanceJoint) world->RemoveConstraint(mDistanceJoint);
        for (unsigned int i = 0; i < kCircleCount; ++i)
            if (mCircles[i]) world->RemoveRigidBody(mCircles[i]);
        if (mThrown)         world->RemoveRigidBody(mThrown);
        if (mPendulumBob)    world->RemoveRigidBody(mPendulumBob);
        if (mPendulumAnchor) world->RemoveRigidBody(mPendulumAnchor);
        if (mGround)         world->RemoveRigidBody(mGround);
        if (mTrigger)        world->RemoveTriggerVolume(mTrigger);
    }

    for (unsigned int i = 0; i < kCircleCount; ++i)
        mCircles[i] = nullptr;
    mGround = nullptr;
    mThrown = nullptr;
    mPendulumAnchor = nullptr;
    mPendulumBob = nullptr;
    mDistanceJoint = nullptr;
    mTrigger = nullptr;

    mCollisionSeen = false;
    mTriggerSeen   = false;
    mRaycastHit    = false;
    mSettleFrame   = 0;
    mCollisionEnterCount = 0;
}

void RigidBody2DTestModule::SetupScene()
{
    auto* world = mPhysics.Get()->GetWorld();

    // --- Flat ConvexPolygon ground: 400 wide, top edge at y = 0 ---
    mGroundTransform.SetLocalPosition(Dia::Maths::Vector2D(500.0f, kGroundTopY));
    {
        const Dia::Maths::Vector2D verts[4] = {
            Dia::Maths::Vector2D(-300.0f, -40.0f), Dia::Maths::Vector2D(300.0f, -40.0f),
            Dia::Maths::Vector2D( 300.0f,   0.0f), Dia::Maths::Vector2D(-300.0f,  0.0f)
        };
        mGroundShape = Dia::Geometry2D::ConvexPolygon(verts, 4);
    }
    Dia::RigidBody2D::RigidBodyDef groundDef;
    groundDef.id          = Dia::Core::StringCRC("ground");
    groundDef.transform   = &mGroundTransform;
    groundDef.polyShape   = &mGroundShape;
    groundDef.type        = Dia::RigidBody2D::BodyType::kStatic;
    groundDef.mass        = 0.0f;
    groundDef.restitution = 0.1f;
    groundDef.friction    = 0.6f;
    mGround = world->AddRigidBody(groundDef);

    // --- Six dynamic circles dropped in a row onto the ground ---
    for (unsigned int i = 0; i < kCircleCount; ++i)
    {
        float x = 350.0f + static_cast<float>(i) * 45.0f;
        float y = 120.0f + static_cast<float>(i) * 25.0f;
        mCircleTransforms[i].SetLocalPosition(Dia::Maths::Vector2D(x, y));
        mCircleShapes[i] = Dia::Geometry2D::Circle(18.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

        Dia::RigidBody2D::RigidBodyDef circleDef;
        circleDef.id            = Dia::Core::StringCRC("circle");
        circleDef.transform     = &mCircleTransforms[i];
        circleDef.circleShape   = &mCircleShapes[i];
        circleDef.type          = Dia::RigidBody2D::BodyType::kDynamic;
        circleDef.mass          = 1.0f;
        circleDef.restitution   = 0.1f;
        circleDef.friction      = 0.6f;
        circleDef.linearDamping = 0.02f;
        circleDef.allowSleeping = true;
        mCircles[i] = world->AddRigidBody(circleDef);
    }

    // --- Thrown body: launched along +x with an impulse; passes through the
    //     trigger and lands on the ground (exercises forces + restitution) ---
    mThrownTransform.SetLocalPosition(Dia::Maths::Vector2D(250.0f, 150.0f));
    mThrownShape = Dia::Geometry2D::Circle(16.0f, Dia::Maths::Vector2D(0.0f, 0.0f));
    Dia::RigidBody2D::RigidBodyDef thrownDef;
    thrownDef.id            = Dia::Core::StringCRC("thrown");
    thrownDef.transform     = &mThrownTransform;
    thrownDef.circleShape   = &mThrownShape;
    thrownDef.type          = Dia::RigidBody2D::BodyType::kDynamic;
    thrownDef.mass          = 1.0f;
    thrownDef.restitution   = 0.2f;
    thrownDef.friction      = 0.6f;
    thrownDef.linearDamping = 0.02f;
    thrownDef.allowSleeping = true;
    mThrown = world->AddRigidBody(thrownDef);
    mThrown->ApplyImpulse(Dia::Maths::Vector2D(120.0f, 0.0f));

    // --- Trigger volume between the thrown body's start and the pile ---
    mTriggerTransform.SetLocalPosition(Dia::Maths::Vector2D(330.0f, 110.0f));
    mTriggerShape = Dia::Geometry2D::Circle(40.0f, Dia::Maths::Vector2D(0.0f, 0.0f));
    Dia::RigidBody2D::TriggerVolumeDef triggerDef;
    triggerDef.id          = Dia::Core::StringCRC("trigger");
    triggerDef.transform   = &mTriggerTransform;
    triggerDef.circleShape = &mTriggerShape;
    mTrigger = world->AddTriggerVolume(triggerDef);

    // --- Distance-constraint pendulum: static anchor + dynamic bob ---
    mAnchorTransform.SetLocalPosition(Dia::Maths::Vector2D(650.0f, 220.0f));
    mAnchorShape = Dia::Geometry2D::Circle(6.0f, Dia::Maths::Vector2D(0.0f, 0.0f));
    Dia::RigidBody2D::RigidBodyDef anchorDef;
    anchorDef.id          = Dia::Core::StringCRC("pendulum_anchor");
    anchorDef.transform   = &mAnchorTransform;
    anchorDef.circleShape = &mAnchorShape;
    anchorDef.type        = Dia::RigidBody2D::BodyType::kStatic;
    anchorDef.mass        = 0.0f;
    mPendulumAnchor = world->AddRigidBody(anchorDef);

    // Bob starts displaced to the side so it swings before coming to rest.
    mBobTransform.SetLocalPosition(Dia::Maths::Vector2D(730.0f, 160.0f));
    mBobShape = Dia::Geometry2D::Circle(16.0f, Dia::Maths::Vector2D(0.0f, 0.0f));
    Dia::RigidBody2D::RigidBodyDef bobDef;
    bobDef.id            = Dia::Core::StringCRC("pendulum_bob");
    bobDef.transform     = &mBobTransform;
    bobDef.circleShape   = &mBobShape;
    bobDef.type          = Dia::RigidBody2D::BodyType::kDynamic;
    bobDef.mass          = 1.0f;
    bobDef.restitution   = 0.1f;
    bobDef.friction      = 0.6f;
    bobDef.linearDamping = 0.05f;
    bobDef.allowSleeping = true;
    mPendulumBob = world->AddRigidBody(bobDef);

    mDistanceJoint = world->AddConstraint(
        new Dia::RigidBody2D::DistanceConstraint(
            mPendulumAnchor, Dia::Maths::Vector2D(0.0f, 0.0f),
            mPendulumBob,    Dia::Maths::Vector2D(0.0f, 0.0f),
            100.0f));
}

void RigidBody2DTestModule::RunRaycast()
{
    // Cast straight down from above the pile; must hit the ground (or a body).
    Dia::Geometry2D::Ray ray(Dia::Maths::Vector2D(500.0f, 300.0f),
                             Dia::Maths::Vector2D(0.0f, -1.0f));
    Dia::RigidBody2D::RaycastHit hit;
    if (mPhysics.Get()->GetWorld()->Raycast(ray, hit))
        mRaycastHit = true;
}

bool RigidBody2DTestModule::AreAllBodiesAsleep() const
{
    for (unsigned int i = 0; i < kCircleCount; ++i)
        if (mCircles[i] && mCircles[i]->IsAwake())
            return false;
    if (mThrown && mThrown->IsAwake())           return false;
    if (mPendulumBob && mPendulumBob->IsAwake()) return false;
    return true;
}

void RigidBody2DTestModule::EmitMetrics()
{
    auto* world = mPhysics.Get()->GetWorld();
    if (mMetricSettleFrame) mMetricSettleFrame->Set(static_cast<double>(mSettleFrame));
    if (mMetricCollisions)  mMetricCollisions->Set(static_cast<double>(mCollisionEnterCount));
    DIA_LOG_INFO("CluicheTest",
        "RigidBody2DTestModule — settled at frame %u, %u collision Enters, step_count %d",
        mSettleFrame, mCollisionEnterCount, world->GetStepCount());
}

} // namespace CluicheTest

namespace { using RigidBody2DTestModule_ = CluicheTest::RigidBody2DTestModule; }
DIA_MODULE(RigidBody2DTestModule_);
DIA_DESCRIBE(RigidBody2DTestModule_::kTypeId,
    "Test stage exercising 2D rigid-body physics: polygon ground, falling circles, a thrown body, "
    "a distance-joint pendulum, a trigger volume, collision events and a raycast.");
