#pragma once
#include "AnimClip.h"

namespace Dia { namespace Rig2D { class Skeleton; class Pose; } }

namespace Dia { namespace Animation2D {
    enum class PlaybackMode { kOneShot, kLooping };

    class AnimClipPlayer {
    public:
        void Play(const AnimClip& clip, PlaybackMode mode = PlaybackMode::kOneShot);
        void Play(const AnimClip* clip);  // preserves current looping mode set by SetLooping()
        void Stop();
        void SetSpeed(float speed);
        void SetLooping(bool looping);

        void Update(float dt);
        void Sample(const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& outPose) const;

        bool  IsPlaying() const;
        float GetCurrentTime() const;
        float GetNormalizedTime() const;
        const AnimClip* GetCurrentClip() const;

    private:
        const AnimClip* mClip = nullptr;
        PlaybackMode    mMode = PlaybackMode::kOneShot;
        float           mCurrentTime = 0.0f;
        float           mSpeed = 1.0f;
        bool            mIsPlaying = false;
    };
} }
