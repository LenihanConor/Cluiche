#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "Keyframe.h"

static const unsigned int kMaxKeyframesPerTrack = 64;

namespace Dia { namespace Animation2D {
    struct KeyframeTrack {
        Dia::Core::StringCRC boneId;
        bool rotationOnly = false;
        Dia::Core::Containers::DynamicArrayC<Keyframe, kMaxKeyframesPerTrack> keyframes;
    };
} }
