// The tagged mutation-queue entry of the weather stage (A4 WeatherModule, gap-pass amendment 1).
// Design: a flat, trivially copyable struct with a fixed payload. Variable-size payloads (a parameter-overrides Json
// object, a decoded state record, the cell list of a region forcing, a preset) are NOT stored inline: the Queue* API
// parks them in a small stage-owned side store and the entry carries only the slot handle in `slot`. This keeps the
// queue (capacity 128) bounded and the entry copyable; the side store arrives with the Queue* API in plan Task 4.
#pragma once

#include <DraoiHexWorld/CellVocabulary.h>
#include <DraoiHexWorldWeather/WeatherForcing.h>

#include <stdint.h>

namespace CoW {
namespace Weather {

enum class EntryKind : uint8_t
{
    kNone = 0,
    kSetParam,            // key + value
    kParamOverrides,      // slot -> side store (Json object), applied atomically
    kReload,              // re-read the sidecar
    kForcing,             // forcing
    kRegionForcing,       // forcing + slot -> side store (sorted cell list)
    kBiomeChange,         // cell + biome
    kPreset,              // slot -> side store (WeatherPreset)
    kStateReplace,        // slot -> side store (decoded weather-state record)
    kClearWindOverrides,
};

constexpr uint32_t kEntryKeyCapacity = 64;
constexpr uint32_t kNoSlot = 0xFFFFFFFFu;

struct WeatherEntry
{
    EntryKind kind = EntryKind::kNone;
    char      key[kEntryKeyCapacity] = {};   // kSetParam dotted key, NUL-terminated
    float     value = 0.0f;                  // kSetParam value
    uint32_t  cell = 0;                      // kBiomeChange
    Draoi::HexWorld::BiomeIndex biome = Draoi::HexWorld::kNoBiome;   // kBiomeChange
    uint32_t  slot = kNoSlot;                // side-store handle (kParamOverrides, kRegionForcing, kPreset, kStateReplace)
    Draoi::HexWorld::Weather::WeatherForcing forcing;   // kForcing, kRegionForcing
};

} // namespace Weather
} // namespace CoW
