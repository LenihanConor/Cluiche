// S-1 Terrain Generator: shared terrain vocabulary, shape parameters and the
// terrain part of WorldStructure. Pure CoW-Shared code, no Dia module dependencies
// beyond StringCRC.
// Spec: docs/specs/applications/cow (S-1 World Generation, Terrain Generator)
#pragma once

#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

constexpr uint32_t kTerrainGeneratorVersion = 1;

// ---------------------------------------------------------------------------
// Cell enums
// ---------------------------------------------------------------------------
enum class WaterSource : uint8_t { None = 0, Ocean, Lake, River };
enum class DepthZone   : uint8_t { None = 0, FirstShores, WildReaches, AncientInteriors }; // None = ocean
enum class TerrainType : uint8_t { DeepWater, ShallowWater, Lake, River, Lowland, Hills, Highland,
                                   Mountain, Peak, Cliff, Pass, Volcano, Count };
enum class BarrierForm : uint8_t { Plateau, VolcanicChain };

namespace TerrainFlag
{
    enum : uint8_t { Coast = 1, Ridge = 2, Barrier = 4, Pass = 8, Volcanic = 16, RiverCrossesBarrier = 32 };
}

constexpr uint8_t kNoDirection = 7;

inline const char* WaterSourceName(WaterSource v)
{
    switch (v)
    {
    case WaterSource::None:  return "None";
    case WaterSource::Ocean: return "Ocean";
    case WaterSource::Lake:  return "Lake";
    case WaterSource::River: return "River";
    }
    return "Unknown";
}

inline const char* DepthZoneName(DepthZone v)
{
    switch (v)
    {
    case DepthZone::None:             return "None";
    case DepthZone::FirstShores:      return "FirstShores";
    case DepthZone::WildReaches:      return "WildReaches";
    case DepthZone::AncientInteriors: return "AncientInteriors";
    }
    return "Unknown";
}

inline const char* TerrainTypeName(TerrainType v)
{
    switch (v)
    {
    case TerrainType::DeepWater:    return "DeepWater";
    case TerrainType::ShallowWater: return "ShallowWater";
    case TerrainType::Lake:         return "Lake";
    case TerrainType::River:        return "River";
    case TerrainType::Lowland:      return "Lowland";
    case TerrainType::Hills:        return "Hills";
    case TerrainType::Highland:     return "Highland";
    case TerrainType::Mountain:     return "Mountain";
    case TerrainType::Peak:         return "Peak";
    case TerrainType::Cliff:        return "Cliff";
    case TerrainType::Pass:         return "Pass";
    case TerrainType::Volcano:      return "Volcano";
    case TerrainType::Count:        break;
    }
    return "Unknown";
}

inline const char* BarrierFormName(BarrierForm v)
{
    switch (v)
    {
    case BarrierForm::Plateau:       return "Plateau";
    case BarrierForm::VolcanicChain: return "VolcanicChain";
    }
    return "Unknown";
}

// e.g. "Terrain.Mountain"
Dia::Core::StringCRC TerrainTypeId(TerrainType t);

// ---------------------------------------------------------------------------
// Shape parameters (sliders)
// ---------------------------------------------------------------------------
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

enum class ShapeParam : uint8_t { LandFraction, Continents, Islands, ContinentIslands, Peninsulas, Coastline, Mountainousness,
                                  SpineToRing, Orientation, Passes, Barrier, BarrierForm, Ruggedness,
                                  Rivers, ValleyDepth, Lakes, LatitudeNorth, LatitudeSouth,
                                  TemperatureOffset, MoistureOffset, Count };

enum class TerrainStage : uint8_t { SeedFields, Landmass, Mountains, Barrier, Elevation, Drainage,
                                    Rivers, Lakes, DepthZones, Valleys, Classify, Latitude,
                                    Temperature, Moisture, Biomes, Count };

inline const char* ShapeParamName(ShapeParam v)
{
    switch (v)
    {
    case ShapeParam::LandFraction:       return "LandFraction";
    case ShapeParam::Continents:         return "Continents";
    case ShapeParam::Islands:            return "Islands";
    case ShapeParam::ContinentIslands:   return "ContinentIslands";
    case ShapeParam::Peninsulas:         return "Peninsulas";
    case ShapeParam::Coastline:          return "Coastline";
    case ShapeParam::Mountainousness:    return "Mountainousness";
    case ShapeParam::SpineToRing:        return "SpineToRing";
    case ShapeParam::Orientation:        return "Orientation";
    case ShapeParam::Passes:             return "Passes";
    case ShapeParam::Barrier:            return "Barrier";
    case ShapeParam::BarrierForm:        return "BarrierForm";
    case ShapeParam::Ruggedness:         return "Ruggedness";
    case ShapeParam::Rivers:             return "Rivers";
    case ShapeParam::ValleyDepth:        return "ValleyDepth";
    case ShapeParam::Lakes:              return "Lakes";
    case ShapeParam::LatitudeNorth:      return "LatitudeNorth";
    case ShapeParam::LatitudeSouth:      return "LatitudeSouth";
    case ShapeParam::TemperatureOffset:  return "TemperatureOffset";
    case ShapeParam::MoistureOffset:     return "MoistureOffset";
    case ShapeParam::Count:              break;
    }
    return "Unknown";
}

inline const char* TerrainStageName(TerrainStage v)
{
    switch (v)
    {
    case TerrainStage::SeedFields:  return "SeedFields";
    case TerrainStage::Landmass:    return "Landmass";
    case TerrainStage::Mountains:   return "Mountains";
    case TerrainStage::Barrier:     return "Barrier";
    case TerrainStage::Elevation:   return "Elevation";
    case TerrainStage::Drainage:    return "Drainage";
    case TerrainStage::Rivers:      return "Rivers";
    case TerrainStage::Lakes:       return "Lakes";
    case TerrainStage::DepthZones:  return "DepthZones";
    case TerrainStage::Valleys:     return "Valleys";
    case TerrainStage::Classify:    return "Classify";
    case TerrainStage::Latitude:    return "Latitude";
    case TerrainStage::Temperature: return "Temperature";
    case TerrainStage::Moisture:    return "Moisture";
    case TerrainStage::Biomes:      return "Biomes";
    case TerrainStage::Count:       break;
    }
    return "Unknown";
}

// First pipeline stage that must re-run when the given slider changes.
TerrainStage FirstStageAffected(ShapeParam p);

// ---------------------------------------------------------------------------
// World structure (terrain part)
// ---------------------------------------------------------------------------
struct WorldProvenance
{
    uint64_t         seed;
    WorldShapeParams shape;
    char             presetId[33];       // empty if modified or none
    uint32_t         generatorVersion;   // kTerrainGeneratorVersion
    uint32_t         rulesVersion;       // terrain_rules.json
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
    DepthZone   depthZone    = DepthZone::None;       // None for ocean
    uint8_t     landmass     = 0;                     // 0 = ocean; 1.. per landmass
    uint8_t     terrainFlags = 0;                     // TerrainFlag bits
    uint8_t     downstream   = kNoDirection;          // river cells: direction 0..5
};

// Cell storage is heap-backed (grid sizes 1x1..128x128 exceed any fixed-capacity
// DiaCore container); the public API exposes only pointers and counts.
struct WorldStructure
{
    GridDimensions  dimensions;
    WorldProvenance provenance{};
    uint8_t         mainLandmass = 0;

    // Resizes to width*height default cells (existing contents are discarded).
    void Resize(GridDimensions dims)
    {
        dimensions = dims;
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
