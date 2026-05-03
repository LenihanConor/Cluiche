#pragma once
#include <DiaAnimation2D/AnimClipDef.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Animation2D { namespace Testing {

    inline AnimClipDef BuildTestClip(
        const Dia::Core::StringCRC& id,
        float duration,
        const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& boneIds,
        int keyframesPerTrack = 2)
    {
        AnimClipDef def;
        def.id = id;
        def.duration = duration;
        for (unsigned int b = 0; b < boneIds.Size(); ++b) {
            KeyframeTrack track;
            track.boneId = boneIds[b];
            for (int k = 0; k < keyframesPerTrack; ++k) {
                Keyframe kf;
                kf.time     = duration * (float)k / (float)(keyframesPerTrack > 1 ? keyframesPerTrack - 1 : 1);
                kf.rotation = (float)k / (float)(keyframesPerTrack > 1 ? keyframesPerTrack - 1 : 1);
                track.keyframes.Add(kf);
            }
            def.tracks.Add(track);
        }
        return def;
    }

    inline AnimClipDef BuildSingleBoneClip(
        const Dia::Core::StringCRC& clipId,
        const Dia::Core::StringCRC& boneId,
        float duration,
        float startRotation, float endRotation)
    {
        AnimClipDef def;
        def.id = clipId;
        def.duration = duration;
        KeyframeTrack track;
        track.boneId = boneId;
        Keyframe kf0; kf0.time = 0.0f;    kf0.rotation = startRotation; track.keyframes.Add(kf0);
        Keyframe kf1; kf1.time = duration; kf1.rotation = endRotation;   track.keyframes.Add(kf1);
        def.tracks.Add(track);
        return def;
    }

} } }
