#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include "BoneMask.h"

namespace Dia { namespace Rig2D { class Pose; } }

namespace Dia { namespace Animation2D {
    struct PoseLayer {
        Dia::Core::StringCRC    id;
        const Dia::Rig2D::Pose* pose     = nullptr;
        float                   weight   = 1.0f;
        int                     priority = 0;
        const BoneMask*         boneMask = nullptr;
    };
} }
