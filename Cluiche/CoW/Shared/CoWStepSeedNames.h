// CoW's RNG step ids for the world generator (RegionsAndSites StepSeedNames). These are CoW content: changing them
// changes every generated CoW world and invalidates the worldgen goldens.
#pragma once

#include <DraoiHexWorldTerrain/RegionsAndSites.h>

namespace CoW {
namespace WorldGeneration {
using namespace Draoi::HexWorld;


inline StepSeedNames CoWStepSeedNames()
{
    StepSeedNames n;
    n.regions      = Dia::Core::StringCRC("CoW.Regions");
    n.nodeCorridor = Dia::Core::StringCRC("CoW.Node.Corridor");
    n.nodeFill     = Dia::Core::StringCRC("CoW.Node.Fill");
    return n;
}

} // namespace WorldGeneration
} // namespace CoW
