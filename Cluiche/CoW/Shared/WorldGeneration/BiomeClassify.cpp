// S-1 Biome Classification: Stage 14 (Biomes). See BiomeClassify.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-classification.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/BiomeClassify.h>
#include <Shared/World/BiomeRegistry.h>

#include <DiaPathfinding/HexTopology.h>
#include <DiaObservation/Log/DiaLog.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;
using World::BiomeIndex;
using World::BiomeRegistry;
using World::kNoBiome;

namespace
{
    constexpr float kLowest = std::numeric_limits<float>::lowest();

    bool IsLandCell(const Stage14Inputs& in, int c)
    {
        const uint32_t ws = in.waterSource[c];
        return ws != static_cast<uint32_t>(WaterSource::Ocean) && ws != static_cast<uint32_t>(WaterSource::Lake);
    }

    // The terrain test behind each pin rule. The test stays in code (it reads terrain layers); which biome a hit
    // produces, and the rule's priority, come from the registry row.
    using PinPredicate = bool (*)(const Stage14Inputs& in, int c, bool wetlandEligible);

    bool HitsOcean(const Stage14Inputs& in, int c, bool) { return in.waterSource[c] == static_cast<uint32_t>(WaterSource::Ocean); }
    bool HitsLake(const Stage14Inputs& in, int c, bool) { return in.waterSource[c] == static_cast<uint32_t>(WaterSource::Lake); }
    bool HitsVent(const Stage14Inputs& in, int c, bool)
    {
        const uint32_t tt = in.terrainType[c];
        const uint32_t fl = in.terrainFlags[c];
        return (fl & TerrainFlag::Volcanic) != 0 || tt == static_cast<uint32_t>(TerrainType::Volcano) ||
               (in.barrierForm == BarrierForm::VolcanicChain && (fl & TerrainFlag::Barrier) != 0);
    }
    bool HitsAlpine(const Stage14Inputs& in, int c, bool)
    {
        const uint32_t tt = in.terrainType[c];
        return tt == static_cast<uint32_t>(TerrainType::Mountain) || tt == static_cast<uint32_t>(TerrainType::Peak) ||
               tt == static_cast<uint32_t>(TerrainType::Cliff) || tt == static_cast<uint32_t>(TerrainType::Pass);
    }
    bool HitsWetland(const Stage14Inputs&, int, bool wetlandEligible) { return wetlandEligible; }

    // The generator's rule table: rule id -> { predicate, biomePin layer value (the layer contract predates the
    // registry) }. The five terrain built-ins; there is no application hook yet.
    struct PinRuleEntry
    {
        Dia::Core::StringCRC id;
        PinPredicate         test;
        uint32_t             layerValue;
    };

    const PinRuleEntry* FindPinRule(Dia::Core::StringCRC rule)
    {
        static const PinRuleEntry kTable[] = {
            { Dia::Core::StringCRC("water_ocean"), &HitsOcean,   BiomePin::Water },
            { Dia::Core::StringCRC("water_lake"),  &HitsLake,    BiomePin::Water },
            { Dia::Core::StringCRC("vent"),        &HitsVent,    BiomePin::Volcanic },
            { Dia::Core::StringCRC("alpine"),      &HitsAlpine,  BiomePin::Alpine },
            { Dia::Core::StringCRC("wetland"),     &HitsWetland, BiomePin::Wetland },
        };
        for (const PinRuleEntry& e : kTable)
        {
            if (e.id == rule) { return &e; }
        }
        return nullptr;
    }

    bool PinTestHits(const PinRuleEntry* rule, const Stage14Inputs& in, int c, bool wetlandEligible)
    {
        return rule != nullptr && rule->test(in, c, wetlandEligible);
    }

    uint32_t PinLayerValue(const PinRuleEntry* rule) { return rule != nullptr ? rule->layerValue : BiomePin::None; }

    struct Classifier
    {
        const BiomeRegistry& reg;
        const Stage14Inputs& in;
        Stage14Layers&       out;
        HexTopology          topo;
        int                  n;
        std::vector<uint8_t> wetland;   // wetland-eligible per cell
        BiomeIndex           ventBiome = kNoBiome;       // the registry rows behind the pin rules the fix-ups name
        BiomeIndex           alpineBiome = kNoBiome;
        BiomeIndex           wetlandBiome = kNoBiome;
        BiomeIndex           oceanBiome = kNoBiome;
        BiomeIndex           lakeBiome = kNoBiome;

        Classifier(const BiomeRegistry& r, const Stage14Inputs& i, Stage14Layers& o)
            : reg(r), in(i), out(o), topo(i.dims.width, i.dims.height), n(i.dims.width * i.dims.height),
              wetland(static_cast<size_t>(n), 0)
        {
            uint32_t          count = 0;
            const BiomeIndex* pins = reg.PinsByPriority(count);
            // A rule no biome declares leaves its slot kNoBiome (unused); the last row declaring a rule wins.
            static const Dia::Core::StringCRC kOcean("water_ocean"), kLake("water_lake"), kVent("vent"),
                                              kAlpine("alpine"), kWetland("wetland");
            for (uint32_t p = 0; p < count; ++p)
            {
                const Dia::Core::StringCRC rule = reg.Pin(pins[p]);
                if (rule == kOcean)        { oceanBiome = pins[p]; }
                else if (rule == kLake)    { lakeBiome = pins[p]; }
                else if (rule == kVent)    { ventBiome = pins[p]; }
                else if (rule == kAlpine)  { alpineBiome = pins[p]; }
                else if (rule == kWetland) { wetlandBiome = pins[p]; }
            }
        }

        BiomeIndex BiomeAt(int c) const { return static_cast<BiomeIndex>(out.biomeIndex[c]); }

        float ScoreOf(int c, BiomeIndex b) const
        {
            return BiomeClimateScore(reg, b, in.temperature[c], in.moisture[c]);
        }

        // Best-scoring zone-valid candidate (the climate biomes in row order, the wetland row last when eligible).
        // checkNeighbours: reject candidates that form a forbidden pair with any neighbour's current biome.
        bool PickBest(int c, bool checkNeighbours, BiomeIndex& result) const
        {
            const DepthZone zone = static_cast<DepthZone>(in.depthZone[c]);
            bool            found = false;
            float           bestScore = kLowest;
            auto            consider = [&](BiomeIndex b, float score)
            {
                if (b == kNoBiome || !reg.IsValidInZone(b, zone))
                {
                    return;
                }
                if (checkNeighbours)
                {
                    for (uint32_t d = 0; d < 6u; ++d)
                    {
                        const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                        if (nb != kNoCell && reg.AreForbidden(b, BiomeAt(static_cast<int>(nb))))
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
            uint32_t          climateCount = 0;
            const BiomeIndex* climate = reg.ClimateBiomes(climateCount);
            for (uint32_t k = 0; k < climateCount; ++k)
            {
                consider(climate[k], BiomeClimateScore(reg, climate[k], in.temperature[c], in.moisture[c]));
            }
            if (wetland[c] != 0)
            {
                consider(wetlandBiome, kLowest);   // no climate target: last resort only
            }
            if (!found && checkNeighbours)
            {
                // Last resort (amended 2026-11-10): the alpine row forms no forbidden pair, accepted when zone-valid.
                consider(alpineBiome, kLowest);
            }
            return found;
        }

        void Assign(int c, BiomeIndex b)
        {
            out.biomeIndex[c] = static_cast<uint32_t>(b);
            out.biomeScore[c] = reg.Climate(b) != nullptr ? ScoreOf(c, b) : 0.0f;
        }
    };
} // namespace

bool IsBuiltInPinRule(Dia::Core::StringCRC rule)
{
    return FindPinRule(rule) != nullptr;
}

float BiomeClimateScore(const BiomeRegistry& biomes, BiomeIndex b, float temperature, float moisture)
{
    const World::ClimateEnvelope* e = biomes.Climate(b);
    if (e == nullptr)
    {
        return kLowest;
    }
    const float dt = (temperature - e->t0) / e->wt;
    const float dm = (moisture - e->m0) / e->wm;
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
    if (!IsLandCell(in, cell) || in.elevation[cell] > rules.kWetlandMaxElev)
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
           in.moisture[cell] >= rules.kWetlandMoist;
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
    if (in.biomes == nullptr || in.biomes->Count() == 0)
    {
        DIA_LOG_ERROR("CoW", "ComputeStage14: no biome registry (or an empty one); biomes were not classified");
        return;
    }

    Classifier k(*in.biomes, in, out);
    const BiomeRegistry& reg = k.reg;
    const int  n = k.n;
    for (int c = 0; c < n; ++c)
    {
        k.wetland[c] = IsWetlandEligible(rules, in, c) ? 1 : 0;
    }

    // The plateau rule needs both plateau rows; a registry without them classifies plateau cells like any other.
    const bool plateau = in.barrierForm == BarrierForm::Plateau && in.barrier > 0.0f && in.barrierMask != nullptr &&
                         reg.Plateau(true) != kNoBiome && reg.Plateau(false) != kNoBiome;

    uint32_t          pinCount = 0;
    const BiomeIndex* pins = reg.PinsByPriority(pinCount);
    uint32_t          climateCount = 0;
    const BiomeIndex* climate = reg.ClimateBiomes(climateCount);

    std::vector<uint8_t> interior(static_cast<size_t>(n), 0);

    // Steps 1-3: pins, plateau interior, climate score.
    for (int c = 0; c < n; ++c)
    {
        out.fixupChanges[c] = BiomeFixup::None;
        out.biomeScore[c] = 0.0f;

        // Pins in the registry's priority order (highest first); the first hit wins.
        bool pinned = false;
        for (uint32_t p = 0; p < pinCount; ++p)
        {
            const PinRuleEntry* rule = FindPinRule(reg.Pin(pins[p]));
            if (PinTestHits(rule, in, c, k.wetland[c] != 0))
            {
                out.biomeIndex[c] = static_cast<uint32_t>(pins[p]);
                out.biomePin[c] = PinLayerValue(rule);
                pinned = true;
                break;
            }
        }
        if (pinned)
        {
            continue;
        }

        out.biomePin[c] = BiomePin::None;
        if (plateau && in.barrierMask[c] >= rules.kPlateauInner)
        {
            interior[c] = 1;   // climate chosen per connected plateau below
            continue;
        }

        // Strict-greater argmax over the climate rows in row order: ties go to the lowest row index.
        BiomeIndex best = climateCount > 0 ? climate[0] : kNoBiome;
        float bestScore = kLowest;
        for (uint32_t i = 0; i < climateCount; ++i)
        {
            const float s = BiomeClimateScore(reg, climate[i], in.temperature[c], in.moisture[c]);
            if (s > bestScore)
            {
                bestScore = s;
                best = climate[i];
            }
        }
        out.biomeIndex[c] = static_cast<uint32_t>(best);
        out.biomeScore[c] = bestScore;
    }

    // Step 2 (amended 2026-11-10): one climate per connected plateau interior, by mean moisture.
    {
        std::vector<int> stack;
        std::vector<int> members;
        for (int s = 0; s < n; ++s)
        {
            if (interior[s] != 1)
            {
                continue;
            }
            members.clear();
            stack.clear();
            interior[s] = 2;
            stack.push_back(s);
            double sum = 0.0;
            while (!stack.empty())
            {
                const int c = stack.back();
                stack.pop_back();
                members.push_back(c);
                sum += static_cast<double>(in.moisture[c]);
                for (uint32_t d = 0; d < 6u; ++d)
                {
                    const uint32_t nb = k.topo.NeighbourAt(static_cast<uint32_t>(c), d);
                    if (nb != kNoCell && interior[nb] == 1)
                    {
                        interior[nb] = 2;
                        stack.push_back(static_cast<int>(nb));
                    }
                }
            }
            const float mean = static_cast<float>(sum / static_cast<double>(members.size()));
            const BiomeIndex b = reg.Plateau(mean >= rules.kPlateauWet);
            for (int c : members)
            {
                k.Assign(c, b);
            }
        }
    }

    // Step 4: zone fix-up.
    for (int c = 0; c < n; ++c)
    {
        const BiomeIndex b = k.BiomeAt(c);
        if (b == k.oceanBiome || b == k.lakeBiome)
        {
            continue;
        }
        if (b == k.ventBiome && IsVentCell(in, c))
        {
            continue;
        }
        if (reg.IsValidInZone(b, static_cast<DepthZone>(in.depthZone[c])))
        {
            continue;
        }
        BiomeIndex pick;
        if (k.PickBest(c, false, pick))
        {
            k.Assign(c, pick);
            out.fixupChanges[c] = BiomeFixup::Zone;
            ++dbg.zoneChanges;
        }
    }

    // Step 5: forbidden-neighbour fix-up.
    const int maxPasses = std::max(rules.kMaxFixupPasses, 1);
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
                if (!reg.AreForbidden(k.BiomeAt(c), k.BiomeAt(nb)))
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
                BiomeIndex pick;
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
            if (nb != kNoCell && static_cast<int>(nb) > c && reg.AreForbidden(k.BiomeAt(c), k.BiomeAt(static_cast<int>(nb))))
            {
                ++dbg.unresolvedPairs;
            }
        }
    }
}

} // namespace WorldGeneration
} // namespace CoW
