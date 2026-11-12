// S-1 World Generation map view: descriptor table and readers that feed the DiaCellLayers viewer.
// Pure CoW-Shared code: each reader copies one generated layer from a WorldGenSession into a caller-supplied
// float buffer of width*height (uint layers widened to float, Cell fields strided out of WorldStructure).
// Landmass ids (0..255) exceed the 64-colour category palette, so landmass is a Float layer with a ramp.
// Region ids (0..RegionTable::kMaxRegions = 64) need 65 values, one more than the palette: the "regions" layer writes
// RegionPaletteSlot(id), which keeps 0 (ocean / regions not fresh) and cycles ids 1..64 over slots 1..63 (only id 64
// shares a colour, with id 1). Regions are empty (all 0) until a Regenerate (regionsFresh), so the layer is then all slot 0.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#pragma once

#include <Shared/WorldGeneration/WorldGenSession.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

enum class WorldLayerKind : uint8_t { Float, Category };

constexpr uint32_t kRegionPaletteSlots = 64;   // CellField palette size: "none" + 63 region slots

// 0 -> 0; id >= 1 -> 1 + (id - 1) % 63.
constexpr uint32_t RegionPaletteSlot(RegionId id)
{
    return id == kNoRegion ? 0u : 1u + (static_cast<uint32_t>(id) - 1u) % (kRegionPaletteSlots - 1u);
}

// Fills out[0..count) from the session; zero-fills when no world has been generated yet.
using WorldLayerReadFn = void (*)(const WorldGenSession& session, float* out, uint32_t count);

struct WorldLayerDesc
{
    const char*      id;              // layer id and schema entry key, e.g. "terrainType"
    const char*      schemaKey;       // full schema key: "<kWorldLayerSchemaFor>/<id>"
    const char*      group;           // viewer picker group
    WorldLayerKind   kind;
    uint32_t         categoryCount;   // enum value count for Category layers, 0 for Float
    WorldLayerReadFn read;
    bool             defaultBase;     // the viewer's default base layer
};

// Schema file "for" value; the layer schema keys are "<for>/<id>".
inline constexpr const char* kWorldLayerSchemaFor = "cow.worldgen.layers";

class WorldLayerSource
{
public:
    static uint32_t              Count();
    static const WorldLayerDesc& At(uint32_t index);
    // nullptr when the id is unknown.
    static const WorldLayerDesc* Find(const char* id);

    // The version stamp of the data the readers return: changes only when a generation changed the world.
    static uint32_t Version(const WorldGenSession& session) { return session.Revision(); }
};

} // namespace WorldGeneration
} // namespace CoW
