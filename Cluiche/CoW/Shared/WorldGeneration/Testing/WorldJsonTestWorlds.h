// Test worlds for the S-1 JSON export/import tests (header-only).
// MakeTinyWorld(): 4x3 hand-built world that uses every cell enum value that fits, two regions, built-in sites and a
// test kind. Region summaries are consistent with the cells (RecomputeRegionRecords).
// GenerateWorld(): a world generated through WorldGenSession (Regenerate => regions and sites present).
#pragma once

#include "../RegionRepair.h"
#include "../WorldGenSession.h"
#include "../WorldJson.h"
#include <Shared/World/Testing/BiomeTestRegistry.h>

#include <DiaMaths/Core/RandomStream.h>

#include <cstring>
#include <string>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

// Legacy-enum value of a registry biome id (Cell::biome is still the enum; Task 27 removes the bridge).
// An import context that carries the shipped biome registry (import requires one).
inline WorldJsonContext TestImportContext()
{
    WorldJsonContext c;
    c.biomes = &::CoW::World::Testing::ShippedBiomeRegistry();
    return c;
}

inline Biome BL(const char* id) { return ::CoW::World::Testing::LegacyB(id); }

inline Cell MakeCell(Biome biome, TerrainType terrain, DepthZone zone, WaterSource water, float elevation, float latitude,
                     RegionId region, uint8_t landmass, uint8_t flags = 0, uint8_t downstream = kNoDirection)
{
    Cell c;
    std::memset(&c, 0, sizeof(Cell));
    c.biome = biome; c.terrain = terrain; c.depthZone = zone; c.waterSource = water;
    c.elevation = elevation; c.latitude = latitude; c.region = region; c.landmass = landmass;
    c.terrainFlags = flags; c.downstream = downstream;
    return c;
}

// Bytewise identity of two worlds' cell arrays (the round-trip contract: no tolerance).
inline bool SameCells(const WorldStructure& a, const WorldStructure& b)
{
    return a.dimensions.width == b.dimensions.width && a.dimensions.height == b.dimensions.height &&
           a.CellCount() == b.CellCount() &&
           std::memcmp(a.Cells(), b.Cells(), sizeof(Cell) * static_cast<size_t>(a.CellCount())) == 0;
}

inline bool SameProvenance(const WorldProvenance& a, const WorldProvenance& b)
{
    return a.seed == b.seed && a.shape == b.shape && std::strcmp(a.presetId, b.presetId) == 0 &&
           a.generatorVersion == b.generatorVersion && a.rulesVersion == b.rulesVersion;
}

// Everything the schema carries.
inline bool SameWorld(const WorldStructure& a, const WorldStructure& b)
{
    return SameCells(a, b) && a.mainLandmass == b.mainLandmass && a.regions == b.regions && a.sites == b.sites &&
           SameProvenance(a.provenance, b.provenance);
}

inline WorldStructure MakeTinyWorld()
{
    WorldStructure w;
    w.Resize(GridDimensions{ 4, 3 });
    w.mainLandmass = 1;
    w.provenance = WorldProvenance{};
    w.provenance.seed = 0x1234567890ABCDEFull;
    w.provenance.shape.landFraction = 0.35f;
    w.provenance.shape.barrierForm = BarrierForm::VolcanicChain;
    w.provenance.shape.orientationDeg = 123.5f;
    w.provenance.generatorVersion = kTerrainGeneratorVersion;
    w.provenance.rulesVersion = 4;
    std::strcpy(w.provenance.presetId, "tiny");

    Cell* c = w.Cells();
    //            biome                    terrain                  zone                       water                elev    lat   reg lm
    c[0]  = MakeCell(BL("Biome.Ocean"),           TerrainType::DeepWater,    DepthZone::None,             WaterSource::Ocean, 0.0f,   0.1f, 0, 0);
    c[1]  = MakeCell(BL("Biome.Ocean"),           TerrainType::ShallowWater, DepthZone::None,             WaterSource::Ocean, 0.0f,   0.2f, 0, 0);
    c[2]  = MakeCell(BL("Biome.Grassland"), TerrainType::Lowland,      DepthZone::FirstShores,      WaterSource::None,  0.35f,  0.3f, 1, 1, TerrainFlag::Coast);
    c[3]  = MakeCell(BL("Biome.TemperateForest"), TerrainType::Hills,        DepthZone::FirstShores,      WaterSource::None,  0.5f,   0.4f, 1, 1);
    c[4]  = MakeCell(BL("Biome.Lake"),            TerrainType::Lake,         DepthZone::WildReaches,      WaterSource::Lake,  0.45f,  0.5f, 1, 1);
    c[5]  = MakeCell(BL("Biome.Wetland"),    TerrainType::River,        DepthZone::WildReaches,      WaterSource::River, 0.41f,  0.6f, 1, 1, 0, 3);
    c[6]  = MakeCell(BL("Biome.Alpine"),  TerrainType::Mountain,     DepthZone::WildReaches,      WaterSource::None,  0.9f,   0.7f, 2, 1, TerrainFlag::Ridge | TerrainFlag::Barrier);
    c[7]  = MakeCell(BL("Biome.Desert"),     TerrainType::Highland,     DepthZone::AncientInteriors, WaterSource::None,  0.7f,   0.8f, 2, 1);
    c[8]  = MakeCell(BL("Biome.Tundra"),    TerrainType::Peak,         DepthZone::AncientInteriors, WaterSource::None,  1.0f,   0.9f, 2, 1);
    c[9]  = MakeCell(BL("Biome.Rainforest"),      TerrainType::Pass,         DepthZone::AncientInteriors, WaterSource::None,  0.8f,   1.0f, 2, 1, TerrainFlag::Pass);
    c[10] = MakeCell(BL("Biome.Volcanic"), TerrainType::Volcano,      DepthZone::AncientInteriors, WaterSource::None,  0.85f,  0.0f, 2, 1, TerrainFlag::Volcanic);
    c[11] = MakeCell(BL("Biome.Grassland"), TerrainType::Cliff,        DepthZone::WildReaches,      WaterSource::None,  0.1f,   1.0f / 3.0f, 0, 2);

    RegionRecord r1;
    r1.id = 1; r1.zone = DepthZone::FirstShores; r1.landmass = 1; r1.isIsland = false; r1.primaryBiome = BL("Biome.Grassland");
    r1.purity = 1.0f / 4.0f; r1.cellCount = 4; r1.centreCell = 3; r1.neighbours.Add(2);
    RegionRecord r2;
    r2.id = 2; r2.zone = DepthZone::AncientInteriors; r2.landmass = 1; r2.isIsland = false; r2.primaryBiome = BL("Biome.Alpine");
    r2.purity = 1.0f / 5.0f; r2.cellCount = 5; r2.centreCell = 8; r2.neighbours.Add(1);
    w.regions.AddRegion(r1);
    w.regions.AddRegion(r2);
    w.regions.absorbedCells = 2;
    w.regions.fragmentCells = 1;
    w.regions.limitedBy = DepthZone::WildReaches;
    RecomputeRegionRecords(w, ::CoW::World::Testing::ShippedBiomeRegistry());   // summaries consistent with the cells: import recomputes them with the same code

    auto add = [&](const Dia::Core::StringCRC& kind, uint32_t cell, RegionId region, uint16_t variant, uint16_t flags)
    {
        Site s; s.kindCrc = SiteKindId(kind); s.cell = cell; s.region = region; s.variant = variant; s.flags = flags;
        return w.sites.Add(s);
    };
    add(BuiltinNodeKinds::Corridor(), 6, 2, 0, 0x0105);
    add(BuiltinNodeKinds::Corridor(), 9, 2, 1, 0x0002);
    add(BuiltinNodeKinds::River(), 5, 1, 0, 0);
    add(BuiltinNodeKinds::Hunting(), 2, 1, 0, 0);
    add(BuiltinNodeKinds::Resource(), 3, 1, 7, 65535);
    add(Dia::Core::StringCRC("Test.Kind"), 7, 2, 3, 9);
    return w;
}

// Same as MakeTinyWorld plus a site of a kind nobody registers (its name is remembered so it exports by name).
inline WorldStructure MakeTinyWorldWithUnknownKind(const char* kindName = "Dragon.Hearth")
{
    WorldStructure w = MakeTinyWorld();
    Site s; s.kindCrc = Dia::Core::StringCRC(kindName).Value(); s.cell = 8; s.region = 2; s.variant = 4; s.flags = 1;
    w.sites.Add(s);
    w.sites.RememberKindName(s.kindCrc, kindName);
    return w;
}

// A world produced by the session the way the commands do: Regenerate then Tick. A non-empty `shapeArgs` object
// is applied with Set first (shape keys, "seed").
inline WorldStructure GenerateWorld(uint64_t seed, const Json::Value& shapeArgs = Json::Value(Json::objectValue),
                                    const SiteRegistry* registry = nullptr)
{
    TestSession s;
    if (registry != nullptr) { s.SetSiteRegistry(registry); }
    Dia::Maths::RandomStream rng{ 1 };
    if (shapeArgs.isObject() && !shapeArgs.getMemberNames().empty()) { s.Set(shapeArgs); }
    Json::Value regen(Json::objectValue);
    regen["seed"] = static_cast<Json::UInt64>(seed);
    s.Regenerate(regen);
    s.Tick(rng);
    return s.World();
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
