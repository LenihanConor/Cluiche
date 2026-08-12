#pragma once

namespace Dia
{
    namespace Input
    {
        enum EKeyModifiers : int
        {
            kModNone    = 0,
            kModShift   = 1 << 0,
            kModControl = 1 << 1,
            kModAlt     = 1 << 2,
            kModSystem  = 1 << 3,
        };
    }
}
