#include "Modules/TestStages/Animation2DTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/SerializedFileLoad.h>
#include <DiaCore/Reflect/JsonArchive.h>
#include <DiaRig2D/DiaRig2DSerializers.h>
#include <DiaAnimation2D/DiaAnimation2DSerializers.h>
#include <cmath>


namespace CluicheTest {

const Dia::Core::StringCRC Animation2DTestStageModule::kTypeId("Animation2DTestStageModule");

Animation2DTestStageModule::Animation2DTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Animation2DTestStageModule::~Animation2DTestStageModule() = default;

Dia::Core::StringCRC Animation2DTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("Animation2DTestStage");
}

const Dia::Core::StringCRC* Animation2DTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("animation2d.clips_completed"),
        Dia::Core::StringCRC("animation2d.pose_correct"),
    };
    outCount = 2;
    return names;
}

void Animation2DTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    if (!LoadAssets())
    {
        DIA_LOG_ERROR("CluicheTest", "Animation2DTestStageModule — asset load failed");
        ReportFailed();
        return;
    }

    mSkeleton = std::make_unique<Dia::Rig2D::Skeleton>(mSkeletonDef);
    mPose     = std::make_unique<Dia::Rig2D::Pose>(*mSkeleton);
    mPose->SetToBindPose(*mSkeleton);

    for (unsigned int i = 0; i < kClipCount; ++i)
        mClips[i] = std::make_unique<Dia::Animation2D::AnimClip>(mClipDefs[i], *mSkeleton);

    mPlayer.Play(*mClips[0], Dia::Animation2D::PlaybackMode::kOneShot);
    mCurrentClipIndex = 0;

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("animation2d.clips_completed"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mAllCompleted,
                     mAllCompleted ? "all 3 clips played" : "playback in progress",
                     0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("animation2d.pose_correct"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPoseCorrect,
                     mPoseCorrect ? "wing pose within tolerance" : "pose not yet verified",
                     0.0f };
        });

    DIA_LOG_INFO("CluicheTest", "Animation2DTestStageModule — started, dragon skeleton loaded (%d bones)",
        mSkeleton->GetBoneCount());
}

void Animation2DTestStageModule::OnUpdate(float deltaTime)
{
    if (IsResolved())
    {
        // Linger so the user can see the final pose before automation exits
        if (mLingerFrames > 0)
            --mLingerFrames;
        return;
    }

    if (mAllCompleted)
        return;

    mPlayer.Update(deltaTime);
    mPlayer.Sample(*mSkeleton, *mPose);
    ++mTotalPlaybackFrames;

    if (!mPlayer.IsPlaying())
    {
        AdvanceToNextClip();
    }

#ifdef DIA_DEBUG
    if (!mDrawer && mSkeleton && mPose)
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            mDrawer = std::make_unique<Animation2DTestDrawer>(
                *mSkeleton, *mPose, mPlayer,
                mCurrentClipIndex, mClipsPlayed, mTotalPlaybackFrames,
                mAllCompleted, mPoseCorrect,
                vd->GetLayerManager());
            vd->GetLayerManager().Register(mDrawer.get(), 20, Dia::Core::StringCRC("Anim2D"));
        }
    }
#endif
}

void Animation2DTestStageModule::OnStop()
{
    for (unsigned int i = 0; i < kClipCount; ++i)
        mClips[i].reset();
    mPose.reset();
    mSkeleton.reset();

    mCurrentClipIndex   = 0;
    mClipsPlayed        = 0;
    mTotalPlaybackFrames = 0;
    mAllCompleted       = false;
    mPoseCorrect        = false;
    mLingerFrames       = 0;

    DIA_LOG_INFO("CluicheTest", "Animation2DTestStageModule — stopped");
}

bool Animation2DTestStageModule::LoadAssets()
{
    // Skeleton
    {
        Dia::Core::FilePath skelPath("stage_root", "skeleton", "dragon.diarig");
        // stage_root alias resolves to the stage asset directory
        Dia::Core::FilePath::ResoledFilePath resolved;
        skelPath.Resolve(resolved);

        Dia::Core::SerializedFileLoad loader;
        if (loader.LoadNow<Dia::Rig2D::SkeletonDef, 8192>(resolved, mSkeletonDef, 8192)
            != Dia::Core::IFileLoad::ReturnCode::kSuccess)
            return false;
    }

    // 3 clips
    const char* clipFiles[] = { "idle_clip.diaclip", "flap_up_clip.diaclip", "flap_down_clip.diaclip" };
    for (unsigned int i = 0; i < kClipCount; ++i)
    {
        Dia::Core::FilePath clipPath("stage_root", "clips", clipFiles[i]);
        Dia::Core::FilePath::ResoledFilePath resolved;
        clipPath.Resolve(resolved);

        Dia::Core::SerializedFileLoad loader;
        if (loader.LoadNow<Dia::Animation2D::AnimClipDef, 8192>(resolved, mClipDefs[i], 8192)
            != Dia::Core::IFileLoad::ReturnCode::kSuccess)
            return false;
    }

    return true;
}

void Animation2DTestStageModule::AdvanceToNextClip()
{
    ++mClipsPlayed;

    if (mCurrentClipIndex < static_cast<int>(kClipCount) - 1)
    {
        ++mCurrentClipIndex;
        mPlayer.Play(*mClips[mCurrentClipIndex], Dia::Animation2D::PlaybackMode::kOneShot);
    }
    else
    {
        mAllCompleted = true;
        mPoseCorrect  = VerifyGoldenPose();
        EmitMetrics();
        mLingerFrames = kLingerFrames;

        if (mPoseCorrect)
            ReportPassed();
        else
            ReportFailed();
    }
}

bool Animation2DTestStageModule::VerifyGoldenPose() const
{
    Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> worldTransforms;
    Dia::Rig2D::BoneTransform rootTransform; // identity
    mPose->ComputeWorldTransforms(*mSkeleton, rootTransform, worldTransforms);

    int wingLIdx = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("Wing_L"));
    int wingRIdx = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("Wing_R"));

    if (wingLIdx < 0 || wingRIdx < 0)
        return false;

    // After flap_down_clip end: wings swept to local Y = -0.6 (world Y = -0.6)
    float wingLY = worldTransforms[wingLIdx].position.Y();
    float wingRY = worldTransforms[wingRIdx].position.Y();

    return std::fabsf(wingLY + 0.6f) < kPoseTolerance
        && std::fabsf(wingRY + 0.6f) < kPoseTolerance;
}

void Animation2DTestStageModule::EmitMetrics()
{
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    auto* clipsPlayed = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.anim2d.clips_played"));
    auto* totalFrames = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.anim2d.total_playback_frames"));

    if (clipsPlayed) clipsPlayed->Set(static_cast<float>(mClipsPlayed));
    if (totalFrames) totalFrames->Set(static_cast<float>(mTotalPlaybackFrames));

    DIA_LOG_INFO("CluicheTest",
        "Animation2DTestStageModule — complete: clips_played=%u, total_frames=%u, pose_correct=%s",
        mClipsPlayed, mTotalPlaybackFrames, mPoseCorrect ? "yes" : "no");
}

} // namespace CluicheTest

namespace { using Animation2DTestStageModule_ = CluicheTest::Animation2DTestStageModule; }
DIA_MODULE(Animation2DTestStageModule_);
