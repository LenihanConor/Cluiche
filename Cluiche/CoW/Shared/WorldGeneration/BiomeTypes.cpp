#include <Shared/WorldGeneration/BiomeTypes.h>

namespace CoW {
namespace WorldGeneration {

namespace
{
    const char* BiomeIdString(Biome b)
    {
        switch (b)
        {
        case Biome::Ocean:           return "Biome.Ocean";
        case Biome::Lake:            return "Biome.Lake";
        case Biome::GrasslandSteppe: return "Biome.Grassland";
        case Biome::TemperateForest: return "Biome.TemperateForest";
        case Biome::WetlandDelta:    return "Biome.Wetland";
        case Biome::DesertBasin:     return "Biome.Desert";
        case Biome::AlpineMountain:  return "Biome.Alpine";
        case Biome::TundraBoreal:    return "Biome.Tundra";
        case Biome::Rainforest:      return "Biome.Rainforest";
        case Biome::VolcanicAshland: return "Biome.Volcanic";
        case Biome::Scrub:             return "Biome.Scrub";
        case Biome::BurnedAshland:     return "Biome.BurnedAshland";
        case Biome::DesertifiedPlain:  return "Biome.DesertifiedPlain";
        case Biome::Count:           break;
        }
        return "Biome.Unknown";
    }

    bool IsPair(Biome a, Biome b, Biome x, Biome y)
    {
        return (a == x && b == y) || (a == y && b == x);
    }
}

Dia::Core::StringCRC BiomeId(Biome b)
{
    return Dia::Core::StringCRC(BiomeIdString(b));
}

const char* BiomeName(Biome b)
{
    switch (b)
    {
    case Biome::Ocean:           return "Ocean";
    case Biome::Lake:            return "Lake";
    case Biome::GrasslandSteppe: return "Grassland / Steppe";
    case Biome::TemperateForest: return "Temperate Forest";
    case Biome::WetlandDelta:    return "Wetland / Delta";
    case Biome::DesertBasin:     return "Desert Basin";
    case Biome::AlpineMountain:  return "Alpine Mountain";
    case Biome::TundraBoreal:    return "Tundra / Boreal";
    case Biome::Rainforest:      return "Rainforest";
    case Biome::VolcanicAshland: return "Volcanic Ashland";
    case Biome::Scrub:            return "Scrub";
    case Biome::BurnedAshland:    return "Burned Ashland";
    case Biome::DesertifiedPlain: return "Desertified Plain";
    case Biome::Count:           break;
    }
    return "Unknown";
}

bool BiomeFromId(Dia::Core::StringCRC id, Biome& out)
{
    for (int i = 0; i < kBiomeCount; ++i)
    {
        const Biome b = static_cast<Biome>(i);
        if (BiomeId(b) == id)
        {
            out = b;
            return true;
        }
    }
    return false;
}

bool IsBiomeValidInZone(Biome b, DepthZone z)
{
    switch (b)
    {
    case Biome::Ocean:
    case Biome::Lake:
        return z == DepthZone::None;
    case Biome::GrasslandSteppe:
    case Biome::TemperateForest:
    case Biome::WetlandDelta:
        return z == DepthZone::FirstShores || z == DepthZone::WildReaches;
    case Biome::DesertBasin:
    case Biome::AlpineMountain:
    case Biome::TundraBoreal:
    case Biome::Rainforest:
    case Biome::VolcanicAshland:
        return z == DepthZone::WildReaches || z == DepthZone::AncientInteriors;
    // Succession biomes are classification predicates: the generator never places them, so no zone admits them.
    case Biome::Scrub:
    case Biome::BurnedAshland:
    case Biome::DesertifiedPlain:
    case Biome::Count:
        break;
    }
    return false;
}

bool AreBiomesForbidden(Biome a, Biome b)
{
    return IsPair(a, b, Biome::VolcanicAshland, Biome::WetlandDelta)
        || IsPair(a, b, Biome::VolcanicAshland, Biome::Rainforest)
        || IsPair(a, b, Biome::VolcanicAshland, Biome::TundraBoreal)
        || IsPair(a, b, Biome::Rainforest, Biome::DesertBasin)
        || IsPair(a, b, Biome::Rainforest, Biome::TundraBoreal)
        || IsPair(a, b, Biome::TundraBoreal, Biome::WetlandDelta)
        || IsPair(a, b, Biome::DesertBasin, Biome::TemperateForest);
}

} // namespace WorldGeneration
} // namespace CoW
