#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::GridVisibility {

    // StringCRC's char* constructor is not constexpr, so this matches the DiaFlowField /
    // DiaPathfinding pattern: namespace-scope `static const`, one copy per translation unit.
    static const Dia::Core::StringCRC kLogChannel{"GridVisibility"};

} // namespace Dia::GridVisibility
