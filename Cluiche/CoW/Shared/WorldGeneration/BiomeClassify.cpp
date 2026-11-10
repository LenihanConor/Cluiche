// S-1 Biome Classification: Stage 14 (Biomes). See BiomeClassify.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-classification.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/BiomeClassify.h>
#include <Shared/WorldGeneration/BiomeTypes.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace
{
    constexpr float kLowest = std::numeric_limits<float>::lowest();

    // Climate biomes in enum order (so strict-greater argmax ties go to the lowest enum value).
    constexpr Biome kClimateBiomes[] = { Biome::GrasslandSteppe, Biome::TemperateForest, Biome::DesertBasin,
                                         Biome::TundraBoreal, Biome::Rainforest };

    bool IsClimateBiome(Biome b)
    {
        return b == Biome::GrasslandSteppe || b == Biome::TemperateForest || b == Biome::DesertBasin ||
               b == Biome::TundraBoreal || b == Biome::Rainforest;
    }

    bool IsLandCell(const Stage14Inputs& in, int c)
    {
        const uint32_t ws = in.waterSource[c];
        return ws != static_cast<uint32_t>(WaterSource::Ocean) && ws != static_cast<uint32_t>(WaterSource::Lake);
    }

    struct Classifier
    {
        const TerrainRules&  rules;
        const Stage14Inputs& in;
        Stage14Layers&       out;
        HexTopology          topo;
        int                  n;
        std::vector<uint8_t> wetland;   // wetland-eligible per cell

        Classifier(const TerrainRules& r, const Stage14Inputs& i, Stage14Layers& o)
            : rules(r), in(i), out(o), topo(i.dims.width, i.dims.height), n(i.dims.width * i.dims.height),
              wetland(static_cast<size_t>(n), 0)
        {
        }

        Biome BiomeAt(int c) const { return static_cast<Biome>(out.biomeIndex[c]); }

        float ScoreOf(int c, Biome b) const
        {
            return b == Biome::WetlandDelta ? 0.0f : BiomeClimateScore(rules, b, in.temperature[c], in.moisture[c]);
        }

        // Best-scoring zone-valid candidate (five climate biomes, WetlandDelta last when eligible).
        // checkNeighbours: reject candidates that form a forbidden pair with any neighbour's current biome.
        bool PickBest(int c, bool checkNeighbours, Biome& result) const
        {
            const DepthZone zone = static_cast<DepthZone>(in.depthZone[c]);
            bool            found = false;
            float           bestScore = kLowest;
            auto            consider = [&](Biome b, float score)
            {
                if (!IsBiomeValidInZone(b, zone))
                {
                    return;
                }
                if (checkNeighbours)
                {
                    for (uint32_t d = 0; d < 6u; ++d)
                    {
                        const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                        if (nb != kNoCell && AreBiomesForbidden(b, BiomeAt(static_cast<int>(nb))))
                        {
                            return;
                        }
                    }
                }
                if (!found || score > bestScore)
                {
                    found = true;
                    bestScore = score;
                    result = b;
                }
            };
            for (Biome b : kClimateBiomes)
            {
                consider(b, BiomeClimateScore(rules, b, in.temperature[c], in.moisture[c]));
            }
            if (wetland[c] != 0)
            {
                consider(Biome::WetlandDelta, kLowest);   // no climate target: last resort only
            }
            return found;
        }

        void Assign(int c, Biome b)
        {
            out.biomeIndex[c] = static_cast<uint32_t>(b);
            out.biomeScore[c] = IsClimateBiome(b) ? ScoreOf(c, b) : 0.0f;
        }
    };
} // namespace

float BiomeClimateScore(const TerrainRules& rules, Biome b, float temperature, float moisture)
{
    float t0, m0, wt, wm;
    switch (b)
    {
    case Biome::GrasslandSteppe: t0 = rules.kT0_Grass();      m0 = rules.kM0_Grass();      wt = rules.kWT_Grass();      wm = rules.kWM_Grass();      break;
    case Biome::TemperateForest: t0 = rules.kT0_Forest();     m0 = rules.kM0_Forest();     wt = rules.kWT_Forest();     wm = rules.kWM_Forest();     break;
    case Biome::DesertBasin:     t0 = rules.kT0_Desert();     m0 = rules.kM0_Desert();     wt = rules.kWT_Desert();     wm = rules.kWM_Desert();     break;
    case Biome::TundraBoreal:    t0 = rules.kT0_Tundra();     m0 = rules.kM0_Tundra();     wt = rules.kWT_Tundra();     wm = rules.kWM_Tundra();     break;
    case Biome::Rainforest:      t0 = rules.kT0_Rainforest(); m0 = rules.kM0_Rainforest(); wt = rules.kWT_Rainforest(); wm = rules.kWM_Rainforest(); break;
    default:                     return kLowest;
    }
    const float dt = (temperature - t0) / wt;
    const float dm = (moisture - m0) / wm;
    return -(dt * dt) - (dm * dm);
}

bool IsRiverMouth(const Stage14Inputs& in, int cell)
{
    if (in.dims.width <= 0 || in.dims.height <= 0 || cell < 0 || cell >= in.dims.width * in.dims.height ||
        in.waterSource == nullptr || in.waterSource[cell] != static_cast<uint32_t>(WaterSource::River))
    {
        return false;
    }
    const HexTopology topo(in.dims.width, in.dims.height);
    for (uint32_t d = 0; d < 6u; ++d)
    {
        const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(cell), d);
        if (nb != kNoCell && in.waterSource[nb] == static_cast<uint32_t>(WaterSource::Ocean))
        {
            return true;
        }
    }
    return false;
}

bool IsWetlandEligible(const TerrainRules& rules, const Stage14Inputs& in, int cell)
{
    if (in.dims.width <= 0 || in.dims.height <= 0 || cell < 0 || cell >= in.dims.width * in.dims.height ||
        in.waterSource == nullptr || in.elevation == nullptr)
    {
        return false;
    }
    if (!IsLandCell(in, cell) || in.elevation[cell] > rules.kWetlandMaxElev())
    {
        return false;
    }
    if (IsRiverMouth(in, cell))
    {
        return true;
    }
    const HexTopology topo(in.dims.width, in.dims.height);
    for (uint32_t d = 0; d < 6u; ++d)
    {
        const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(cell), d);
        if (nb == kNoCell)
        {
            continue;
        }
        if (in.waterSource[nb] == static_cast<uint32_t>(WaterSource::Lake) || IsRiverMouth(in, static_cast<int>(nb)))
        {
            return true;
        }
    }
    return in.terrainFlags != nullptr && in.moisture != nullptr && (in.terrainFlags[cell] & TerrainFlag::Coast) != 0 &&
           in.moisture[cell] >= rules.kWetlandMoist();
}

bool IsVentCell(const Stage14Inputs& in, int cell)
{
    if (cell < 0 || cell >= in.dims.width * in.dims.height)
    {
        return false;
    }
    const bool flagged = in.terrainFlags != nullptr && (in.terrainFlags[cell] & TerrainFlag::Volcanic) != 0;
    const bool volcano = in.terrainType != nullptr && in.terrainType[cell] == static_cast<uint32_t>(TerrainType::Volcano);
    return flagged || volcano;
}

void ComputeStage14(const TerrainRules& rules, const Stage14Inputs& in, Stage14Layers& out, Stage14Debug& dbg)
{
    dbg = Stage14Debug{};
    if (in.dims.width <= 0 || in.dims.height <= 0)
    {
        return;
    }
    if (in.temperature == nullptr || in.moisture == nullptr || in.elevation == nullptr || in.terrainType == nullptr ||
        in.depthZone == nullptr || in.terrainFlags == nullptr || in.waterSource == nullptr || out.biomeScore == nullptr ||
        out.biomePin == nullptr || out.biomeIndex == nullptr || out.fixupChanges == nullptr)
    {
        return;
    }

    Classifier k(rules, in, out);
    const int  n = k.n;
    for (int c = 0; c < n; ++c)
    {
        k.wetland[c] = IsWetlandEligible(rules, in, c) ? 1 : 0;
    }

    const bool plateau = in.barrierForm == BarrierForm::Plateau && in.barrier > 0.0f && in.barrierMask != nullptr;

    // Steps 1-3: pins, plateau interior, climate score.
    for (int c = 0; c < n; ++c)
    {
        out.fixupChanges[c] = BiomeFixup::None;
        out.biomeScore[c] = 0.0f;
        const uint32_t ws = in.waterSource[c];
        const uint32_t tt = in.terrainType[c];
        const uint32_t fl = in.terrainFlags[c];

        if (ws == static_cast<uint32_t>(WaterSource::Ocean))
        {
            out.biomeIndex[c] = static_cast<uint32_t>(Biome::Ocean);
            out.biomePin[c] = BiomePin::Water;
            continue;
        }
        if (ws == static_cast<uint32_t>(WaterSource::Lake))
        {
            out.biomeIndex[c] = static_cast<uint32_t>(Biome::Lake);
            out.biomePin[c] = BiomePin::Water;
            continue;
        }
        if ((fl & TerrainFlag::Volcanic) != 0 || tt == static_cast<uint32_t>(TerrainType::Volcano) ||
            (in.barrierForm == BarrierForm::VolcanicChain && (fl & TerrainFlag::Barrier) != 0))
        {
            out.biomeIndex[c] = static_cast<uint32_t>(Biome::VolcanicAshland);
            out.biomePin[c] = BiomePin::Volcanic;
            continue;
        }
        if (tt == static_cast<uint32_t>(TerrainType::Mountain) || tt == static_cast<uint32_t>(TerrainType::Peak) ||
            tt == static_cast<uint32_t>(TerrainType::Cliff) || tt == static_cast<uint32_t>(TerrainType::Pass))
        {
            out.biomeIndex[c] = static_cast<uint32_t>(Biome::AlpineMountain);
            out.biomePin[c] = BiomePin::Alpine;
            continue;
        }
        if (k.wetland[c] != 0)
        {
            out.biomeIndex[c] = static_cast<uint32_t>(Biome::WetlandDelta);
            out.biomePin[c] = BiomePin::Wetland;
            continue;
        }

        out.biomePin[c] = BiomePin::None;
        if (plateau && in.barrierMask[c] >= rules.kPlateauInner())
        {
            k.Assign(c, in.moisture[c] >= rules.kPlateauWet() ? Biome::Rainforest : Biome::DesertBasin);
            continue;
        }

        Biome best = kClimateBiomes[0];
        float bestScore = kLowest;
        for (Biome b : kClimateBiomes)
        {
            const float s = BiomeClimateScore(rules, b, in.temperature[c], in.moisture[c]);
            if (s > bestScore)
            {
                bestScore = s;
                best = b;
            }
        }
        out.biomeIndex[c] = static_cast<uint32_t>(best);
        out.biomeScore[c] = bestScore;
    }

    // Step 4: zone fix-up.
    for (int c = 0; c < n; ++c)
    {
        const Biome b = k.BiomeAt(c);
        if (b == Biome::Ocean || b == Biome::Lake)
        {
            continue;
        }
        if (b == Biome::VolcanicAshland && IsVentCell(in, c))
        {
            continue;
        }
        if (IsBiomeValidInZone(b, static_cast<DepthZone>(in.depthZone[c])))
        {
            continue;
        }
        Biome pick;
        if (k.PickBest(c, false, pick))
        {
            k.Assign(c, pick);
            out.fixupChanges[c] = BiomeFixup::Zone;
            ++dbg.zoneChanges;
        }
    }

    // Step 5: forbidden-neighbour fix-up.
    const int maxPasses = std::max(rules.GetInt(Rule::kMaxFixupPasses), 1);
    for (int pass = 0; pass < maxPasses; ++pass)
    {
        bool changed = false;
        for (int c = 0; c < n; ++c)
        {
            for (uint32_t d = 0; d < 6u; ++d)
            {
                const uint32_t nbu = k.topo.NeighbourAt(static_cast<uint32_t>(c), d);
                if (nbu == kNoCell || static_cast<int>(nbu) <= c)
                {
                    continue;   // each unordered pair is handled from its lower-index cell
                }
                const int nb = static_cast<int>(nbu);
                if (!AreBiomesForbidden(k.BiomeAt(c), k.BiomeAt(nb)))
                {
                    continue;
                }
                // Loser: lower pin, then lower biomeScore, then lower index (c < nb).
                int loser = c;
                if (out.biomePin[nb] < out.biomePin[c])
                {
                    loser = nb;
                }
                else if (out.biomePin[nb] == out.biomePin[c] && out.biomeScore[nb] < out.biomeScore[c])
                {
                    loser = nb;
                }
                Biome pick;
                if (k.PickBest(loser, true, pick))
                {
                    k.Assign(loser, pick);
                    out.fixupChanges[loser] = BiomeFixup::Forbidden;
                    ++dbg.forbiddenChanges;
                    changed = true;
                }
            }
        }
        if (!changed)
        {
            break;
        }
        ++dbg.passes;
    }

    for (int c = 0; c < n; ++c)
    {
        for (uint32_t d = 0; d < 6u; ++d)
        {
            const uint32_t nb = k.topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb != kNoCell && static_cast<int>(nb) > c && AreBiomesForbidden(k.BiomeAt(c), k.BiomeAt(static_cast<int>(nb))))
            {
                ++dbg.unresolvedPairs;
            }
        }
    }
}

} // namespace WorldGeneration
} // namespace CoW
