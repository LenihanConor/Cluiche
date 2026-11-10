#pragma once

#include <stdint.h>

#include "DiaCore/CRC/StringCRC.h"

namespace Dia::SaveGame {

struct SlotInfo {
    Dia::Core::StringCRC id;
    char path[256]; // absolute path to the .sav file
    uint64_t timestamp = 0; // Unix seconds (UTC) the slot file was last written
};

} // namespace Dia::SaveGame
