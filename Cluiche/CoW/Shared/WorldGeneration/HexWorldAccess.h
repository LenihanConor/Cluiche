// Bridge until the terrain extraction (extract-hexworld-terrain.md): the generator code in CoW::WorldGeneration keeps
// unqualified access to the DraoiHexWorld types (Cell, WorldStructure, TerrainType, ...) that moved out of it.
#pragma once

#include <DraoiHexWorld/WorldStructure.h>

namespace CoW {
namespace WorldGeneration {

using namespace Draoi::HexWorld;

} // namespace WorldGeneration
} // namespace CoW
