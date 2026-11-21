#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cmath>
#include <cstdio>

namespace CoW {
namespace WorldGeneration {

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

void WorldShapeParams::Clamp()
{
    landFraction      = ClampValue(landFraction, 0.20f, 0.65f);
    continents        = ClampValue<uint8_t>(continents, 1, 3);
    islands           = ClampValue<uint8_t>(islands, 0, 6);
    continentIslands  = ClampValue(continentIslands, 0.0f, 1.0f);
    peninsulas        = ClampValue<uint8_t>(peninsulas, 0, 3);
    coastline         = ClampValue(coastline, 0.0f, 1.0f);
    mountainousness   = ClampValue(mountainousness, 0.0f, 1.0f);
    spineToRing       = ClampValue(spineToRing, 0.0f, 1.0f);
    passes            = ClampValue<uint8_t>(passes, 0, 4);
    barrier           = ClampValue(barrier, 0.0f, 1.0f);
    ruggedness        = ClampValue(ruggedness, 0.0f, 1.0f);
    rivers            = ClampValue(rivers, 0.0f, 1.0f);
    valleyDepth       = ClampValue(valleyDepth, 0.0f, 1.0f);
    lakes             = ClampValue<uint8_t>(lakes, 0, 12);
    latitudeNorthDeg  = ClampValue(latitudeNorthDeg, -90.0f, 90.0f);
    latitudeSouthDeg  = ClampValue(latitudeSouthDeg, -90.0f, 90.0f);
    temperatureOffset = ClampValue(temperatureOffset, -0.3f, 0.3f);
    moistureOffset    = ClampValue(moistureOffset, -0.3f, 0.3f);

    // Wrap into [0, 360). A tiny negative input can round up to exactly 360.
    float wrapped = std::fmod(orientationDeg, 360.0f);
    if (wrapped < 0.0f)
    {
        wrapped += 360.0f;
    }
    if (!(wrapped < 360.0f))
    {
        wrapped = 0.0f;
    }
    orientationDeg = wrapped;
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

} // namespace WorldGeneration
} // namespace CoW
