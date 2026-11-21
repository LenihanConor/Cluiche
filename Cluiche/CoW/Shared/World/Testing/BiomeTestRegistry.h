// Test utilities for the biome registry. Ships with the library (no gtest dependency): failures print and abort.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md
#pragma once

#include <Shared/World/BiomeRegistry.h>
#include <Shared/World/CellVocabulary.h>
#include <Shared/World/StratumTable.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <Shared/World/Testing/StratumTestIndices.h>

namespace CoW {
namespace World {
namespace Testing {

inline const StratumTable& ShippedStratumTable();

namespace Detail
{
    // Walks up from this header's directory until `<dir>/<relative>` is a regular file or directory.
    inline std::string FindUpwards(const char* relative, bool wantDirectory)
    {
        std::error_code ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / relative;
            if (wantDirectory ? std::filesystem::is_directory(candidate, ec) : std::filesystem::is_regular_file(candidate, ec))
            {
                return candidate.string();
            }
            const std::filesystem::path parent = dir.parent_path();
            if (parent == dir) { break; }
            dir = parent;
        }
        return {};
    }

    inline std::string ReadFileText(const std::string& path)
    {
        std::ifstream     in(path, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    inline void LoadOrAbort(BiomeRegistry& reg, const std::string& path, const char* what)
    {
        std::ifstream in(path, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        RegistryError err;
        if (path.empty() || !in.good() || !reg.LoadFromText(ss.str().c_str(), ShippedStratumTable(), err))
        {
            std::fprintf(stderr, "BiomeTestRegistry: failed to load %s (path='%s') field=%s id=%s row=%u reason=%s\n", what,
                         path.c_str(), err.field.AsCStr(), err.id.AsCStr(), err.row, err.reason.AsCStr());
            std::fflush(stderr);
            std::abort();
        }
    }
}

// Absolute paths of the shipped Global/World/strata.json and its sidecar strata.schema.json.
inline std::string ShippedStrataPath() { return Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/strata.json", false); }
inline std::string ShippedStratumSidecarPath() { return Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/strata.schema.json", false); }

// The shipped stratum table (FirstShores, WildReaches, AncientInteriors), loaded once per process. Aborts if it cannot be loaded.
inline const StratumTable& ShippedStratumTable()
{
    static StratumTable strata;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        StratumError err;
        const std::string strataText = Detail::ReadFileText(ShippedStrataPath());
        const std::string sidecarText = Detail::ReadFileText(ShippedStratumSidecarPath());
        if (strataText.empty() || sidecarText.empty() || !strata.LoadFromText(strataText.c_str(), sidecarText.c_str(), err))
        {
            std::fprintf(stderr, "BiomeTestRegistry: failed to load shipped strata: field=%s reason=%s\n", err.field.c_str(), err.reason.c_str());
            std::fflush(stderr);
            std::abort();
        }
    }
    return strata;
}

// A three-stratum depth-set strata file naming the strata `names` (the shipped numbers: 4-6, 4-6, 2-4 regions,
// innermost main-landmass-only with min coast depth 3) and the matching sidecar (shares 0.45 / 0.40 / 0.15).
// For tests that need a differently named table; a name list of another length is allowed for load-fault tests.
inline std::string TestStrataText(const std::vector<std::string>& names)
{
    std::string list;
    for (size_t i = 0; i < names.size(); ++i)
    {
        const bool inner = (i + 1 == names.size());
        list += std::string(i == 0 ? "" : ",") + "{\"name\":\"" + names[i] + "\",\"regions_min\":" + (inner ? "2" : "4") +
                ",\"regions_max\":" + (inner ? "4" : "6") + (inner ? ",\"main_landmass_only\":true,\"min_coast_depth\":3" : "") + "}";
    }
    return "{\"format\":\"cow.strata\",\"version\":1,\"sets\":[{\"id\":\"depth\",\"ordered\":true,"
           "\"assigner\":{\"type\":\"coast_cost_bands\",\"metric\":\"coast_cost\"},\"strata\":[" + list + "]}]}";
}

inline std::string TestStrataSidecar(const std::vector<std::string>& names, const std::vector<float>& shares = { 0.45f, 0.40f, 0.15f })
{
    std::string entries;
    for (size_t i = 0; i < names.size(); ++i)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(i < shares.size() ? shares[i] : 0.0f));
        entries += std::string(i == 0 ? "" : ",") + "{\"key\":\"depth." + names[i] + ".share\",\"kind\":\"parameter\",\"label\":\"share\","
                   "\"description\":\"test\",\"type\":\"float\",\"min\":0.0,\"max\":1.0,\"default\":" + buf + "}";
    }
    if (!names.empty())
    {
        // The innermost stratum declares min_coast_depth (TestStrataText), so the sidecar carries its live value.
        entries += ",{\"key\":\"depth." + names.back() + ".min_coast_depth\",\"kind\":\"parameter\",\"label\":\"depth\","
                   "\"description\":\"test\",\"type\":\"float\",\"min\":0.0,\"max\":16.0,\"default\":3.0}";
    }
    return "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"cow.strata\",\"entries\":[" + entries + "]}";
}

// Absolute path of the shipped Global/World/biomes.json (for WorldGenerationModule::SetBiomesPath in module tests).
inline std::string ShippedBiomesPath() { return Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/biomes.json", false); }

// The shipped Global/World/biomes.json, loaded once per process. Aborts if it cannot be loaded.
inline const BiomeRegistry& ShippedBiomeRegistry()
{
    static BiomeRegistry reg;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        Detail::LoadOrAbort(reg, Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/biomes.json", false), "shipped biomes.json");
    }
    return reg;
}

// Shipped rows plus one appended climate biome (Biome.Savanna). Aborts if it cannot be loaded.
inline const BiomeRegistry& ExtraBiomeRegistry()
{
    static BiomeRegistry reg;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        Detail::LoadOrAbort(reg, Detail::FindUpwards("Cluiche/Tests/GoogleTests/CoW/Fixtures/biomes_extra.json", false), "biomes_extra.json");
    }
    return reg;
}

// The shipped biomes.json with textual edits applied (each `from` must occur once; the first occurrence is replaced),
// loaded into a fresh registry. For fixtures that change a priority, an envelope or a row. Aborts on a bad edit or load.
inline BiomeRegistry LoadShippedVariant(std::initializer_list<std::pair<const char*, const char*>> edits)
{
    std::ifstream     in(Detail::FindUpwards("Cluiche/Assets/CoW/Global/World/biomes.json", false), std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    for (const auto& e : edits)
    {
        const size_t at = text.find(e.first);
        if (at == std::string::npos)
        {
            std::fprintf(stderr, "BiomeTestRegistry: variant edit target missing: %s\n", e.first);
            std::fflush(stderr);
            std::abort();
        }
        text.replace(at, std::strlen(e.first), e.second);
    }
    BiomeRegistry reg;
    RegistryError err;
    if (!reg.LoadFromText(text.c_str(), ShippedStratumTable(), err))
    {
        std::fprintf(stderr, "BiomeTestRegistry: variant failed to load: field=%s id=%s row=%u reason=%s\n", err.field.AsCStr(),
                     err.id.AsCStr(), err.row, err.reason.AsCStr());
        std::fflush(stderr);
        std::abort();
    }
    return reg;
}

// Id -> index in `reg`. Aborts with a message if the id is absent.
inline BiomeIndex B(const BiomeRegistry& reg, const char* id)
{
    const BiomeIndex b = reg.Find(Dia::Core::StringCRC(id));
    if (b == kNoBiome)
    {
        std::fprintf(stderr, "BiomeTestRegistry: unknown biome id '%s'\n", id);
        std::fflush(stderr);
        std::abort();
    }
    return b;
}

inline BiomeIndex B(const char* id) { return B(ShippedBiomeRegistry(), id); }

} // namespace Testing
} // namespace World
} // namespace CoW
