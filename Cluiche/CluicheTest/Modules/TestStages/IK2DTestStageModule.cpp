#include "Modules/TestStages/IK2DTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/SerializedFileLoad.h>
#include <DiaCore/Reflect/JsonArchive.h>
#include <DiaRig2D/DiaRig2DSerializers.h>
#include <DiaIK2D/IKChainDef.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <cmath>

namespace CluicheTest {

const Dia::Core::StringCRC IK2DTestStageModule::kTypeId("IK2DTestStageModule");

IK2DTestStageModule::IK2DTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

IK2DTestStageModule::~IK2DTestStageModule() = default;

Dia::Core::StringCRC IK2DTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("IK2DTestStage");
}

const Dia::Core::StringCRC* IK2DTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("ik2d.two_bone_converged"),
        Dia::Core::StringCRC("ik2d.fabrik_converged"),
        Dia::Core::StringCRC("ik2d.look_at_accurate"),
    };
    outCount = 3;
    return names;
}

void IK2DTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // Phases 1+2: procedural straight-line 5-bone limb (consecutive indices,
    // compatible with IKSolver's midIdx = startIdx+1 assumption)
    BuildLimbRig();

    // Phase 3: dragon loaded from disk (look-at only, no chain registration)
    if (!LoadDragonAsset())
    {
        DIA_LOG_ERROR("CluicheTest", "IK2DTestStageModule — dragon asset load failed");
        ReportFailed();
        return;
    }

    mDragonSkeleton = std::make_unique<Dia::Rig2D::Skeleton>(mDragonDef);
    mDragonPose     = std::make_unique<Dia::Rig2D::Pose>(*mDragonSkeleton);
    mDragonPose->SetToBindPose(*mDragonSkeleton);
    mDragonSolver   = std::make_unique<Dia::IK2D::IKSolver>(*mDragonSkeleton, *mDragonPose);

    {
        Dia::Rig2D::BoneTransform identity;
        mDragonSolver->SetRootTransform(identity);
    }

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ik2d.two_bone_converged"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mTwoBoneConverged,
                     mTwoBoneConverged ? "end effector within tolerance" : "two-bone phase not yet complete",
                     0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ik2d.fabrik_converged"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mFABRIKConverged,
                     mFABRIKConverged ? "end effector within tolerance" : "FABRIK phase not yet complete",
                     0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ik2d.look_at_accurate"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mLookAtAccurate,
                     mLookAtAccurate ? "Head facing target within tolerance" : "look-at phase not yet complete",
                     0.0f };
        });

    DIA_LOG_INFO("CluicheTest", "IK2DTestStageModule — started (limb: %d bones, dragon: %d bones)",
        mLimbSkeleton->GetBoneCount(), mDragonSkeleton->GetBoneCount());
}

void IK2DTestStageModule::OnUpdate(float /*deltaTime*/)
{
    DIA_TRACE_ZONE("ik2d_stage.update", Dia::Observation::Trace::Category::kDiaAnimation);
    DIA_PROFILE_SCOPE("ik2d_stage.update", Dia::Observation::Profile::Category::kDiaAnimation);

    if (IsResolved())
    {
        if (mLingerFrames > 0)
            --mLingerFrames;
        return;
    }

    Dia::Rig2D::BoneTransform identity;

    if (mPhase == 0)
    {
        mLimbSolver->SetRootTransform(identity);
        mCurrentTarget = ComputePhase1Target(mPhaseFrame);
        Dia::IK2D::PoleVector pole;
        pole.direction = Dia::Maths::Vector2D(1.0f, 0.0f);
        pole.weight    = 1.0f;
        mLimbSolver->SolveTwoBone(Dia::Core::StringCRC("limb.two_bone"), mCurrentTarget, &pole);

        if (mPhaseFrame == kPhase1GoldenFrame)
            VerifyPhase1();

        if (mPhaseFrame >= kPhase1Frames - 1)
        {
            mPhase      = 1;
            mPhaseFrame = 0;
            mLimbPose->SetToBindPose(*mLimbSkeleton);
            return;
        }
    }
    else if (mPhase == 1)
    {
        mLimbSolver->SetRootTransform(identity);
        mCurrentTarget = ComputePhase2Target(mPhaseFrame);
        mLimbSolver->SolveFABRIK(Dia::Core::StringCRC("limb.fabrik"), mCurrentTarget);

        if (mPhaseFrame == kPhase2GoldenFrame)
            VerifyPhase2();

        if (mPhaseFrame >= kPhase2Frames - 1)
        {
            mPhase      = 2;
            mPhaseFrame = 0;
            mDragonPose->SetToBindPose(*mDragonSkeleton);
#ifdef DIA_DEBUG
            if (mDrawer)
                mDrawer->UpdateSolver(*mDragonSolver, *mDragonSkeleton);
#endif
            return;
        }
    }
    else if (mPhase == 2)
    {
        mDragonSolver->SetRootTransform(identity);
        mCurrentTarget = ComputePhase3Target(mPhaseFrame);
        mDragonSolver->SolveLookAt(Dia::Core::StringCRC("Head"), mCurrentTarget);

        if (mPhaseFrame == kPhase3GoldenFrame)
            VerifyPhase3();

        if (mPhaseFrame >= kPhase3Frames - 1)
        {
            EmitMetrics();
            mLingerFrames = kLingerFrames;

            bool allPassed = mTwoBoneConverged && mFABRIKConverged && mLookAtAccurate;
            if (allPassed)
                ReportPassed();
            else
                ReportFailed();

            mPhase = 3;
            return;
        }
    }

    ++mPhaseFrame;

#ifdef DIA_DEBUG
    if (!mDrawer && mLimbSkeleton && mLimbSolver)
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            // Expose the active solver to the drawer:
            // phases 0+1 → limb solver, phase 2 → dragon solver
            const Dia::IK2D::IKSolver& activeSolver =
                (mPhase < 2) ? *mLimbSolver : *mDragonSolver;
            const Dia::Rig2D::Skeleton& activeSkeleton =
                (mPhase < 2) ? *mLimbSkeleton : *mDragonSkeleton;

            mDrawer = std::make_unique<IK2DTestDrawer>(
                activeSolver, activeSkeleton,
                mPhase, mPhaseFrame, mCurrentTarget,
                mTwoBoneConverged, mFABRIKConverged, mLookAtAccurate,
                mTwoBoneError, mFABRIKError, mLookAtError,
                vd->GetLayerManager());
            vd->GetLayerManager().Register(mDrawer.get(), 10, Dia::Core::StringCRC("IK2D"));
        }
    }
#endif
}

void IK2DTestStageModule::OnStop()
{
    mLimbSolver.reset();
    mLimbPose.reset();
    mLimbSkeleton.reset();
    mDragonSolver.reset();
    mDragonPose.reset();
    mDragonSkeleton.reset();

    mPhase             = 0;
    mPhaseFrame        = 0;
    mTwoBoneConverged  = false;
    mFABRIKConverged   = false;
    mLookAtAccurate    = false;
    mTwoBoneError      = 3.402823466e+38f;
    mFABRIKError       = 3.402823466e+38f;
    mLookAtError       = 3.402823466e+38f;
    mLingerFrames      = 0;

    DIA_LOG_INFO("CluicheTest", "IK2DTestStageModule — stopped");
}

void IK2DTestStageModule::BuildLimbRig()
{
    // kLimbBoneCount=5 consecutive bones, kLimbBoneLength=0.8 each.
    // Two-bone chain: bone0→bone2 (jointCount = 2-0 = 2). ✓
    // FABRIK chain:   bone0→bone4 (jointCount = 4-0 = 4). ✓
    mLimbDef      = Dia::IK2D::Testing::BuildLimbSkeletonDef(kLimbBoneCount, kLimbBoneLength);
    mLimbSkeleton = std::make_unique<Dia::Rig2D::Skeleton>(mLimbDef);
    mLimbPose     = std::make_unique<Dia::Rig2D::Pose>(*mLimbSkeleton);
    mLimbPose->SetToBindPose(*mLimbSkeleton);
    mLimbSolver   = std::make_unique<Dia::IK2D::IKSolver>(*mLimbSkeleton, *mLimbPose);

    // Pre-initialise world transforms so Draw() is safe before the first OnUpdate tick.
    Dia::Rig2D::BoneTransform identity;
    mLimbSolver->SetRootTransform(identity);

    RegisterChains();
}

bool IK2DTestStageModule::LoadDragonAsset()
{
    Dia::Core::FilePath skelPath("stage_root", "skeleton", "dragon.diarig");
    Dia::Core::FilePath::ResoledFilePath resolved;
    skelPath.Resolve(resolved);

    Dia::Core::SerializedFileLoad loader;
    return loader.LoadNow<Dia::Rig2D::SkeletonDef, 8192>(resolved, mDragonDef, 8192)
        == Dia::Core::IFileLoad::ReturnCode::kSuccess;
}

void IK2DTestStageModule::RegisterChains()
{
    // Two-bone: bone0(root)→bone2(end), jointCount = 2-0 = 2 ✓
    {
        Dia::IK2D::IKChainDef def;
        def.id          = Dia::Core::StringCRC("limb.two_bone");
        def.startBoneId = Dia::Core::StringCRC("bone0");
        def.endBoneId   = Dia::Core::StringCRC("bone2");
        def.reachWeight = 1.0f;
        mLimbSolver->RegisterChain(def);
    }

    // FABRIK: bone0(root)→bone4(end), jointCount = 4-0 = 4 ✓
    {
        Dia::IK2D::IKChainDef def;
        def.id            = Dia::Core::StringCRC("limb.fabrik");
        def.startBoneId   = Dia::Core::StringCRC("bone0");
        def.endBoneId     = Dia::Core::StringCRC("bone4");
        def.reachWeight   = 1.0f;
        def.maxIterations = 20;
        def.tolerance     = 0.001f;
        mLimbSolver->RegisterChain(def);
    }
}

// Phase 1 target: arc sweep, radius within 2-bone reach (2 bones × 0.8 = 1.6)
Dia::Maths::Vector2D IK2DTestStageModule::ComputePhase1Target(int phaseFrame) const
{
    float t     = static_cast<float>(phaseFrame) / static_cast<float>(kPhase1Frames);
    float angle = Dia::Maths::PI * t;
    return Dia::Maths::Vector2D(1.4f * std::cos(angle), 1.4f * std::sin(angle));
}

// Phase 2 target: sinusoidal, within full limb reach (4 bones × 0.8 = 3.2)
Dia::Maths::Vector2D IK2DTestStageModule::ComputePhase2Target(int phaseFrame) const
{
    float t = static_cast<float>(phaseFrame) / static_cast<float>(kPhase2Frames);
    float x = 2.5f * std::sin(t * Dia::Maths::PI_2);
    float y = 1.5f + 0.5f * std::cos(t * Dia::Maths::PI_2);
    return Dia::Maths::Vector2D(x, y);
}

// Phase 3 target: circular orbit around dragon head
Dia::Maths::Vector2D IK2DTestStageModule::ComputePhase3Target(int phaseFrame) const
{
    float t     = static_cast<float>(phaseFrame) / static_cast<float>(kPhase3Frames);
    float angle = t * Dia::Maths::PI_2;
    return Dia::Maths::Vector2D(2.0f * std::cos(angle), 2.0f * std::sin(angle));
}

void IK2DTestStageModule::VerifyPhase1()
{
    const auto& wt = mLimbSolver->GetWorldTransforms();
    // bone2 is the end effector of the two-bone chain
    Dia::Maths::Vector2D tipPos = wt[2].position;
    Dia::Maths::Vector2D target = ComputePhase1Target(kPhase1GoldenFrame);
    Dia::Maths::Vector2D delta  = tipPos - target;
    mTwoBoneError     = delta.Magnitude();
    mTwoBoneConverged = (mTwoBoneError < kPositionTolerance);

    DIA_LOG_INFO("CluicheTest", "IK2D Phase1 (TwoBone): error=%.4f %s",
        mTwoBoneError, mTwoBoneConverged ? "PASS" : "FAIL");
}

void IK2DTestStageModule::VerifyPhase2()
{
    const auto& wt = mLimbSolver->GetWorldTransforms();
    // bone4 is the end effector of the FABRIK chain
    Dia::Maths::Vector2D tipPos = wt[4].position;
    Dia::Maths::Vector2D target = ComputePhase2Target(kPhase2GoldenFrame);
    Dia::Maths::Vector2D delta  = tipPos - target;
    mFABRIKError     = delta.Magnitude();
    mFABRIKConverged = (mFABRIKError < kPositionTolerance);

    DIA_LOG_INFO("CluicheTest", "IK2D Phase2 (FABRIK): error=%.4f %s",
        mFABRIKError, mFABRIKConverged ? "PASS" : "FAIL");
}

void IK2DTestStageModule::VerifyPhase3()
{
    const auto& wt  = mDragonSolver->GetWorldTransforms();
    int headIdx     = mDragonSkeleton->FindBoneIndex(Dia::Core::StringCRC("Head"));
    if (headIdx < 0) return;

    float headRot    = wt[headIdx].rotation;
    Dia::Maths::Vector2D headPos = wt[headIdx].position;
    Dia::Maths::Vector2D target  = ComputePhase3Target(kPhase3GoldenFrame);
    float expectedAngle = std::atan2(target.Y() - headPos.Y(), target.X() - headPos.X());

    mLookAtError = std::fabsf(headRot - expectedAngle);
    if (mLookAtError > Dia::Maths::PI)
        mLookAtError = Dia::Maths::PI_2 - mLookAtError;
    mLookAtAccurate = (mLookAtError < kAngleTolerance);

    DIA_LOG_INFO("CluicheTest", "IK2D Phase3 (LookAt): error=%.4f rad %s",
        mLookAtError, mLookAtAccurate ? "PASS" : "FAIL");
}

void IK2DTestStageModule::EmitMetrics()
{
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    auto* twoBone = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.ik2d.two_bone_error"));
    auto* fabrik  = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.ik2d.fabrik_error"));
    auto* lookAt  = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.ik2d.look_at_error"));

    if (twoBone) twoBone->Set(mTwoBoneError);
    if (fabrik)  fabrik->Set(mFABRIKError);
    if (lookAt)  lookAt->Set(mLookAtError);

    DIA_LOG_INFO("CluicheTest",
        "IK2DTestStageModule — complete: two_bone=%.4f FABRIK=%.4f look_at=%.4f rad",
        mTwoBoneError, mFABRIKError, mLookAtError);
}

} // namespace CluicheTest

namespace { using IK2DTestStageModule_ = CluicheTest::IK2DTestStageModule; }
DIA_MODULE(IK2DTestStageModule_);
DIA_DESCRIBE(IK2DTestStageModule_::kTypeId, "Test stage that validates DiaIK2D solvers (two-bone, FABRIK, look-at) on procedural limb and dragon skeletons.");
