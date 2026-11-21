// S-1 Biome Classification test support: hand-built grids for climate stage tests (AC7/AC8/AC9).
// Header-only. Row-major, index = y * width + x.
#pragma once

#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/BiomeClassify.h>
#include <Shared/WorldGeneration/TerrainTypes.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <cstdint>
#include <vector>
#include <Shared/World/Testing/StratumTestIndices.h>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

struct ClimateGrid
{
    GridDimensions        dims;
    std::vector<float>    elevation;
    std::vector<float>    latitude;
    std::vector<uint32_t> waterSource;   // WaterSource values

    ClimateGrid(int w, int h, float lat = 0.0f)
        : dims{w, h},
          elevation(static_cast<size_t>(w) * h, 0.1f),
          latitude(static_cast<size_t>(w) * h, lat),
          waterSource(static_cast<size_t>(w) * h, 0u)
    {
    }

    int Index(int x, int y) const { return y * dims.width + x; }

    void SetWater(int x, int y, WaterSource s)
    {
        waterSource[Index(x, y)] = static_cast<uint32_t>(s);
        elevation[Index(x, y)] = (s == WaterSource::Ocean) ? 0.0f : elevation[Index(x, y)];
    }
};

// North-south ridge at column ridgeX of the given height over flat land (elevation 0.1).
// oceanWest / oceanEast fill column 0 / the last column with ocean. Constant latitude.
inline ClimateGrid MakeRidgeGrid(int w, int h, int ridgeX, float ridgeHeight, float latitude, bool oceanWest,
                                 bool oceanEast)
{
    ClimateGrid g(w, h, latitude);
    for (int y = 0; y < h; ++y)
    {
        g.elevation[g.Index(ridgeX, y)] = ridgeHeight;
        if (oceanWest)
        {
            g.SetWater(0, y, WaterSource::Ocean);
        }
        if (oceanEast)
        {
            g.SetWater(w - 1, y, WaterSource::Ocean);
        }
    }
    return g;
}

// Hand-built stage 14 grid. Defaults: every cell dry-ish grassland-perfect land (T 0.6, M 0.35), Lowland,
// WildReaches, no flags, no water. Row-major, index = y * width + x.
struct BiomeGrid
{
    GridDimensions        dims;
    std::vector<float>    temperature, moisture, elevation, barrierMask;
    std::vector<uint32_t> terrainType, stratum, terrainFlags, waterSource;
    BarrierForm           barrierForm = BarrierForm::Plateau;
    float                 barrier = 0.0f;
    const ::CoW::World::BiomeRegistry* registry = &::CoW::World::Testing::ShippedBiomeRegistry();   // swap for a fixture

    BiomeGrid(int w, int h, float t = 0.6f, float m = 0.35f)
        : dims{w, h},
          temperature(static_cast<size_t>(w) * h, t),
          moisture(static_cast<size_t>(w) * h, m),
          elevation(static_cast<size_t>(w) * h, 0.3f),
          barrierMask(static_cast<size_t>(w) * h, 0.0f),
          terrainType(static_cast<size_t>(w) * h, static_cast<uint32_t>(TerrainType::Lowland)),
          stratum(static_cast<size_t>(w) * h, static_cast<uint32_t>(kStratumWild)),
          terrainFlags(static_cast<size_t>(w) * h, 0u),
          waterSource(static_cast<size_t>(w) * h, 0u)
    {
    }

    int Index(int x, int y) const { return y * dims.width + x; }

    void SetClimate(int c, float t, float m) { temperature[c] = t; moisture[c] = m; }
    void SetZone(int c, StratumIndex z) { stratum[c] = static_cast<uint32_t>(z); }
    void SetTerrain(int c, TerrainType t) { terrainType[c] = static_cast<uint32_t>(t); }
    void AddFlag(int c, uint8_t f) { terrainFlags[c] |= f; }
    void SetWater(int c, WaterSource s)
    {
        waterSource[c] = static_cast<uint32_t>(s);
        if (s == WaterSource::Ocean)
        {
            elevation[c] = 0.0f;
            stratum[c] = static_cast<uint32_t>(kNoStratum);
            terrainType[c] = static_cast<uint32_t>(TerrainType::DeepWater);
        }
        else if (s == WaterSource::Lake)
        {
            stratum[c] = static_cast<uint32_t>(kNoStratum);
            terrainType[c] = static_cast<uint32_t>(TerrainType::Lake);
        }
    }

    Stage14Inputs Inputs() const
    {
        Stage14Inputs in;
        in.dims = dims;
        in.temperature = temperature.data();
        in.moisture = moisture.data();
        in.elevation = elevation.data();
        in.terrainType = terrainType.data();
        in.stratum = stratum.data();
        in.terrainFlags = terrainFlags.data();
        in.barrierMask = barrierMask.data();
        in.waterSource = waterSource.data();
        in.barrierForm = barrierForm;
        in.barrier = barrier;
        in.biomes = registry;
        return in;
    }
};

struct BiomeResult
{
    std::vector<float>    score;
    std::vector<uint32_t> pin, biome, fixup;
    Stage14Debug          debug;

    ::CoW::World::BiomeIndex At(int c) const { return static_cast<::CoW::World::BiomeIndex>(biome[c]); }
};

inline BiomeResult RunBiomes(const BiomeGrid& g, const TerrainRules& rules)
{
    const size_t n = g.temperature.size();
    BiomeResult  r;
    r.score.assign(n, -123.0f);
    r.pin.assign(n, 99u);
    r.biome.assign(n, 99u);
    r.fixup.assign(n, 99u);
    Stage14Layers out;
    out.biomeScore = r.score.data();
    out.biomePin = r.pin.data();
    out.biomeIndex = r.biome.data();
    out.fixupChanges = r.fixup.data();
    ComputeStage14(rules, g.Inputs(), out, r.debug);
    return r;
}

inline BiomeResult RunBiomes(const BiomeGrid& g)
{
    return RunBiomes(g, TestRules());
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
