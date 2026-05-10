#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche {

    // Event carried on the SimToUI stream (Sim PU -> Main PU UIModule).
    // Used for HUD values (FPS, Score) and page lifecycle triggers.
    struct UICommand {
        enum class Type {
            kShowText,
            kUpdateValue,
            kHideText,
        };

        Type type = Type::kShowText;
        Dia::Core::StringCRC key;   // e.g. "FPS", "Score"
        float value = 0.0f;
    };

}
