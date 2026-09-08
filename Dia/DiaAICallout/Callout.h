#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia::AICallout {

    struct Callout {
        Dia::Core::StringCRC  kind;       // e.g. StringCRC{"HelpNeeded"}
        Dia::Maths::Vector2D  position;
        float                 radius = 0.0f; // query radius in world units
        Dia::Core::StringCRC  faction;      // kInvalidCRC = any faction
        float                 ttl    = 0.0f; // seconds until auto-expiry
        Json::Value           payload;    // optional typed data (caller-defined schema)
    };

} // namespace Dia::AICallout
