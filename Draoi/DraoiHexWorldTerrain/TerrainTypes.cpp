#include <DraoiHexWorldTerrain/TerrainTypes.h>

#include <cmath>
#include <cstdio>

namespace Draoi {
namespace HexWorld {

namespace
{
    template <typename T>
    T ClampValue(T v, T lo, T hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }
}

Dia::Core::StringCRC TerrainTypeId(TerrainType t)
{
    char buffer[Dia::Core::StringCRC::kStringLength];
    std::snprintf(buffer, sizeof(buffer), "Terrain.%s", TerrainTypeName(t));
    return Dia::Core::StringCRC(buffer);
}

TerrainStage FirstStageAffected(ShapeParam p)
{
    switch (p)
    {
    case ShapeParam::LandFraction:
    case ShapeParam::Continents:
    case ShapeParam::Islands:
    case ShapeParam::ContinentIslands:
    case ShapeParam::Peninsulas:
    case ShapeParam::Coastline:         return TerrainStage::Landmass;
    case ShapeParam::Mountainousness:
    case ShapeParam::SpineToRing:
    case ShapeParam::Orientation:
    case ShapeParam::Passes:            return TerrainStage::Mountains;
    case ShapeParam::Barrier:
    case ShapeParam::BarrierForm:       return TerrainStage::Barrier;
    case ShapeParam::Ruggedness:        return TerrainStage::Elevation;
    case ShapeParam::Rivers:            return TerrainStage::Rivers;
    case ShapeParam::Lakes:             return TerrainStage::Lakes;
    case ShapeParam::ValleyDepth:       return TerrainStage::Valleys;
    case ShapeParam::LatitudeNorth:
    case ShapeParam::LatitudeSouth:     return TerrainStage::Latitude;
    case ShapeParam::TemperatureOffset: return TerrainStage::Temperature;
    case ShapeParam::MoistureOffset:    return TerrainStage::Moisture;
    case ShapeParam::Count:             break;
    }
    return TerrainStage::SeedFields;
}

} // namespace HexWorld
} // namespace Draoi
