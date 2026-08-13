#include "Modules/TestStages/DebugGalleryTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

#ifdef DIA_DEBUG
// Domain headers
#include <DiaGeometry2DVisualDebugger/Geometry2DDebugDomain.h>
#include <DiaAssetRuntimeVisualDebugger/AssetRuntimeDebugDomain.h>
#include <DiaUtilityAIVisualDebugger/UtilityAIDebugDomain.h>
#include <DiaRigidBody2DVisualDebugger/RigidBody2DDebugDomain.h>
#include <DiaSoftBody2DVisualDebugger/SoftBody2DDebugDomain.h>
#include <DiaLighting3DVisualDebugger/Lighting3DDebugDomain.h>
#include <DiaIK2DVisualDebugger/IK2DDebugDomain.h>
#include <DiaRig2DVisualDebugger/Rig2DDebugDomain.h>
#include <DiaAnimation2DVisualDebugger/Animation2DDebugDomain.h>
#include <DiaScene2DVisualDebugger/Scene2DDebugDomain.h>
#include <DiaEntityVisualDebugger/EntityDebugDomain.h>
#include <DiaMesh3DVisualDebugger/Mesh3DDebugDomain.h>
// Fixture headers
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/World/WorldDef.h>
#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaEntity/Domain.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#endif

namespace CluicheTest {

const Dia::Core::StringCRC DebugGalleryTestStageModule::kTypeId("DebugGalleryTestStageModule");

DebugGalleryTestStageModule::DebugGalleryTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

DebugGalleryTestStageModule::~DebugGalleryTestStageModule() = default;

Dia::Core::StringCRC DebugGalleryTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("DebugGalleryTestStage");
}

const Dia::Core::StringCRC* DebugGalleryTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.debuggallery.passed")
    };
    outCount = 1;
    return names;
}

void DebugGalleryTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.debuggallery.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            const bool passed = GetFrameCount() >= 300;
            return { passed, passed ? "300 frames elapsed" : "pending", 0.0f };
        });

#ifdef DIA_DEBUG
    auto* vd = mVisualDebuggerRef.Get();
    if (!vd)
    {
        DIA_LOG_WARNING("CluicheTest", "DebugGalleryTestStageModule — VisualDebuggerModule unavailable, skipping domain registration");
        return;
    }

    // Build shared rig — 5-bone straight-line limb used by IK2D, Rig2D, and Animation2D domains.
    {
        auto skelDef   = Dia::IK2D::Testing::MakeLimbSkeletonDef(5, 0.8f);
        mSkeleton      = std::make_unique<Dia::Rig2D::Skeleton>(*skelDef);
        mPose          = std::make_unique<Dia::Rig2D::Pose>(*mSkeleton);
        mPose->SetToBindPose(*mSkeleton);
        mIKSolver      = std::make_unique<Dia::IK2D::IKSolver>(*mSkeleton, *mPose);
        mIKSolver->SetRootTransform(Dia::Rig2D::BoneTransform{});
        mAnimEvaluator = std::make_unique<Dia::Animation2D::AnimationEvaluator>(*mSkeleton);
    }

    // Physics worlds
    {
        Dia::RigidBody2D::WorldDef def;
        mPhysicsWorld = std::make_unique<Dia::RigidBody2D::PhysicsWorld>(def);
    }
    {
        Dia::SoftBody2D::WorldDef def;
        mSoftBodyWorld = std::make_unique<Dia::SoftBody2D::SoftBodyWorld>(def);
    }

    // Remaining fixtures — all default-constructible
    mUtilitySet       = std::make_unique<Dia::UtilityAI::UtilitySet>();
    mLightRegistry3D  = std::make_unique<Dia::Lighting3D::LightRegistry3D>();
    mCameraRegistry   = std::make_unique<Dia::Camera2D::CameraRegistry2D>();
    mLightRegistry2D  = std::make_unique<Dia::Lighting2D::LightRegistry2D>();
    mLayerTable       = std::make_unique<Dia::Scene2D::LayerTable>();
    mEntityDomain     = std::make_unique<Dia::Entity::Domain>();
    mMeshFrameData    = std::make_unique<Dia::Graphics3D::Mesh3DFrameData>();
    mMeshAssetHandler = std::make_unique<Dia::Mesh3D::Mesh3DAssetHandler>();

    // Construct all 12 gallery domains
    mGeometry2DDomain   = std::make_unique<Dia::Geometry2DVisualDebugger::Geometry2DDebugDomain>();
    mAssetRuntimeDomain = std::make_unique<Dia::AssetRuntime::AssetRuntimeDebugDomain>();
    mUtilityAIDomain    = std::make_unique<Dia::UtilityAI::UtilityAIDebugDomain>(*mUtilitySet);
    mRigidBody2DDomain  = std::make_unique<Dia::RigidBody2D::RigidBody2DDebugDomain>(*mPhysicsWorld);
    mSoftBody2DDomain   = std::make_unique<Dia::SoftBody2D::SoftBody2DDebugDomain>(*mSoftBodyWorld);
    mLighting3DDomain   = std::make_unique<Dia::Lighting3D::Lighting3DDebugDomain>(*mLightRegistry3D);
    mIK2DDomain         = std::make_unique<Dia::IK2D::IK2DDebugDomain>(*mIKSolver, *mSkeleton);
    mRig2DDomain        = std::make_unique<Dia::Rig2D::Rig2DDebugDomain>(*mSkeleton, mIKSolver->GetWorldTransforms());
    mAnimation2DDomain  = std::make_unique<Dia::Animation2D::Animation2DDebugDomain>(
                              *mAnimEvaluator, *mSkeleton, mIKSolver->GetWorldTransforms());
    mScene2DDomain      = std::make_unique<Dia::Scene2DVisualDebugger::Scene2DDebugDomain>(
                              *mCameraRegistry, *mLightRegistry2D, *mLayerTable);
    mEntityDebugDomain  = std::make_unique<Dia::EntityVisualDebugger::EntityDebugDomain>(
                              *mEntityDomain, *mEntityDomain, Dia::Core::StringCRC("TransformComponent"));
    mMesh3DDomain       = std::make_unique<Dia::Mesh3D::Mesh3DDebugDomain>(*mMeshFrameData, *mMeshAssetHandler);

    DIA_LOG_INFO("CluicheTest", "DebugGalleryTestStageModule — fixtures built; domains register on first update");
#endif
}

void DebugGalleryTestStageModule::OnUpdate(float /*deltaTime*/)
{
#ifdef DIA_DEBUG
    if (!mDomainsRegistered)
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            vd->RegisterDomain(*mGeometry2DDomain);
            vd->RegisterDomain(*mAssetRuntimeDomain);
            vd->RegisterDomain(*mUtilityAIDomain);
            vd->RegisterDomain(*mRigidBody2DDomain);
            vd->RegisterDomain(*mSoftBody2DDomain);
            vd->RegisterDomain(*mLighting3DDomain);
            vd->RegisterDomain(*mIK2DDomain);
            vd->RegisterDomain(*mRig2DDomain);
            vd->RegisterDomain(*mAnimation2DDomain);
            vd->RegisterDomain(*mScene2DDomain);
            vd->RegisterDomain(*mEntityDebugDomain);
            vd->RegisterDomain(*mMesh3DDomain);
            mDomainsRegistered = true;
            DIA_LOG_INFO("CluicheTest", "DebugGalleryTestStageModule — 12 domains registered");
        }
    }
#endif

    if (!IsResolved() && GetFrameCount() >= 300)
        ReportPassed();
}

void DebugGalleryTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    auto* vd = mVisualDebuggerRef.Get();
    if (vd)
    {
        if (mMesh3DDomain)       vd->UnregisterDomain(*mMesh3DDomain);
        if (mEntityDebugDomain)  vd->UnregisterDomain(*mEntityDebugDomain);
        if (mScene2DDomain)      vd->UnregisterDomain(*mScene2DDomain);
        if (mAnimation2DDomain)  vd->UnregisterDomain(*mAnimation2DDomain);
        if (mRig2DDomain)        vd->UnregisterDomain(*mRig2DDomain);
        if (mIK2DDomain)         vd->UnregisterDomain(*mIK2DDomain);
        if (mLighting3DDomain)   vd->UnregisterDomain(*mLighting3DDomain);
        if (mSoftBody2DDomain)   vd->UnregisterDomain(*mSoftBody2DDomain);
        if (mRigidBody2DDomain)  vd->UnregisterDomain(*mRigidBody2DDomain);
        if (mUtilityAIDomain)    vd->UnregisterDomain(*mUtilityAIDomain);
        if (mAssetRuntimeDomain) vd->UnregisterDomain(*mAssetRuntimeDomain);
        if (mGeometry2DDomain)   vd->UnregisterDomain(*mGeometry2DDomain);
    }

    // Domains first, then the fixtures they reference.
    mMesh3DDomain.reset();
    mEntityDebugDomain.reset();
    mScene2DDomain.reset();
    mAnimation2DDomain.reset();
    mRig2DDomain.reset();
    mIK2DDomain.reset();
    mLighting3DDomain.reset();
    mSoftBody2DDomain.reset();
    mRigidBody2DDomain.reset();
    mUtilityAIDomain.reset();
    mAssetRuntimeDomain.reset();
    mGeometry2DDomain.reset();

    mMeshAssetHandler.reset();
    mMeshFrameData.reset();
    mEntityDomain.reset();
    mLayerTable.reset();
    mLightRegistry2D.reset();
    mCameraRegistry.reset();
    mAnimEvaluator.reset();
    mIKSolver.reset();
    mPose.reset();
    mSkeleton.reset();
    mLightRegistry3D.reset();
    mUtilitySet.reset();
    mSoftBodyWorld.reset();
    mPhysicsWorld.reset();

    mDomainsRegistered = false;
    DIA_LOG_INFO("CluicheTest", "DebugGalleryTestStageModule — 12 domains unregistered");
#endif
}

} // namespace CluicheTest

namespace { using DebugGalleryTestStageModule_ = CluicheTest::DebugGalleryTestStageModule; }
DIA_MODULE(DebugGalleryTestStageModule_);
DIA_DESCRIBE(DebugGalleryTestStageModule_::kTypeId,
    "Visual gallery stage: registers all 14 IDebugDomain instances simultaneously for DiaDebugPanel validation.");
