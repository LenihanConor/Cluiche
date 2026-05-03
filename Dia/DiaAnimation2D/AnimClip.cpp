#include "AnimClip.h"
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/Bone.h>
#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia { namespace Animation2D {

static constexpr float kPi = 3.14159265358979f;

static float ShortestArcLerp(float a, float b, float t) {
    float diff = b - a;
    while (diff > kPi)  diff -= 2.0f * kPi;
    while (diff < -kPi) diff += 2.0f * kPi;
    return a + diff * t;
}

static float LerpFloat(float a, float b, float t) { return a + (b - a) * t; }

AnimClip::AnimClip(const AnimClipDef& def, const Dia::Rig2D::Skeleton& skeleton) {
    DIA_ASSERT(def.duration > 0.0f, "AnimClipDef duration must be positive");
    mId = def.id;
    mDuration = def.duration;

    for (unsigned int i = 0; i < def.tracks.Size(); ++i) {
        for (unsigned int j = i + 1; j < def.tracks.Size(); ++j) {
            DIA_ASSERT(def.tracks[i].boneId != def.tracks[j].boneId, "Duplicate bone track in AnimClipDef");
        }
    }

    for (unsigned int i = 0; i < def.tracks.Size(); ++i) {
        const KeyframeTrack& track = def.tracks[i];
        if (track.keyframes.Size() == 0) continue;

        for (unsigned int k = 1; k < track.keyframes.Size(); ++k) {
            DIA_ASSERT(track.keyframes[k].time >= track.keyframes[k-1].time, "Keyframes must be sorted by time");
        }

        int boneIdx = skeleton.FindBoneIndex(track.boneId);
        if (boneIdx == -1) continue;

        ResolvedTrack rt;
        rt.boneIndex = boneIdx;
        rt.rotationOnly = track.rotationOnly;
        rt.keyframes = track.keyframes;
        mTracks.Add(rt);
    }
}

const Dia::Core::StringCRC& AnimClip::GetId() const { return mId; }
float AnimClip::GetDuration() const { return mDuration; }
int   AnimClip::GetTrackCount() const { return (int)mTracks.Size(); }

void AnimClip::Sample(float time, const Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& outPose) const {
    if (time < 0.0f) time = 0.0f;
    if (time > mDuration) time = mDuration;

    outPose.SetToBindPose(skeleton);

    for (int ti = 0; ti < (int)mTracks.Size(); ++ti) {
        const ResolvedTrack& rt = mTracks[ti];
        const auto& kfs = rt.keyframes;
        if (kfs.Size() == 0) continue;

        float resultRotation = 0.0f;
        Dia::Maths::Vector2D resultPosition(0.0f, 0.0f);
        Dia::Maths::Vector2D resultScale(1.0f, 1.0f);

        if (kfs.Size() == 1 || time <= kfs[0].time) {
            resultRotation = kfs[0].rotation;
            resultPosition = kfs[0].position;
            resultScale    = kfs[0].scale;
        } else if (time >= kfs[kfs.Size()-1].time) {
            resultRotation = kfs[kfs.Size()-1].rotation;
            resultPosition = kfs[kfs.Size()-1].position;
            resultScale    = kfs[kfs.Size()-1].scale;
        } else {
            int lo = 0;
            for (unsigned int k = 0; k < kfs.Size() - 1; ++k) {
                if (time >= kfs[k].time && time <= kfs[k+1].time) { lo = (int)k; break; }
            }
            float t0 = kfs[lo].time;
            float t1 = kfs[lo+1].time;
            float t = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;

            resultRotation = ShortestArcLerp(kfs[lo].rotation, kfs[lo+1].rotation, t);
            resultPosition = Dia::Maths::Vector2D(
                LerpFloat(kfs[lo].position.x, kfs[lo+1].position.x, t),
                LerpFloat(kfs[lo].position.y, kfs[lo+1].position.y, t));
            resultScale = Dia::Maths::Vector2D(
                LerpFloat(kfs[lo].scale.x, kfs[lo+1].scale.x, t),
                LerpFloat(kfs[lo].scale.y, kfs[lo+1].scale.y, t));
        }

        Dia::Rig2D::BoneTransform& out = outPose.GetLocalTransform(rt.boneIndex);
        out.rotation = resultRotation;
        if (!rt.rotationOnly) {
            out.position = resultPosition;
            out.scale    = resultScale;
        }
    }
}

} }
