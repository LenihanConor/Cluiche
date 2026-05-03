#include "AnimClipPlayer.h"
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <cmath>

namespace Dia { namespace Animation2D {

void AnimClipPlayer::Play(const AnimClip& clip, PlaybackMode mode) {
    mClip = &clip;
    mMode = mode;
    mCurrentTime = 0.0f;
    mIsPlaying = true;
}

void AnimClipPlayer::Play(const AnimClip* clip) {
    if (clip) {
        mClip = clip;
        mCurrentTime = 0.0f;
        mIsPlaying = true;
        // mMode is preserved — caller uses SetLooping() to control looping behavior
    }
}

void AnimClipPlayer::SetLooping(bool looping) {
    mMode = looping ? PlaybackMode::kLooping : PlaybackMode::kOneShot;
}

void AnimClipPlayer::Stop() {
    mIsPlaying = false;
    mClip = nullptr;
    mCurrentTime = 0.0f;
}

void AnimClipPlayer::SetSpeed(float speed) {
    mSpeed = speed;
}

void AnimClipPlayer::Update(float dt) {
    if (!mIsPlaying || !mClip) return;

    mCurrentTime += dt * mSpeed;
    float dur = mClip->GetDuration();

    if (mMode == PlaybackMode::kOneShot) {
        if (mCurrentTime >= dur) {
            mCurrentTime = dur;
            mIsPlaying = false;
        } else if (mCurrentTime < 0.0f) {
            mCurrentTime = 0.0f;
            mIsPlaying = false;
        }
    } else { // kLooping
        if (dur > 0.0f) {
            mCurrentTime = std::fmod(mCurrentTime, dur);
            if (mCurrentTime < 0.0f) mCurrentTime += dur;
        }
    }
}

void AnimClipPlayer::Sample(const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& outPose) const {
    if (!mIsPlaying || !mClip) return;
    mClip->Sample(mCurrentTime, skeleton, outPose);
}

bool  AnimClipPlayer::IsPlaying()  const { return mIsPlaying; }
bool  AnimClipPlayer::IsLooping()  const { return mMode == PlaybackMode::kLooping; }
float AnimClipPlayer::GetCurrentTime() const { return mCurrentTime; }
float AnimClipPlayer::GetNormalizedTime() const {
    if (!mClip || mClip->GetDuration() <= 0.0f) return 0.0f;
    float t = mCurrentTime / mClip->GetDuration();
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}
const AnimClip* AnimClipPlayer::GetCurrentClip() const { return mClip; }

} }
