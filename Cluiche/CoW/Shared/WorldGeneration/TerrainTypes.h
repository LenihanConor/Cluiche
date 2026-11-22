// S-1 Terrain Generator: shared terrain vocabulary, shape parameters and the
// terrain part of WorldStructure. Pure library code, no Dia module dependencies
// beyond StringCRC.
// Spec: docs/specs/applications/cow (S-1 World Generation, Terrain Generator)
#pragma once

#include <DiaCore/CRC/StringCRC.h>

#include <DraoiHexWorld/StratumTable.h>

#include <Shared/WorldGeneration/HexWorldAccess.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace CoW {
namespace WorldGeneration {

constexpr uint32_t kTerrainGeneratorVersion = 1;

// ---------------------------------------------------------------------------
// Cell enums
// ---------------------------------------------------------------------------
// (enums live in DraoiHexWorld/CellVocabulary.h and WorldStructure.h)


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

// Stratum name from the table ("None" for 0); "Unknown" for an index the table does not have.
inline const char* StratumName(const Draoi::HexWorld::StratumTable& strata, StratumIndex v)
{
    const char* name = strata.Name(v);
    return name != nullptr ? name : "Unknown";
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
enum class ShapeParam : uint8_t { LandFraction, Continents, Islands, ContinentIslands, Peninsulas, Coastline, Mountainousness,
                                  SpineToRing, Orientation, Passes, Barrier, BarrierForm, Ruggedness,
                                  Rivers, ValleyDepth, Lakes, LatitudeNorth, LatitudeSouth,
                                  TemperatureOffset, MoistureOffset, Count };

enum class TerrainStage : uint8_t { SeedFields, Landmass, Mountains, Barrier, Elevation, Drainage,
                                    Rivers, Lakes, Strata, Valleys, Classify, Latitude,
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
    case TerrainStage::Strata:  return "Strata";
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
} // namespace WorldGeneration
} // namespace CoW
