#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Animation2DTestDrawer.h"
#include <DiaRig2DVisualDebugger/BoneLinesDrawer.h>
#include <DiaRig2DVisualDebugger/JointCirclesDrawer.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <imgui.h>

namespace CluicheTest {

static const char* kClipNames[] = { "idle_clip", "flap_up_clip", "flap_down_clip" };

Animation2DTestDrawer::Animation2DTestDrawer(
    const Dia::Rig2D::Skeleton&             skeleton,
    const Dia::Rig2D::Pose&                 pose,
    const Dia::Animation2D::AnimClipPlayer& player,
    const int&                              currentClipIndex,
    const unsigned int&                     clipsPlayed,
    const unsigned int&                     totalPlaybackFrames,
    const bool&                             allCompleted,
    const bool&                             poseCorrect,
    const Dia::Debug::DebugLayerManager&    layerManager)
    : mSkeleton(skeleton)
    , mPose(pose)
    , mPlayer(player)
    , mCurrentClipIndex(currentClipIndex)
    , mClipsPlayed(clipsPlayed)
    , mTotalPlaybackFrames(totalPlaybackFrames)
    , mAllCompleted(allCompleted)
    , mPoseCorrect(poseCorrect)
    , mLayerManager(layerManager)
{}

Dia::Core::StringCRC Animation2DTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("animation2d.dragon");
}

void Animation2DTestDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    // Compute raw world transforms
    Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> rawTransforms;
    Dia::Rig2D::BoneTransform rootTransform;
    mPose.ComputeWorldTransforms(mSkeleton, rootTransform, rawTransforms);

    // Re-map into screen-space so engine drawers can draw without knowing the test layout
    Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> screenTransforms;
    for (unsigned int i = 0; i < rawTransforms.Size(); ++i)
    {
        Dia::Rig2D::BoneTransform st = rawTransforms[i];
        st.position = Dia::Maths::Vector2D(
            kOriginX + rawTransforms[i].position.X() * kDisplayScale,
            kOriginY - rawTransforms[i].position.Y() * kDisplayScale);
        screenTransforms.Add(st);
    }

    // Delegate bone lines and joint circles to engine drawers
    Dia::Rig2D::BoneLinesDrawer    boneLines   (mSkeleton, screenTransforms, mLayerManager);
    Dia::Rig2D::JointCirclesDrawer jointCircles(mSkeleton, screenTransforms, mLayerManager);

    boneLines.Draw(frameData);
    jointCircles.Draw(frameData);
}

void Animation2DTestDrawer::DrawImGui()
{
    ImGui::Text("Clip:   %s", (mCurrentClipIndex >= 0 && mCurrentClipIndex < 3) ? kClipNames[mCurrentClipIndex] : "none");
    ImGui::Text("Played: %u / 3", mClipsPlayed);
    ImGui::Text("Frames: %u / 180", mTotalPlaybackFrames);

    float normTime = mPlayer.GetNormalizedTime();
    ImGui::ProgressBar(normTime, ImVec2(-1.f, 0.f));

    ImGui::Separator();

    int wingL = mSkeleton.FindBoneIndex(Dia::Core::StringCRC("Wing_L"));
    int wingR = mSkeleton.FindBoneIndex(Dia::Core::StringCRC("Wing_R"));
    if (wingL >= 0)
    {
        float rot = mPose.GetLocalTransform(wingL).rotation;
        ImGui::Text("Wing_L: %.3f rad (%.1f\xc2\xb0)", rot, rot * 57.2957f);
    }
    if (wingR >= 0)
    {
        float rot = mPose.GetLocalTransform(wingR).rotation;
        ImGui::Text("Wing_R: %.3f rad (%.1f\xc2\xb0)", rot, rot * 57.2957f);
    }

    ImGui::Separator();

    if (mAllCompleted)
    {
        if (mPoseCorrect)
            ImGui::TextColored(ImVec4(0.2f, 1.f, 0.2f, 1.f), "PASS - pose within tolerance");
        else
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "FAIL - pose mismatch");
    }
    else
    {
        ImGui::TextDisabled("Playback in progress...");
    }
}

} // namespace CluicheTest

#endif // DIA_DEBUG
