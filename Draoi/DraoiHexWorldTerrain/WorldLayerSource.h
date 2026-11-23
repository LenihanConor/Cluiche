// S-1 World Generation map view: descriptor table and readers that feed the DiaCellLayers viewer.
// Pure library code: each reader copies one generated layer from a WorldGenSession into a caller-supplied
// float buffer of width*height (uint layers widened to float, Cell fields strided out of WorldStructure).
// Landmass ids (0..255) exceed the 64-colour category palette, so landmass is a Float layer with a ramp.
// Region ids (0..RegionTable::kMaxRegions = 64) need 65 values, one more than the palette: the "regions" layer writes
// RegionPaletteSlot(id), which keeps 0 (ocean / regions not fresh) and cycles ids 1..64 over slots 1..63 (only id 64
// shares a colour, with id 1). Regions are empty (all 0) until a Regenerate (regionsFresh), so the layer is then all slot 0.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#pragma once

#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorld/WorldStructure.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Draoi {
namespace HexWorld {

enum class WorldLayerKind : uint8_t { Float, Category };

constexpr uint32_t kRegionPaletteSlots = 64;   // CellField palette size: "none" + 63 region slots

// 0 -> 0; id >= 1 -> 1 + (id - 1) % 63.
constexpr uint32_t RegionPaletteSlot(RegionId id)
{
    return id == kNoRegion ? 0u : 1u + (static_cast<uint32_t>(id) - 1u) % (kRegionPaletteSlots - 1u);
}

// Fills out[0..count) from the session; zero-fills when no world has been generated yet.
using WorldLayerReadFn = void (*)(const WorldGenSession& session, float* out, uint32_t count);

// WorldLayerDesc::categoryCount value for a layer whose category count is the biome registry's row count (biomeIndex).
// Resolve it with WorldLayerSource::CategoryCount.
constexpr uint32_t kCategoriesFromBiomeRegistry = 0;

struct WorldLayerDesc
{
    const char*      id;              // layer id, "worldgen.<snake_case field>", e.g. "worldgen.terrain_type"
    const char*      schemaKey;       // full schema key: "<kWorldLayerSchemaFor>/<snake_case field>"
    const char*      group;           // viewer picker group
    WorldLayerKind   kind;
    uint32_t         categoryCount;   // enum value count for Category layers, 0 for Float (or kCategoriesFromBiomeRegistry)
    WorldLayerReadFn read;
    bool             defaultBase;     // the viewer's default base layer
};

// Layer ids are "<kWorldLayerIdPrefix><field>"; field is snake_case.
inline constexpr const char* kWorldLayerIdPrefix = "worldgen.";

// Schema file "for" value; the layer schema keys are "<for>/<field>".
inline constexpr const char* kWorldLayerSchemaFor = "cow.worldgen.layers";

// Extra stratum sets (every set of the stratum table except "depth") are dynamic Category layers: one per set, id
// "worldgen.strata.<set_id>" (set id converted to snake_case), group "Strata". They share ONE schema entry
// (kExtraStrataSchemaKey) for their palette: None plus up to StratumTable::kMaxStrata strata, one colour each, so a
// stratum has the same colour in every set. Count() / At() / Find() stay the static layer table; the extras are listed
// per stratum table.
inline constexpr const char* kExtraStrataSchemaKey = "cow.worldgen.layers/strata_set";
inline constexpr const char* kExtraStrataIdPrefix  = "worldgen.strata.";
inline constexpr const char* kExtraStrataGroup     = "Strata";

struct ExtraStrataLayerDesc
{
    std::string id;            // "worldgen.strata.<set_id>"
    std::string setId;         // the stratum table's raw set id
    uint32_t    categoryCount; // set.count + 1 (None + the strata)
};

class WorldLayerSource
{
public:
    // One entry per non-depth set of the table, in table order; empty for a null registry or a depth-only table.
    static std::vector<ExtraStrataLayerDesc> ExtraStrataLayers(const Draoi::HexWorld::BiomeRegistry* biomes);
    // Fills out[0..count) with the set's stratum per cell; zero-fills when no world is generated or the world has no layer for the set.
    static void ReadExtraStrata(const WorldGenSession& session, const char* setId, float* out, uint32_t count);

    static uint32_t              Count();
    static const WorldLayerDesc& At(uint32_t index);
    // nullptr when the id is unknown.
    static const WorldLayerDesc* Find(const char* id);

    // Category count of a layer for the given biome registry: the registry's Count() for the registry-sized biomeIndex layer,
    // desc.categoryCount for every other layer (0 for Float layers). The descriptor table stays static; only this count is
    // resolved per registry.
    static uint32_t CategoryCount(const WorldLayerDesc& desc, const Draoi::HexWorld::BiomeRegistry& biomes)
    {
        return desc.kind == WorldLayerKind::Category && desc.categoryCount == kCategoriesFromBiomeRegistry ? biomes.Count() : desc.categoryCount;
    }

    // The version stamp of the data the readers return: changes only when a generation changed the world.
    static uint32_t Version(const WorldGenSession& session) { return session.Revision(); }
};

} // namespace HexWorld
} // namespace Draoi
