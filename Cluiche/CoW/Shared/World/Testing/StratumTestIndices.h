// Test-only names for the shipped CoW stratum indices (FirstShores, WildReaches, AncientInteriors).
// Production code addresses strata by index/table position; tests keep readable intent with these constants.
// Visible from both CoW::World and CoW::WorldGeneration so either `using namespace` finds them.
#pragma once

#include <Shared/World/CellVocabulary.h>

namespace CoW {
namespace World {

constexpr WorldGeneration::StratumIndex kStratumFirst   = 1;   // shipped stratum 1: FirstShores
constexpr WorldGeneration::StratumIndex kStratumWild    = 2;   // shipped stratum 2: WildReaches
constexpr WorldGeneration::StratumIndex kStratumAncient = 3;   // shipped stratum 3: AncientInteriors

using WorldGeneration::kNoStratum;

} // namespace World

namespace WorldGeneration {

using World::kStratumFirst;
using World::kStratumWild;
using World::kStratumAncient;

} // namespace WorldGeneration
} // namespace CoW
