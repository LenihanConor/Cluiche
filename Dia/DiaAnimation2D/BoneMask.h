#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

static const unsigned int kMaxBonesInMask = 128;

namespace Dia { namespace Animation2D {
    struct BoneMask {
        Dia::Core::StringCRC id;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxBonesInMask> boneIds;

        void Add(Dia::Core::StringCRC boneId) { boneIds.Add(boneId); }
    };
} }
