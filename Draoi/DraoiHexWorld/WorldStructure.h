// World data types: grid dimensions, Cell, WorldStructure, provenance and the shape parameters recorded in it.
// Plain data (no generator logic); the generator and sims both read these.
#pragma once

#include <DraoiHexWorld/CellVocabulary.h>
#include <DraoiHexWorld/Regions.h>
#include <DraoiHexWorld/Sites.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace CoW {
namespace WorldGeneration {

enum class BarrierForm : uint8_t { Plateau, VolcanicChain };

constexpr uint8_t kNoDirection = 7;

struct WorldShapeParams
{
    float       landFraction      = 0.42f;  // 0.20..0.65
    uint8_t     continents        = 1;      // 1..3
    uint8_t     islands           = 4;      // 0..6
    float       continentIslands  = 0.7f;   // 0..1
    uint8_t     peninsulas        = 2;      // 0..3
    float       coastline         = 0.5f;   // 0..1
    float       mountainousness   = 0.55f;  // 0..1
    float       spineToRing       = 0.15f;  // 0..1
    float       orientationDeg    = 30.0f;  // wraps to [0, 360)
    uint8_t     passes            = 2;      // 0..4
    float       barrier           = 0.6f;   // 0..1
    BarrierForm barrierForm       = BarrierForm::Plateau;
    float       ruggedness        = 0.4f;   // 0..1
    float       rivers            = 0.5f;   // 0..1
    float       valleyDepth       = 0.4f;   // 0..1
    uint8_t     lakes             = 4;      // 0..12
    float       latitudeNorthDeg  = 60.0f;  // -90..90
    float       latitudeSouthDeg  = 20.0f;  // -90..90
    float       temperatureOffset = 0.0f;   // -0.3..0.3
    float       moistureOffset    = 0.0f;   // -0.3..0.3

    void Clamp();
    bool operator==(const WorldShapeParams&) const = default;
};

struct WorldProvenance
{
    uint64_t         seed;
    WorldShapeParams shape;
    char             presetId[33];       // empty if modified or none
    uint32_t         generatorVersion;   // kTerrainGeneratorVersion
    uint32_t         rulesVersion;       // terrain_rules sidecar (a code constant, TerrainRules::rulesVersion)
};

struct GridDimensions
{
    int width  = 0;
    int height = 0;
};

struct Cell
{
    float       elevation    = 0.0f;                  // [0,1]; ocean 0, land >= kCoastElev, lake = surface
    float       latitude     = 0.0f;                  // [0,1]; 0 = equator, 1 = pole
    WaterSource waterSource  = WaterSource::None;
    TerrainType terrain      = TerrainType::DeepWater;
    StratumIndex   stratum    = kNoStratum;               // kNoStratum for ocean
    uint8_t     landmass     = 0;                     // 0 = ocean; 1.. per landmass
    uint8_t     terrainFlags = 0;                     // TerrainFlag bits
    uint8_t     downstream   = kNoDirection;          // river cells: direction 0..5
    World::BiomeIndex biome  = 0;                     // S-1 biome classification (stage 14); registry row. Default 0 = the shipped Ocean row
    RegionId    region       = kNoRegion;             // S-1 regions (0 = ocean / not yet assigned); fills the old tail padding byte
};

// One extra stratum set's assignment (every set of the stratum table except "depth", which lives in Cell::stratum):
// one byte per cell, 0 = none/ocean, 1..N = the set's stratum.
struct ExtraStratumLayer
{
    char                 setId[32] = {};
    std::vector<uint8_t> cells;
    bool operator==(const ExtraStratumLayer&) const = default;
};

// Cell storage is heap-backed (grid sizes 1x1..128x128 exceed any fixed-capacity
// DiaCore container); the public API exposes only pointers and counts.
struct WorldStructure
{
    GridDimensions  dimensions;
    WorldProvenance provenance{};
    uint8_t         mainLandmass = 0;
    RegionTable     regions;   // empty until RegionsAndSites::Place runs
    SiteSet         sites;     // empty until RegionsAndSites::Place runs
    // Extra stratum sets, in table order; empty (zero cost) when the table declares only "depth".
    std::vector<ExtraStratumLayer> extraStrata;

    // The extra layer of the set with this id, nullptr when absent.
    const ExtraStratumLayer* FindExtraStrata(const char* setId) const
    {
        for (const ExtraStratumLayer& l : extraStrata)
        {
            if (setId != nullptr && std::strcmp(l.setId, setId) == 0) { return &l; }
        }
        return nullptr;
    }

    // Resizes to width*height default cells (existing contents are discarded,
    // including regions and sites).
    void Resize(GridDimensions dims)
    {
        dimensions = dims;
        regions = RegionTable{};
        sites.Clear();
        extraStrata.clear();
        const int n = (dims.width > 0 && dims.height > 0) ? dims.width * dims.height : 0;
        mCells.assign(static_cast<size_t>(n), Cell{});
    }

    int         CellCount() const { return static_cast<int>(mCells.size()); }
    Cell*       Cells()           { return mCells.data(); }
    const Cell* Cells()     const { return mCells.data(); }
    Cell&       At(int x, int y)       { return mCells[static_cast<size_t>(y) * dimensions.width + x]; }
    const Cell& At(int x, int y) const { return mCells[static_cast<size_t>(y) * dimensions.width + x]; }

private:
    std::vector<Cell> mCells;
};

} // namespace WorldGeneration
} // namespace CoW
