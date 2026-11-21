// Test-only names for the shipped CoW zone indices (FirstShores, WildReaches, AncientInteriors).
// Production code addresses zones by index/table position; tests keep readable intent with these constants.
// Visible from both CoW::World and CoW::WorldGeneration so either `using namespace` finds them.
#pragma once

#include <Shared/World/CellVocabulary.h>

namespace CoW {
namespace World {

constexpr WorldGeneration::DepthZone kZoneFirst   = 1;   // shipped zone 1: FirstShores
constexpr WorldGeneration::DepthZone kZoneWild    = 2;   // shipped zone 2: WildReaches
constexpr WorldGeneration::DepthZone kZoneAncient = 3;   // shipped zone 3: AncientInteriors

using WorldGeneration::kNoZone;

} // namespace World

namespace WorldGeneration {

using World::kZoneFirst;
using World::kZoneWild;
using World::kZoneAncient;

} // namespace WorldGeneration
} // namespace CoW
