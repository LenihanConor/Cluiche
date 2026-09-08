#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

class Animation2DTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    Animation2DTestDrawer(
        const Dia::Rig2D::Skeleton&             skeleton,
        const Dia::Rig2D::Pose&                 pose,
        const Dia::Animation2D::AnimClipPlayer& player,
        const int&                              currentClipIndex,
        const unsigned int&                     clipsPlayed,
        const unsigned int&                     totalPlaybackFrames,
        const bool&                             allCompleted,
        const bool&                             poseCorrect,
        const Dia::Debug::DebugLayerManager&    layerManager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::Rig2D::Skeleton&             mSkeleton;
    const Dia::Rig2D::Pose&                 mPose;
    const Dia::Animation2D::AnimClipPlayer& mPlayer;
    const int&                              mCurrentClipIndex;
    const unsigned int&                     mClipsPlayed;
    const unsigned int&                     mTotalPlaybackFrames;
    const bool&                             mAllCompleted;
    const bool&                             mPoseCorrect;

    const Dia::Debug::DebugLayerManager& mLayerManager;

    static constexpr float kDisplayScale = 200.0f;
    static constexpr float kOriginX      = 400.0f;
    static constexpr float kOriginY      = 300.0f;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
