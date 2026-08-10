#pragma once

#include "DiaCore/CRC/StringCRC.h"

namespace Dia::SaveGame {

struct SlotInfo {
    Dia::Core::StringCRC id;
    char path[256]; // absolute path to the .sav file
};

} // namespace Dia::SaveGame
