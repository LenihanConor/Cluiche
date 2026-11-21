// Test utilities for the biome registry. Ships with the library (no gtest dependency): failures print and abort.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md
#pragma once

#include <Shared/World/BiomeRegistry.h>
#include <Shared/WorldGeneration/TerrainEnums.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace CoW {
namespace World {
namespace Testing {

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

    inline void LoadOrAbort(BiomeRegistry& reg, const std::string& path, const char* what)
    {
        std::ifstream in(path, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        RegistryError err;
        if (path.empty() || !in.good() || !reg.LoadFromText(ss.str().c_str(), err))
        {
            std::fprintf(stderr, "BiomeTestRegistry: failed to load %s (path='%s') field=%s id=%s row=%u reason=%s\n", what,
                         path.c_str(), err.field.AsCStr(), err.id.AsCStr(), err.row, err.reason.AsCStr());
            std::fflush(stderr);
            std::abort();
        }
    }
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

// Interim bridge from the legacy enum (plan Task 27 deletes it).
inline BiomeIndex Idx(CoW::WorldGeneration::Biome b) { return static_cast<BiomeIndex>(b); }
// Reverse bridge for assigning into production types that still hold the enum (Cell::biome, RegionRecord::primaryBiome).
// Task 27 deletes these with Idx.
inline CoW::WorldGeneration::Biome Legacy(BiomeIndex b) { return static_cast<CoW::WorldGeneration::Biome>(b); }
inline CoW::WorldGeneration::Biome LegacyB(const char* id) { return Legacy(B(id)); }
// constexpr-friendly named constant that resolves lazily on conversion (avoids loading the registry during static
// initialisation): `constexpr BiomeConst G{"Biome.Grassland"};` then pass G wherever a legacy Biome is expected.
struct BiomeConst
{
    const char* id;
    operator CoW::WorldGeneration::Biome() const { return LegacyB(id); }
};

} // namespace Testing
} // namespace World
} // namespace CoW
