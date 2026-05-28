#include "Modules/TestStages/RigidBody2DTestModule.h"

#include "Modules/VisualDebuggerModule.h"
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC RigidBody2DTestModule::kTypeId("RigidBody2DTestModule");

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
        Dia::Core::StringCRC("test.rigid_body.all_settled")
    };
    outCount = 1;
    return names;
}

bool RigidBody2DTestModule::AreDependenciesReady()
{
    auto* physicsModule = mPhysics.Get();
    return physicsModule && physicsModule->GetWorld();
}

void RigidBody2DTestModule::OnStart(Dia::Automation::AutomationService* service)
{
    SetupScene();

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.rigid_body.all_settled"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool settled = AreAllBodiesAsleep();
            return { settled,
                     settled ? "all 10 bodies at rest" : "bodies still moving",
                     0.0f };
        });
}

void RigidBody2DTestModule::OnUpdate(float /*deltaTime*/)
{
#ifdef DIA_DEBUG
    if (!mShapesDrawer)
    {
        if (auto* mgr = Cluiche::AppFlow::VisualDebuggerModule::GetStaticLayerManager())
        {
            auto* world = mPhysics.Get()->GetWorld();

            mShapesDrawer     = std::make_unique<Dia::RigidBody2D::PhysicsShapesDrawer>(*world, *mgr);
            mVelocityDrawer   = std::make_unique<Dia::RigidBody2D::VelocityArrowsDrawer>(*world, *mgr);
            mContactsDrawer   = std::make_unique<Dia::RigidBody2D::ContactNormalsDrawer>(*world, *mgr);
            mAABBDrawer       = std::make_unique<Dia::RigidBody2D::PhysicsAABBDrawer>(*world, *mgr);
            mConstraintsDrawer = std::make_unique<Dia::RigidBody2D::ConstraintLinesDrawer>(*world, *mgr);

            const Dia::Core::StringCRC stageTag("RigidBody2DTestStage");
            mgr->Register(mShapesDrawer.get(),      10, stageTag);
            mgr->Register(mVelocityDrawer.get(),    11, stageTag);
            mgr->Register(mContactsDrawer.get(),    12, stageTag);
            mgr->Register(mAABBDrawer.get(),        13, stageTag);
            mgr->Register(mConstraintsDrawer.get(), 14, stageTag);
        }
    }
#endif

    if (!IsResolved() && AreAllBodiesAsleep())
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
        for (unsigned int i = 0; i < kCircleCount; ++i)
        {
            if (mCircles[i])
                world->RemoveRigidBody(mCircles[i]);
        }
        if (mGround)
            world->RemoveRigidBody(mGround);
    }

    for (unsigned int i = 0; i < kCircleCount; ++i)
        mCircles[i] = nullptr;
    mGround = nullptr;

    mSettleFrame = 0;
}

void RigidBody2DTestModule::SetupScene()
{
    auto* world = mPhysics.Get()->GetWorld();

    world->SetGravity(Dia::Maths::Vector2D(0.0f, -120.0f));

    mGroundTransform.SetLocalPosition(Dia::Maths::Vector2D(700.0f, -4800.0f));
    mGroundShape = Dia::Geometry2D::Circle(5000.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    Dia::RigidBody2D::RigidBodyDef groundDef;
    groundDef.id = Dia::Core::StringCRC("ground");
    groundDef.transform = &mGroundTransform;
    groundDef.circleShape = &mGroundShape;
    groundDef.type = Dia::RigidBody2D::BodyType::kStatic;
    groundDef.mass = 0.0f;
    groundDef.restitution = 0.3f;
    groundDef.friction = 0.5f;
    mGround = world->AddRigidBody(groundDef);

    for (unsigned int i = 0; i < kCircleCount; ++i)
    {
        float col = 400.0f + static_cast<float>(i % 5) * 120.0f;
        float row = (i < 5) ? 600.0f : 750.0f;

        mCircleTransforms[i].SetLocalPosition(Dia::Maths::Vector2D(col, row));
        mCircleShapes[i] = Dia::Geometry2D::Circle(30.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

        Dia::RigidBody2D::RigidBodyDef circleDef;
        circleDef.id = Dia::Core::StringCRC("circle");
        circleDef.transform = &mCircleTransforms[i];
        circleDef.circleShape = &mCircleShapes[i];
        circleDef.type = Dia::RigidBody2D::BodyType::kDynamic;
        circleDef.mass = 1.0f;
        circleDef.restitution = 0.3f;
        circleDef.friction = 0.5f;
        circleDef.linearDamping = 0.01f;
        circleDef.allowSleeping = true;

        mCircles[i] = world->AddRigidBody(circleDef);
    }
}

bool RigidBody2DTestModule::AreAllBodiesAsleep() const
{
    for (unsigned int i = 0; i < kCircleCount; ++i)
    {
        if (mCircles[i] && mCircles[i]->IsAwake())
            return false;
    }
    return true;
}

void RigidBody2DTestModule::EmitMetrics()
{
    auto* world = mPhysics.Get()->GetWorld();
    DIA_LOG_INFO("CluicheTest", "RigidBody2DTestModule — settled at frame %u, step_count %d",
        mSettleFrame, world->GetStepCount());
}

} // namespace CluicheTest

namespace { using RigidBody2DTestModule_ = CluicheTest::RigidBody2DTestModule; }
DIA_MODULE(RigidBody2DTestModule_);
