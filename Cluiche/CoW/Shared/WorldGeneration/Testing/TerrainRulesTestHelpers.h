// S-1 test helper: the terrain rules and a ready-to-use session for tests.
// Production has no compiled default rules; the rules come from the terrain_rules sidecar
// (Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json). Tests read that file from the source tree once per
// process (the cache below lives in this Testing helper only, never in production code).
//   TestRules()          a TerrainRules at the sidecar defaults
//   TestSession          a WorldGenSession with the sidecar rules loaded and the shipped guarantee spec installed
//   TestSidecarText()    the sidecar text (for LoadRules / LoadFromJson / the module seam)
//   TestSidecarFile()    the sidecar loaded as a ParameterFile (rule metadata: names, ranges, defaults)
// Header-only. Aborts with a message if the sidecar cannot be found or loaded (a broken checkout, not a test result).
#pragma once

#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/Testing/GuaranteeSpecTestHelpers.h>
#include <Shared/WorldGeneration/WorldGenSession.h>

#include <DiaAttribute/ParameterFile.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace CoW {
namespace WorldGeneration {

namespace TerrainRulesTestDetail
{
    inline void Fail(const char* what, const char* detail)
    {
        std::fprintf(stderr, "TerrainRulesTestHelpers: %s%s\n", what, detail);
        std::fflush(stderr);
        std::abort();
    }

    inline std::string ReadSidecarFromSourceTree()
    {
        constexpr const char* kRelative = "Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json";
        std::error_code ec;
        const std::filesystem::path starts[] = { std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path(),
                                                 std::filesystem::current_path(ec) };
        for (std::filesystem::path dir : starts)
        {
            while (!dir.empty())
            {
                const std::filesystem::path candidate = dir / kRelative;
                if (std::filesystem::exists(candidate, ec))
                {
                    std::ifstream     in(candidate, std::ios::binary);
                    std::stringstream ss;
                    ss << in.rdbuf();
                    return ss.str();
                }
                if (dir == dir.parent_path())
                {
                    break;
                }
                dir = dir.parent_path();
            }
        }
        return {};
    }
}

inline const std::string& TestSidecarText()
{
    static const std::string text = [] {
        std::string t = TerrainRulesTestDetail::ReadSidecarFromSourceTree();
        if (t.empty())
        {
            TerrainRulesTestDetail::Fail("terrain_rules.schema.json not found above ", __FILE__);
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
            TerrainRulesTestDetail::Fail("sidecar failed to load at key ", err.key.AsCStr());
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
            TerrainRulesTestDetail::Fail("sidecar does not match TerrainRules", "");
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
        SetBiomeRegistry(&::CoW::World::Testing::ShippedBiomeRegistry());
        SetGuaranteeSpec(Testing::ShippedGuaranteeSpec());
        Dia::Attribute::ParamError err;
        if (!LoadRules(TestSidecarText().c_str(), err))
        {
            TerrainRulesTestDetail::Fail("LoadRules failed at key ", err.key.AsCStr());
        }
    }
};

} // namespace WorldGeneration
} // namespace CoW
