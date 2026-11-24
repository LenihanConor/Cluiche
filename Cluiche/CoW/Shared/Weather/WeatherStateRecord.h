// A decoded weather-state record, the payload of a StateReplace queue entry (A4 WeatherModule, Gap-pass amendment 1).
// Plain struct created in Task 4 so the queue entry kind has a payload type; Feature 4 (save/restore) owns the field
// list and the apply branch, and extends this struct. Until then a drained StateReplace is logged and not applied.
#pragma once

#include <stdint.h>

namespace CoW {
namespace Weather {

struct WeatherStateRecord
{
    uint32_t cellCount = 0;
    uint64_t week = 0;
};

} // namespace Weather
} // namespace CoW
