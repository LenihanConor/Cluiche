#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace Steering
    {
        // Log channel identifier used by DiaObservation macros throughout DiaSteering.
        static const Dia::Core::StringCRC kLogChannel{"Steering"};
    }
}
