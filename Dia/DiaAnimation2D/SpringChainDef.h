#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include "SpringNodeDef.h"

static const unsigned int kMaxSpringChainBones = 64;

namespace Dia { namespace Animation2D {
    struct SpringChainDef {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC rootBoneId;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxSpringChainBones> boneIds;
        SpringNodeDef defaultNode;
        Dia::Core::Containers::DynamicArrayC<SpringNodeDef, kMaxSpringChainBones> nodeOverrides;
        Dia::Maths::Vector2D gravityDirection = Dia::Maths::Vector2D(0.0f, -1.0f);
        float gravityStrength = 0.0f;
    };
} }
