#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::GridVisibility {

    // Opaque group key — any string name the game assigns meaning to (team, faction, squad).
    using VisibilityGroupId = Dia::Core::StringCRC;

} // namespace Dia::GridVisibility
