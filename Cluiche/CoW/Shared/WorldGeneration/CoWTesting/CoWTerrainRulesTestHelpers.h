// CoW half of the S-1 test helper: the shipped CoW terrain rules and a ready-to-use CoW session for tests. The neutral
// half (file lookup, Fail) is Testing/TerrainRulesTestHelpers.h.
// Production has no compiled default rules; the rules come from the terrain_rules sidecar
// (Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json). Tests read that file from the source tree once per
// process (the cache below lives in this Testing helper only, never in production code).
//   TestRules()          a TerrainRules at the sidecar defaults
//   TestSession          a WorldGenSession with the sidecar rules loaded and the shipped guarantee spec installed
//   TestSidecarText()    the sidecar text (for LoadRules / LoadFromJson / the module seam)
//   TestSidecarFile()    the sidecar loaded as a ParameterFile (rule metadata: names, ranges, defaults)
// Header-only. Aborts with a message if the sidecar cannot be found or loaded (a broken checkout, not a test result).
#pragma once

#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldTerrain/Testing/TerrainRulesTestHelpers.h>
#include <DraoiHexWorldTerrain/Testing/TerrainTestHost.h>
#include <Shared/CoWNodeKinds.h>
#include <Shared/CoWStepSeedNames.h>
#include <DraoiHexWorld/WorldStructure.h>
#include <DraoiHexWorldTerrain/TerrainRules.h>
#include <Shared/WorldGeneration/CoWTesting/GuaranteeSpecTestHelpers.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>

#include <DiaAttribute/ParameterFile.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace CoW {
namespace WorldGeneration {
using namespace Draoi::HexWorld;


// Forwarders to the neutral host (DraoiHexWorldTerrain/Testing/TerrainTestHost.h); the CoW values are installed by
// SetTerrainTestConfig in the GoogleTests CoWTestAssetPaths.cpp.
using ::Draoi::HexWorld::Testing::TestSidecarText;
using ::Draoi::HexWorld::Testing::TestSidecarFile;
using ::Draoi::HexWorld::Testing::TestRules;
using ::Draoi::HexWorld::Testing::TestSession;

} // namespace WorldGeneration
} // namespace CoW
