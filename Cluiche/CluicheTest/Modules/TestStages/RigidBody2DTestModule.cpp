#include "Modules/TestStages/RigidBody2DTestModule.h"
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>

namespace CluicheTest {

const Dia::Core::StringCRC RigidBody2DTestModule::kTypeId("RigidBody2DTestModule");

RigidBody2DTestModule::RigidBody2DTestModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

RigidBody2DTestModule::~RigidBody2DTestModule() = default;

Dia::ApplicationFlow::StartResult RigidBody2DTestModule::DoStart()
{
    DIA_LOG_INFO("CluicheTest", "RigidBody2DTestModule::DoStart entry");

    auto* automationModule = mAutomation.Get();
    if (!automationModule || !automationModule->GetService())
        return Dia::ApplicationFlow::StartResult::kLoading;

    SetupScene();
    RegisterCheckpoints();

    TestResultsRegistry::GetInstance().SetRunning(
        Dia::Core::StringCRC("RigidBody2DStage"), kBudgetFrames);

    DIA_LOG_INFO("CluicheTest", "RigidBody2DTestModule::DoStart — 10 circles + ground, checkpoint registered");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void RigidBody2DTestModule::DoUpdate(float deltaTime)
{
    mWorld->Update(deltaTime);
    ++mFrameCount;

    TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);

    if (!mSettled)
    {
        if (AreAllBodiesAsleep())
        {
            mSettled = true;
            mSettleFrame = mFrameCount;
            EmitMetrics();
            TestResultsRegistry::GetInstance().SetPassed(
                Dia::Core::StringCRC("RigidBody2DStage"), mFrameCount);
        }
        else if (mFrameCount >= kBudgetFrames)
        {
            mSettled = true;
            TestResultsRegistry::GetInstance().SetTimeout(
                Dia::Core::StringCRC("RigidBody2DStage"));
        }
    }
}

Dia::ApplicationFlow::StopResult RigidBody2DTestModule::DoStop()
{
    DIA_LOG_INFO("CluicheTest", "RigidBody2DTestModule::DoStop entry");

    if (auto* automationModule = mAutomation.Get())
    {
        if (auto* service = automationModule->GetService())
            service->UnregisterCheckpoints(this);
    }

    if (mWorld)
    {
        delete mWorld;
        mWorld = nullptr;
    }

    for (unsigned int i = 0; i < kCircleCount; ++i)
        mCircles[i] = nullptr;
    mGround = nullptr;

    mFrameCount = 0;
    mSettleFrame = 0;
    mSettled = false;

    DIA_LOG_INFO("CluicheTest", "RigidBody2DTestModule::DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void RigidBody2DTestModule::SetupScene()
{
    Dia::RigidBody2D::WorldDef worldDef;
    worldDef.gravity = Dia::Maths::Vector2D(0.0f, -9.81f);
    worldDef.fixedTimestep = 1.0f / 30.0f;
    worldDef.maxSubSteps = 4;
    worldDef.broadPhase = nullptr;

    mWorld = new Dia::RigidBody2D::PhysicsWorld(worldDef);

    // Ground: static circle at y=-10 with large radius acting as floor
    mGroundTransform.SetLocalPosition(Dia::Maths::Vector2D(3.0f, -10.0f));
    mGroundShape = Dia::Geometry2D::Circle(10.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    Dia::RigidBody2D::RigidBodyDef groundDef;
    groundDef.id = Dia::Core::StringCRC("ground");
    groundDef.transform = &mGroundTransform;
    groundDef.circleShape = &mGroundShape;
    groundDef.type = Dia::RigidBody2D::BodyType::kStatic;
    groundDef.mass = 0.0f;
    groundDef.restitution = 0.3f;
    groundDef.friction = 0.5f;
    mGround = mWorld->AddRigidBody(groundDef);

    // 10 circles: 2 rows of 5, positions at y=5 and y=8
    for (unsigned int i = 0; i < kCircleCount; ++i)
    {
        float col = static_cast<float>(i % 5) + 1.0f;
        float row = (i < 5) ? 5.0f : 8.0f;

        mCircleTransforms[i].SetLocalPosition(Dia::Maths::Vector2D(col, row));
        mCircleShapes[i] = Dia::Geometry2D::Circle(0.5f, Dia::Maths::Vector2D(0.0f, 0.0f));

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

        mCircles[i] = mWorld->AddRigidBody(circleDef);
    }
}

void RigidBody2DTestModule::RegisterCheckpoints()
{
    auto* service = mAutomation.Get()->GetService();

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("rigid_body.all_settled"),
        [this]() -> Dia::Automation::CheckpointResult {
            return {
                mSettled,
                mSettled ? "all 10 bodies at rest" : "bodies still moving",
                0.0f
            };
        });
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
    DIA_LOG_INFO("CluicheTest", "RigidBody2DTestModule — settled at frame %u, step_count %d",
        mSettleFrame, mWorld->GetStepCount());
}

} // namespace CluicheTest

namespace { using RigidBody2DTestModule_ = CluicheTest::RigidBody2DTestModule; }
DIA_MODULE(RigidBody2DTestModule_);
