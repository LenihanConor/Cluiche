#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "AnimClipDef.h"

namespace Dia { namespace Rig2D { class Skeleton; class Pose; } }

namespace Dia { namespace Animation2D {
    class AnimClip {
    public:
        explicit AnimClip(const AnimClipDef& def, const Dia::Rig2D::Skeleton& skeleton);

        const Dia::Core::StringCRC& GetId() const;
        float GetDuration() const;
        int   GetTrackCount() const;

        void Sample(float time,
                    const Dia::Rig2D::Skeleton& skeleton,
                    Dia::Rig2D::Pose& outPose) const;

    private:
        struct ResolvedTrack {
            int boneIndex = -1;
            bool rotationOnly = false;
            Dia::Core::Containers::DynamicArrayC<Keyframe, kMaxKeyframesPerTrack> keyframes;
        };

        static const unsigned int kMaxResolvedTracks = 32;

        Dia::Core::StringCRC mId;
        float mDuration = 0.0f;
        Dia::Core::Containers::DynamicArrayC<ResolvedTrack, kMaxResolvedTracks> mTracks;
    };
} }
