// A4 weather step profile: per-pass timing of the last step.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md ("Profile")
#pragma once

#include <cstdint>

namespace Draoi {
namespace HexWorld {
namespace Weather {

struct WeatherStepProfile
{
    // 0, 1, 2, 3, 4, 5a, 5b, 5c, 6a, 6b, 7, 8, 9, finish (pass 10 retired).
    static constexpr uint32_t kPassCount = 14;
    uint64_t wallMicros[kPassCount] = {};
    uint64_t cpuMicros[kPassCount] = {};
    uint64_t totalCpuMicros = 0;
    uint32_t lastRangeCount = 0;
};

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
