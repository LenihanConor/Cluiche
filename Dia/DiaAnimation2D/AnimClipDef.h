#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "KeyframeTrack.h"

static const unsigned int kMaxTracksPerClip = 32;

namespace Dia { namespace Animation2D {
    struct AnimClipDef {
        Dia::Core::StringCRC id;
        float duration = 0.0f;
        Dia::Core::Containers::DynamicArrayC<KeyframeTrack, kMaxTracksPerClip> tracks;
    };
} }
