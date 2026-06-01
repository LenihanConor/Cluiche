#include "Modules/TestStages/SoftBody2DTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaSoftBody2D/Rope.h>
#include <DiaSoftBody2D/Cloth.h>
#include <DiaSoftBody2D/Particle.h>

namespace CluicheTest {

const Dia::Core::StringCRC SoftBody2DTestStageModule::kTypeId("SoftBody2DTestStageModule");

SoftBody2DTestStageModule::SoftBody2DTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
    , mAnchorShape(5.0f, Dia::Maths::Vector2D(0.0f, 0.0f))
{}

SoftBody2DTestStageModule::~SoftBody2DTestStageModule() = default;

Dia::Core::StringCRC SoftBody2DTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("SoftBody2DTestStage");
}

const Dia::Core::StringCRC* SoftBody2DTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.soft_body.rope_settled"),
        Dia::Core::StringCRC("test.soft_body.cloth_settled"),
    };
    outCount = 2;
    return names;
}

bool SoftBody2DTestStageModule::AreDependenciesReady()
{
    auto* physicsModule = mPhysics.Get();
    return physicsModule && physicsModule->GetWorld();
}

void SoftBody2DTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    auto* rbWorld = mPhysics.Get()->GetWorld();

    // Create static anchor body for rope top
    mAnchorTransform.SetLocalPosition(Dia::Maths::Vector2D(400.0f, 600.0f));

    Dia::RigidBody2D::RigidBodyDef anchorDef;
    anchorDef.id          = Dia::Core::StringCRC("softbody_rope_anchor");
    anchorDef.transform   = &mAnchorTransform;
    anchorDef.circleShape = &mAnchorShape;
    anchorDef.type        = Dia::RigidBody2D::BodyType::kStatic;
    anchorDef.mass        = 0.0f;
    mRopeAnchor = rbWorld->AddRigidBody(anchorDef);

    // Create soft body world — shares the RB world for anchor coupling
    Dia::SoftBody2D::WorldDef worldDef;
    worldDef.gravity          = { 0.0f, -120.0f };
    worldDef.fixedTimestep    = kFixedDt;
    worldDef.solverIterations = 3;
    worldDef.rigidBodyWorld   = rbWorld;
    mWorld = new Dia::SoftBody2D::SoftBodyWorld(worldDef);

    SetupRope(mRopeAnchor);
    SetupCloth();

#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebugger.Get())
    {
        auto& mgr = vd->GetLayerManager();
        const Dia::Core::StringCRC stageTag("SoftBody2D");

        mParticlesDrawer    = std::make_unique<Dia::SoftBody2D::SoftParticlesDrawer>   (*mWorld, mgr);
        mConstraintsDrawer  = std::make_unique<Dia::SoftBody2D::SoftConstraintsDrawer> (*mWorld, mgr);
        mVelocityDrawer     = std::make_unique<Dia::SoftBody2D::SoftVelocityDrawer>    (*mWorld, mgr);
        mAnchorLinksDrawer  = std::make_unique<Dia::SoftBody2D::SoftAnchorLinksDrawer> (*mWorld, mgr);

        mgr.Register(mParticlesDrawer.get(),   20, stageTag);
        mgr.Register(mConstraintsDrawer.get(), 21, stageTag);
        mgr.Register(mVelocityDrawer.get(),    22, stageTag);
        mgr.Register(mAnchorLinksDrawer.get(), 23, stageTag);
    }
#endif

    // Register metrics
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricRopeFrame)
        mMetricRopeFrame = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.soft_body.rope_settle_frame_count"));
    if (!mMetricClothFrame)
        mMetricClothFrame = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.soft_body.cloth_settle_frame_count"));
    if (!mMetricConstraintIters)
        mMetricConstraintIters = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.soft_body.constraint_iterations"));

    if (mMetricConstraintIters)
        mMetricConstraintIters->Set(static_cast<double>(worldDef.solverIterations));

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.soft_body.rope_settled"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRopeSettled, mRopeSettled ? "12 rope particles at rest" : "rope still moving", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.soft_body.cloth_settled"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mClothSettled, mClothSettled ? "16 cloth particles at rest" : "cloth still moving", 0.0f };
        });

    DIA_LOG_INFO("CluicheTest", "SoftBody2DTestStageModule::OnStart — rope + cloth created, checkpoints registered");
}

void SoftBody2DTestStageModule::SetupRope(Dia::RigidBody2D::RigidBody2D* anchorBody)
{
    Dia::SoftBody2D::RopeDef ropeDef;
    ropeDef.id            = Dia::Core::StringCRC("softbody_rope");
    // End offset gives the rope an initial lateral displacement so it swings before settling
    ropeDef.startPoint    = { 400.0f, 600.0f };
    ropeDef.endPoint      = { 550.0f,  30.0f };
    ropeDef.particleCount = 12;
    ropeDef.mass          = 1.0f;
    ropeDef.stiffness     = 1.0f;
    ropeDef.startAnchor   = anchorBody;
    mRope = mWorld->AddRope(ropeDef);
}

void SoftBody2DTestStageModule::SetupCloth()
{
    Dia::SoftBody2D::ClothDef clothDef;
    clothDef.id                  = Dia::Core::StringCRC("softbody_cloth");
    // Asymmetric origin: cloth hangs from two pinned corners but starts displaced
    // so it has to swing + fold before reaching rest
    clothDef.origin              = { 650.0f, 500.0f };
    clothDef.width               = 200.0f;
    clothDef.height              = 180.0f;
    clothDef.resX                = 4;
    clothDef.resY                = 4;
    clothDef.mass                = 1.0f;
    clothDef.structuralStiffness = 0.6f;
    clothDef.shearStiffness      = 0.3f;
    clothDef.bendStiffness       = 0.1f;
    mCloth = mWorld->AddCloth(clothDef);

    // Pin top-left and top-right corners
    mCloth->PinParticle(0, 0);
    mCloth->PinParticle(3, 0);
}

void SoftBody2DTestStageModule::OnUpdate(float deltaTime)
{
    mWorld->Update(deltaTime);

    if (!mRopeSettled && IsRopeSettled())
    {
        mRopeSettled = true;
        mRopeSettleFrame = GetFrameCount();
        DIA_LOG_INFO("CluicheTest", "SoftBody2DTestStageModule — rope settled at frame %u", mRopeSettleFrame);
    }

    if (!mClothSettled && IsClothSettled())
    {
        mClothSettled = true;
        mClothSettleFrame = GetFrameCount();
        EmitMetrics();
        DIA_LOG_INFO("CluicheTest", "SoftBody2DTestStageModule — cloth settled at frame %u", mClothSettleFrame);
    }

    if (mRopeSettled && mClothSettled && !IsResolved())
        ReportPassed();
}

void SoftBody2DTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebugger.Get())
    {
        auto& mgr = vd->GetLayerManager();
        if (mParticlesDrawer)   { mgr.Unregister(mParticlesDrawer->GetLayerName());   mParticlesDrawer.reset(); }
        if (mConstraintsDrawer) { mgr.Unregister(mConstraintsDrawer->GetLayerName()); mConstraintsDrawer.reset(); }
        if (mVelocityDrawer)    { mgr.Unregister(mVelocityDrawer->GetLayerName());    mVelocityDrawer.reset(); }
        if (mAnchorLinksDrawer) { mgr.Unregister(mAnchorLinksDrawer->GetLayerName()); mAnchorLinksDrawer.reset(); }
    }
#endif

    delete mWorld;
    mWorld = nullptr;
    mRope  = nullptr;
    mCloth = nullptr;

    if (mRopeAnchor && mPhysics.Get() && mPhysics.Get()->GetWorld())
        mPhysics.Get()->GetWorld()->RemoveRigidBody(mRopeAnchor);
    mRopeAnchor = nullptr;

    mRopeSettled   = false;
    mClothSettled  = false;
    mRopeSettleFrame  = 0;
    mClothSettleFrame = 0;

    DIA_LOG_INFO("CluicheTest", "SoftBody2DTestStageModule::OnStop — worlds destroyed");
}

bool SoftBody2DTestStageModule::IsRopeSettled() const
{
    if (!mRope) return false;
    for (int i = 0; i < mRope->GetParticleCount(); ++i)
    {
        const auto vel = Dia::SoftBody2D::DeriveVelocity(mRope->GetParticle(i), kFixedDt);
        if (vel.SquareMagnitude() > kVelocityEpsilon * kVelocityEpsilon)
            return false;
    }
    return true;
}

bool SoftBody2DTestStageModule::IsClothSettled() const
{
    if (!mCloth) return false;
    for (int y = 0; y < mCloth->GetResY(); ++y)
    {
        for (int x = 0; x < mCloth->GetResX(); ++x)
        {
            const auto vel = Dia::SoftBody2D::DeriveVelocity(mCloth->GetParticle(x, y), kFixedDt);
            if (vel.SquareMagnitude() > kVelocityEpsilon * kVelocityEpsilon)
                return false;
        }
    }
    return true;
}

void SoftBody2DTestStageModule::EmitMetrics()
{
    if (mMetricRopeFrame)
        mMetricRopeFrame->Set(static_cast<double>(mRopeSettleFrame));
    if (mMetricClothFrame)
        mMetricClothFrame->Set(static_cast<double>(mClothSettleFrame));
}

} // namespace CluicheTest

namespace { using SoftBody2DTestStageModule_ = CluicheTest::SoftBody2DTestStageModule; }
DIA_MODULE(SoftBody2DTestStageModule_);
