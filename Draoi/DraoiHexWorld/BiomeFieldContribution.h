// Biome field contribution: registers biome.index and one biome.tag.<tag> per registry tag into a WorldCellFields.
// Spec: docs/specs/features/draoi/worldsim-kit-draoi.md (AC 10a). Plain function, no bus/event/module knowledge:
// who calls Clear and re-registers (and when) is the application's.
// Slot "biome"; fields "index" and "tag.<tag>" (tags have both a bool and a 1.0/0.0 float reader).
// Lifetime: the caller keeps the BiomeFieldSource, the BiomeRegistry and the WorldStructure alive and at a fixed address
// until WorldCellFields::Clear (the registered readers point into the source).
#pragma once

#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorld/WorldCellFields.h>
#include <DraoiHexWorld/WorldStructure.h>

#include <cstdint>

namespace Draoi {
namespace HexWorld {

class BiomeFieldSource
{
public:
    static constexpr uint32_t kMaxTags = 64;

    BiomeFieldSource(const BiomeRegistry& biomes, const WorldStructure& world) : mBiomes(&biomes), mWorld(&world) {}
    BiomeFieldSource(const BiomeFieldSource&) = delete;
    BiomeFieldSource& operator=(const BiomeFieldSource&) = delete;

    struct TagBinding
    {
        const BiomeFieldSource* owner = nullptr;
        TagId                   tag   = kNoTag;
    };

    const BiomeRegistry&  Biomes() const { return *mBiomes; }
    const WorldStructure& World() const  { return *mWorld; }
    TagBinding&           Binding(uint32_t i) { return mTags[i]; }

private:
    const BiomeRegistry*  mBiomes;
    const WorldStructure* mWorld;
    TagBinding            mTags[kMaxTags];
};

// Registers biome.index and biome.tag.<tag> for every tag of the registry's tag vocabulary. Re-register after Clear to refresh.
void RegisterBiomeFields(WorldCellFields& fields, BiomeFieldSource& source);

} // namespace HexWorld
} // namespace Draoi
