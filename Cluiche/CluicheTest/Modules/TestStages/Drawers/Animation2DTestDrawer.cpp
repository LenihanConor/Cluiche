#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Animation2DTestDrawer.h"
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <imgui.h>
#include <cmath>

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
    Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> worldTransforms;
    Dia::Rig2D::BoneTransform rootTransform;
    mPose.ComputeWorldTransforms(mSkeleton, rootTransform, worldTransforms);

    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mLayerManager);

    static const Dia::Graphics::RGBA kBoneColour(120, 200, 255, 220);
    static const Dia::Graphics::RGBA kJointColour(255, 220, 80, 255);
    static const Dia::Graphics::RGBA kWingColour(80, 220, 120, 200);
    static const Dia::Graphics::RGBA kRootColour(255, 100, 100, 255);

    int boneCount = mSkeleton.GetBoneCount();
    for (int i = 0; i < boneCount; ++i)
    {
        const Dia::Rig2D::BoneTransform& wt = worldTransforms[i];
        float sx = kOriginX + wt.position.X() * kDisplayScale;
        float sy = kOriginY - wt.position.Y() * kDisplayScale;

        // Bone line to parent
        int parentIdx = mSkeleton.GetBone(i).parentIndex;
        if (parentIdx >= 0)
        {
            const Dia::Rig2D::BoneTransform& pt = worldTransforms[parentIdx];
            float px = kOriginX + pt.position.X() * kDisplayScale;
            float py = kOriginY - pt.position.Y() * kDisplayScale;

            // Wing bones in green, others in blue
            Dia::Core::StringCRC boneName = mSkeleton.GetBone(i).name;
            bool isWing = (boneName == Dia::Core::StringCRC("Wing_L") || boneName == Dia::Core::StringCRC("Wing_R"));
            Dia::Geometry2D::Line line(Dia::Maths::Vector2D(px, py), Dia::Maths::Vector2D(sx, sy));
            drawer.SubmitLine(line, isWing ? kWingColour : kBoneColour);
        }

        // Joint circle
        bool isRoot = (i == 0);
        float radius = isRoot ? 10.0f : 6.0f;
        Dia::Geometry2D::Circle joint(radius, Dia::Maths::Vector2D(sx, sy));
        drawer.SubmitCircle(joint, isRoot ? kRootColour : kJointColour);
    }

    drawer.Draw(frameData);
}

void Animation2DTestDrawer::DrawImGui()
{
    // Playback state
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
