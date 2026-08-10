#pragma once

#include <stdint.h>

#include "DiaSaveGame/SaveFormat.h"

namespace Dia::SaveGame {

struct SaveConfig {
    const char* baseDirectory;  // e.g. "out/saves/"
    const char* slotPattern;    // e.g. "slot_{id}.sav" — {id} is replaced by slotId.AsChar()
    uint32_t    maxSlots;       // 0 = unlimited
    SaveFormat  format;
};

} // namespace Dia::SaveGame
