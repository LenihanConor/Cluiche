#pragma once
#include <DiaAnimation2D/AnimClip.h>
#include <DiaAnimation2D/AnimClipDef.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Animation2D { namespace Testing {

    // Build a clip covering the first boneCount bones from the skeleton (by index).
    // Each track has two keyframes: rotation=startRot at t=0, rotation=endRot at t=1.
    inline AnimClip BuildTestClip(
        const Dia::Rig2D::Skeleton& skeleton,
        int boneCount,
        float startRot,
        float endRot,
        float duration = 1.0f)
    {
        AnimClipDef def;
        def.id       = Dia::Core::StringCRC("test_clip");
        def.duration = duration;

        int count = boneCount;
        if (count > skeleton.GetBoneCount()) count = skeleton.GetBoneCount();

        for (int b = 0; b < count; ++b) {
            KeyframeTrack track;
            track.boneId = skeleton.GetBone(b).name;

            Keyframe kf0;
            kf0.time     = 0.0f;
            kf0.rotation = startRot;
            kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
            kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

            Keyframe kf1;
            kf1.time     = duration;
            kf1.rotation = endRot;
            kf1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
            kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

            track.keyframes.Add(kf0);
            track.keyframes.Add(kf1);
            def.tracks.Add(track);
        }

        return AnimClip(def, skeleton);
    }

    // Build a clip on a single named bone.
    inline AnimClip BuildSingleBoneClip(
        const Dia::Rig2D::Skeleton& skeleton,
        Dia::Core::StringCRC boneId,
        float startRot,
        float endRot,
        float duration = 1.0f)
    {
        AnimClipDef def;
        def.id       = Dia::Core::StringCRC("single_bone_clip");
        def.duration = duration;

        KeyframeTrack track;
        track.boneId = boneId;

        Keyframe kf0;
        kf0.time     = 0.0f;
        kf0.rotation = startRot;
        kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
        kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

        Keyframe kf1;
        kf1.time     = duration;
        kf1.rotation = endRot;
        kf1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
        kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

        track.keyframes.Add(kf0);
        track.keyframes.Add(kf1);
        def.tracks.Add(track);

        return AnimClip(def, skeleton);
    }

} } }
