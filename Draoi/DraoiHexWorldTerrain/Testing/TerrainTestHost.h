// Neutral half of the S-1 terrain test host. The host application injects what is specific to its shipped data once,
// before any test runs (SetTerrainTestConfig); this header then provides the rules sidecar, TestRules() and a ready-to-use
// TestSession without naming any game. Mirrors the TestAssetPaths pattern of DraoiHexWorld/Testing/BiomeTestRegistry.h.
//   TestSidecarText()    the sidecar text (for LoadRules / LoadFromJson / the module seam)
//   TestSidecarFile()    the sidecar loaded as a ParameterFile (rule metadata: names, ranges, defaults)
//   TestRules()          a TerrainRules at the sidecar defaults
//   TestSession          a WorldGenSession with seed names, node kinds, biome registry, guarantees and sidecar rules loaded
// Production has no compiled default rules; tests read the sidecar from the source tree once per process (the caches
// below live in this Testing helper only). Header-only. Aborts with a message if the config was never set or the sidecar
// cannot be found or loaded (a broken checkout, not a test result).
#pragma once

#include <DraoiHexWorld/Sites.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldTerrain/GuaranteeSpec.h>
#include <DraoiHexWorldTerrain/RegionsAndSites.h>
#include <DraoiHexWorldTerrain/Testing/TerrainRulesTestHelpers.h>
#include <DraoiHexWorldTerrain/TerrainRules.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>

#include <DiaAttribute/ParameterFile.h>

#include <string>

namespace Draoi {
namespace HexWorld {
namespace Testing {

struct TerrainTestConfig
{
    std::string          rulesSidecarPath;                // repo-relative path of the terrain rules sidecar
    StepSeedNames        stepSeedNames;
    NodeKindIds          nodeKinds;
    const GuaranteeSpec* (*guaranteeSpec)() = nullptr;    // lazy: the shipped spec, installed into TestSession
};

namespace TerrainTestHostDetail
{
    inline TerrainTestConfig& Config() { static TerrainTestConfig config; return config; }

    inline const TerrainTestConfig& RequireConfig()
    {
        const TerrainTestConfig& config = Config();
        if (config.rulesSidecarPath.empty())
        {
            ::Draoi::HexWorld::TerrainRulesTestDetail::Fail("TerrainTestHost: terrain test config not set; call SetTerrainTestConfig", "");
        }
        return config;
    }
}

// The host application's tests set this once (before any test runs).
inline void SetTerrainTestConfig(const TerrainTestConfig& config) { TerrainTestHostDetail::Config() = config; }

// Clears the config so the unset-config abort can be exercised in a death test.
inline void ResetTerrainTestConfigForTest() { TerrainTestHostDetail::Config() = TerrainTestConfig(); }

// The node kind ids the host injected.
inline const NodeKindIds& TestNodeKinds() { return TerrainTestHostDetail::RequireConfig().nodeKinds; }

inline const std::string& TestSidecarText()
{
    const TerrainTestConfig& config = TerrainTestHostDetail::RequireConfig();
    static const std::string text = [&config] {
        std::string t = ::Draoi::HexWorld::TerrainRulesTestDetail::ReadFileFromSourceTree(config.rulesSidecarPath.c_str(), __FILE__);
        if (t.empty())
        {
            ::Draoi::HexWorld::TerrainRulesTestDetail::Fail("terrain rules sidecar not found above ", __FILE__);
        }
        return t;
    }();
    return text;
}

inline const Dia::Attribute::ParameterFile& TestSidecarFile()
{
    TerrainTestHostDetail::RequireConfig();
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
    TerrainTestHostDetail::RequireConfig();
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

// A WorldGenSession whose constructor installs the configured seed names, node kinds, biome registry and guarantees, then
// loads the sidecar rules (and aborts if that fails).
struct TestSession : WorldGenSession
{
    TestSession()
    {
        const TerrainTestConfig& config = TerrainTestHostDetail::RequireConfig();
        SetStepSeedNames(config.stepSeedNames);
        SetNodeKinds(config.nodeKinds);
        SetBiomeRegistry(&ShippedBiomeRegistry());
        if (config.guaranteeSpec != nullptr)
        {
            SetGuaranteeSpec(*config.guaranteeSpec());
        }
        Dia::Attribute::ParamError err;
        if (!LoadRules(TestSidecarText().c_str(), err))
        {
            ::Draoi::HexWorld::TerrainRulesTestDetail::Fail("LoadRules failed at key ", err.key.AsCStr());
        }
    }
};

} // namespace Testing
} // namespace HexWorld
} // namespace Draoi
