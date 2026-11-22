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


namespace CoWTerrainRulesTestDetail
{
    inline std::string ReadSidecarFromSourceTree()
    {
        return ::Draoi::HexWorld::TerrainRulesTestDetail::ReadFileFromSourceTree("Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json", __FILE__);
    }
}

inline const std::string& TestSidecarText()
{
    static const std::string text = [] {
        std::string t = CoWTerrainRulesTestDetail::ReadSidecarFromSourceTree();
        if (t.empty())
        {
            ::Draoi::HexWorld::TerrainRulesTestDetail::Fail("terrain_rules.schema.json not found above ", __FILE__);
        }
        return t;
    }();
    return text;
}

inline const Dia::Attribute::ParameterFile& TestSidecarFile()
{
    static Dia::Attribute::ParameterFile* file = [] {
        auto* f = new Dia::Attribute::ParameterFile();   // intentionally leaked: lives for the process
        Dia::Attribute::ParamError err;
        if (!f->Load(TestSidecarText().c_str(), "terrain_rules.schema.json", nullptr, err))
        {
            ::Draoi::HexWorld::TerrainRulesTestDetail::Fail("sidecar failed to load at key ", err.key.AsCStr());
        }
        return f;
    }();
    return *file;
}

inline TerrainRules TestRules()
{
    static const TerrainRules rules = [] {
        TerrainRules r;
        if (!TerrainRules::FromParameterFile(TestSidecarFile(), r))
        {
            ::Draoi::HexWorld::TerrainRulesTestDetail::Fail("sidecar does not match TerrainRules", "");
        }
        return r;
    }();
    return rules;
}

// A WorldGenSession whose constructor loads the sidecar rules (and aborts if that fails).
struct TestSession : WorldGenSession
{
    TestSession()
    {
        SetStepSeedNames(CoWStepSeedNames());
        SetNodeKinds(CoWNodeKindIds());
        SetBiomeRegistry(&::Draoi::HexWorld::Testing::ShippedBiomeRegistry());
        SetGuaranteeSpec(CoW::WorldGeneration::Testing::ShippedGuaranteeSpec());
        Dia::Attribute::ParamError err;
        if (!LoadRules(TestSidecarText().c_str(), err))
        {
            ::Draoi::HexWorld::TerrainRulesTestDetail::Fail("LoadRules failed at key ", err.key.AsCStr());
        }
    }
};

} // namespace WorldGeneration
} // namespace CoW
