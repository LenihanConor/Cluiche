// A4 weather forcings (scripted overrides), cell sets and grid totals.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md ("Forcings", "Grid totals", Public API)
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArray.h>

#include <cstdint>

namespace Draoi {
namespace HexWorld {
namespace Weather {

enum class ForcingKind : uint8_t { PressureHold, MoistureHold, Rain, WindOverride };
constexpr uint32_t kForcingKindCount = 4;

// A forcing targets one cell, or a registered cell set (cellSet != kNoCellSet, then `cell` is ignored).
using CellSetHandle = uint16_t;
constexpr CellSetHandle kNoCellSet = 0xFFFF;

constexpr uint32_t kMaxForcings = 64;
constexpr uint32_t kMaxCellSets = 16;

// POD record. WindOverride: value = radians, value2 = speed; weeksRemaining 0 = until cleared. Every other kind needs
// weeksRemaining >= 1 (AddForcing rejects 0).
struct WeatherForcing
{
    ForcingKind   kind = ForcingKind::PressureHold;
    uint32_t      cell = 0;
    CellSetHandle cellSet = kNoCellSet;
    float         value = 0.0f;
    float         value2 = 0.0f;
    uint16_t      weeksRemaining = 0;
};

// Registered cell sets: up to kMaxCellSets sets of at most CellCount() cells each, storage allocated once at Initialise.
struct CellSetStore
{
    Dia::Core::Containers::DynamicArray<uint32_t> cells;     // kMaxCellSets * N, set s starts at s * N
    uint32_t counts[kMaxCellSets] = {};
    uint32_t used = 0;
};

// Serial reduction in cell order at the end of Run; never feeds back into the simulation.
struct WeatherGridTotals
{
    double   meanMoisture = 0.0;
    double   totalSnowPack = 0.0;
    uint32_t rainingCellCount = 0;
};

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
