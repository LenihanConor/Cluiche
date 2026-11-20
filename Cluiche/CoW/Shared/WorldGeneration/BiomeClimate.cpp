// S-1 Biome Classification: Stages 12 and 13. See BiomeClimate.h.
#pragma fp_contract(off)

#include <Shared/WorldGeneration/BiomeClimate.h>

#include <DiaPathfinding/GridSearch.h>
#include <DiaPathfinding/HexNeighbours.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;
using Dia::Pathfinding::kUnreached;
using Dia::Pathfinding::StepSeed;

namespace
{
    float Clamp01(float v)
    {
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    // Weight of the westerly sweep for a latitude in [0,1]: 0 trade/polar, 1 westerly, linear across +/-blend.
    float WesterlyWeight(float latitude, float blend)
    {
        const float deg = latitude * 90.0f;
        if (blend <= 0.0f)
        {
            return (deg >= 30.0f && deg <= 60.0f) ? 1.0f : 0.0f;
        }
        const float lowRise = Clamp01((deg - (30.0f - blend)) / (2.0f * blend));
        const float highFall = Clamp01(((60.0f + blend) - deg) / (2.0f * blend));
        return std::min(lowRise, highFall);
    }

    // One wind sweep. eastward: columns west->east, upwind dirs 2,3; else east->west, dirs 0,5.
    void Sweep(const HexTopology& topo, int w, int h, bool eastward, const float* elevation, const uint32_t* waterSource,
               float decay, float orographic, float* air, float* rain)
    {
        const uint32_t dirA = eastward ? 2u : 0u;
        const uint32_t dirB = eastward ? 3u : 5u;
        for (int step = 0; step < w; ++step)
        {
            const int x = eastward ? step : (w - 1 - step);
            for (int y = 0; y < h; ++y)
            {
                const int c = y * w + x;
                const uint32_t ws = waterSource[c];
                if (ws == static_cast<uint32_t>(WaterSource::Ocean) || ws == static_cast<uint32_t>(WaterSource::Lake))
                {
                    air[c] = 1.0f;
                    rain[c] = 0.0f;
                    continue;
                }
                float airSum = 0.0f;
                float elevSum = 0.0f;
                int   count = 0;
                const uint32_t dirs[2] = {dirA, dirB};
                for (uint32_t d : dirs)
                {
                    const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                    if (nb != kNoCell)
                    {
                        airSum += air[nb];
                        elevSum += elevation[nb];
                        ++count;
                    }
                }
                float carried = 0.0f;
                float rise = 0.0f;
                if (count > 0)
                {
                    carried = airSum / static_cast<float>(count);
                    rise = std::max(0.0f, elevation[c] - elevSum / static_cast<float>(count));
                }
                const float rainOut = std::min(carried, orographic * rise);
                rain[c] = rainOut;
                air[c] = Clamp01(carried - decay - rainOut);
            }
        }
    }
}

void ComputeStage12(const TerrainRules& rules, GridDimensions dims, float temperatureOffset, const float* elevation,
                    const float* latitude, float* temperatureOut)
{
    const int n = std::max(dims.width, 0) * std::max(dims.height, 0);
    if (n == 0 || temperatureOut == nullptr || elevation == nullptr || latitude == nullptr)
    {
        return;
    }
    const float latCool = rules.kLatCool;
    const float lapse = rules.kLapse;
    for (int i = 0; i < n; ++i)
    {
        temperatureOut[i] = Clamp01(1.0f - latCool * latitude[i] - lapse * elevation[i] + temperatureOffset);
    }
}

void ComputeStage13(const TerrainRules& rules, GridDimensions dims, float moistureOffset, const float* elevation,
                    const float* latitude, const uint32_t* waterSource, Stage13Layers& out)
{
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || elevation == nullptr || latitude == nullptr || waterSource == nullptr)
    {
        return;
    }

    const HexTopology topo(w, h);

    // Water distance: multi-source step search; oceans/lakes start at 0, rivers at the river penalty.
    const uint32_t riverStart = static_cast<uint32_t>(std::max(TerrainRules::RoundToInt(rules.kRiverPenalty), 0));
    std::vector<StepSeed> seeds;
    seeds.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        const uint32_t ws = waterSource[i];
        if (ws == static_cast<uint32_t>(WaterSource::Ocean) || ws == static_cast<uint32_t>(WaterSource::Lake))
        {
            seeds.push_back({static_cast<uint32_t>(i), 0u, static_cast<uint32_t>(i)});
        }
        else if (ws == static_cast<uint32_t>(WaterSource::River))
        {
            seeds.push_back({static_cast<uint32_t>(i), riverStart, static_cast<uint32_t>(i)});
        }
    }
    std::vector<uint32_t> dist(static_cast<size_t>(n), kUnreached);
    const uint32_t maxD = static_cast<uint32_t>(n) + riverStart + 1u;
    const Dia::Pathfinding::HexNeighbours graph(topo);
    Dia::Pathfinding::GridSearchScratch scratch;
    Dia::Pathfinding::MultiSourceSteps(graph, seeds.data(), static_cast<uint32_t>(seeds.size()),
                                       [](uint32_t) { return true; }, maxD, dist.data(), nullptr, scratch);

    // Wind sweeps.
    std::vector<float> airW(static_cast<size_t>(n), 0.0f), rainW(static_cast<size_t>(n), 0.0f);
    std::vector<float> airE(static_cast<size_t>(n), 0.0f), rainE(static_cast<size_t>(n), 0.0f);
    const float decay = rules.kAirDecay;
    const float oro = rules.kOrographic;
    Sweep(topo, w, h, true, elevation, waterSource, decay, oro, airW.data(), rainW.data());
    Sweep(topo, w, h, false, elevation, waterSource, decay, oro, airE.data(), rainE.data());

    const float scale = rules.kMoistScale;
    const float mix = rules.kProximityMix;
    const float bonus = rules.kRainBonus;
    const float blend = rules.kBandBlend;
    for (int i = 0; i < n; ++i)
    {
        const float ww = WesterlyWeight(latitude[i], blend);
        const float air = ww * airW[i] + (1.0f - ww) * airE[i];
        const float rain = ww * rainW[i] + (1.0f - ww) * rainE[i];
        const float proximity = dist[i] == kUnreached ? 0.0f
                                                      : static_cast<float>(std::exp(-static_cast<double>(dist[i]) / scale));
        if (out.waterDistance != nullptr) { out.waterDistance[i] = dist[i]; }
        if (out.airMoisture != nullptr)   { out.airMoisture[i] = air; }
        if (out.rainOut != nullptr)       { out.rainOut[i] = rain; }
        if (out.moisture != nullptr)
        {
            out.moisture[i] = Clamp01(mix * proximity + (1.0f - mix) * air + bonus * rain + moistureOffset);
        }
    }
}

} // namespace WorldGeneration
} // namespace CoW
