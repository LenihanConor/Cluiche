// Stage-owned side store for the variable-size payloads of mutation-queue entries (WeatherEntry::slot).
// A ring of kSlotCount slots handed out in order. The queue holds at most 128 entries and is FIFO, so a slot still
// referenced by a queued entry is never recycled while kSlotCount > the queue capacity; no per-slot release is needed
// (the base drops entries without telling the hooks when a step is invalid, so release-on-drain would leak).
// SimPU only. Overflow is bounded by the queue: the caller rolls the slot back when Enqueue refuses the entry.
#pragma once

#include <DiaCore/Json/external/json/json.h>
#include <Shared/Weather/WeatherPreset.h>
#include <Shared/Weather/WeatherStateRecord.h>

#include <stdint.h>
#include <vector>

namespace CoW {
namespace Weather {

struct SideSlot
{
    Json::Value pack;                // kParamOverrides
    std::vector<uint32_t> cells;     // kRegionForcing: ascending, de-duplicated
    WeatherPreset preset;            // kPreset
    WeatherStateRecord record;       // kStateReplace
};

class WeatherSideStore
{
public:
    static constexpr uint32_t kSlotCount = 256;   // > the queue capacity (128)

    uint32_t Acquire() { const uint32_t slot = mNext; mNext = (mNext + 1) % kSlotCount; return slot; }
    void Rollback() { mNext = (mNext + kSlotCount - 1) % kSlotCount; }   // the last Acquire was never queued
    SideSlot& Get(uint32_t slot) { return mSlots[slot % kSlotCount]; }
    void Reset()
    {
        mNext = 0;
        for (SideSlot& s : mSlots) { s = SideSlot(); }
    }

private:
    uint32_t mNext = 0;
    SideSlot mSlots[kSlotCount];
};

} // namespace Weather
} // namespace CoW
