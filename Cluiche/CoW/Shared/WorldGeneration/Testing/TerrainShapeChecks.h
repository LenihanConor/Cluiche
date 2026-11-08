// S-1 Terrain Generator test support: whole-world shape checks used by AC18 (presets produce worlds).
// Header-only, no gtest dependency: every check returns failure texts so callers decide how to report.
// Covers AC2 (terrain contract), AC3 (ocean border), AC4 (land fraction, end to end through the Cells),
// AC9 (single connected ridge), AC10 (river reach and downhill), AC11 (lake outflow) and AC13 (depth zones).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

// Ridge cells are ridgeMask >= this (as the stage 2 tests).
constexpr float kShapeRidgeMin = 0.35f;
// AC4 tolerance on total land / all cells.
constexpr float kShapeLandTolerance = 0.01f;

struct ShapeFailures
{
    std::vector<std::string> messages;
    int count = 0;

    void Add(const std::string& m)
    {
        ++count;
        if (messages.size() < 12) { messages.push_back(m); }
    }
    bool Empty() const { return count == 0; }
    std::string Text() const
    {
        std::string s;
        for (const std::string& m : messages) { s += m + "; "; }
        if (count > static_cast<int>(messages.size())) { s += "(+" + std::to_string(count - static_cast<int>(messages.size())) + " more)"; }
        return s;
    }
};

inline const float* ShapeF(const TerrainGenerator& g, const char* name)
{
    return g.GetLayers().GetFloat(g.GetLayers().FindLayer(name));
}
inline const uint32_t* ShapeU(const TerrainGenerator& g, const char* name)
{
    return g.GetLayers().GetUInt(g.GetLayers().FindLayer(name));
}

// Connected groups of cells satisfying pred over the hex neighbours.
template <typename Pred>
int CountConnectedGroups(Pred pred, GridDimensions d)
{
    const int n = d.width * d.height;
    const Dia::Pathfinding::HexTopology topo(d.width, d.height);
    std::vector<uint8_t> seen(static_cast<size_t>(n), 0);
    int groups = 0;
    for (int i = 0; i < n; ++i)
    {
        if (seen[i] || !pred(i)) { continue; }
        ++groups;
        std::vector<int> q{ i };
        seen[i] = 1;
        for (size_t h = 0; h < q.size(); ++h)
        {
            for (uint32_t k = 0; k < 6; ++k)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(q[h]), k);
                if (nb != Dia::Pathfinding::kNoCell && !seen[nb] && pred(static_cast<int>(nb)))
                {
                    seen[nb] = 1;
                    q.push_back(static_cast<int>(nb));
                }
            }
        }
    }
    return groups;
}

// AC2 + AC3 + AC4, from the written Cells.
inline void CheckContractAndBorderAndLand(const TerrainGenerator& g, const WorldStructure& w, const TerrainRules& rules, ShapeFailures& f)
{
    const int W = w.dimensions.width, H = w.dimensions.height;
    if (W <= 0 || H <= 0 || w.CellCount() != W * H) { f.Add("world has no cells"); return; }
    const Dia::Pathfinding::HexTopology topo(W, H);
    const float coast = rules.kCoastElev();
    const int border = rules.GetInt(Rule::kBorderWidth);
    const uint32_t* lakeMask = ShapeU(g, "lakeMask");

    std::map<uint32_t, float> lakeElev;
    bool anyWater = false;
    int land = 0;
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            const int i = y * W + x;
            const Cell& c = w.At(x, y);
            const std::string at = " at (" + std::to_string(x) + "," + std::to_string(y) + ")";
            if (!(c.elevation >= 0.0f && c.elevation <= 1.0f)) { f.Add("AC2 elevation outside [0,1]" + at); }
            if (!(c.latitude >= 0.0f && c.latitude <= 1.0f)) { f.Add("AC2 latitude outside [0,1]" + at); }
            if (static_cast<uint8_t>(c.waterSource) > static_cast<uint8_t>(WaterSource::River)) { f.Add("AC2 invalid waterSource" + at); }
            if (static_cast<uint8_t>(c.terrain) >= static_cast<uint8_t>(TerrainType::Count)) { f.Add("AC2 invalid terrain" + at); }
            if (static_cast<uint8_t>(c.depthZone) > static_cast<uint8_t>(DepthZone::AncientInteriors)) { f.Add("AC2 invalid depthZone" + at); }
            anyWater = anyWater || c.waterSource != WaterSource::None;

            const bool ocean = c.landmass == 0;
            if ((c.depthZone == DepthZone::None) != ocean) { f.Add("AC2 depthZone None must be exactly ocean" + at); }
            if (ocean) { if (c.elevation != 0.0f) { f.Add("AC2 ocean elevation != 0" + at); } }
            else
            {
                ++land;
                if (c.elevation < coast) { f.Add("AC2 land below kCoastElev" + at); }
            }
            if (lakeMask[i] != 0)
            {
                const auto it = lakeElev.find(lakeMask[i]);
                if (it == lakeElev.end()) { lakeElev[lakeMask[i]] = c.elevation; }
                else if (it->second != c.elevation) { f.Add("AC2 lake cells at different elevations" + at); }
            }
            if (std::min(std::min(x, W - 1 - x), std::min(y, H - 1 - y)) < border)
            {
                if (!ocean || c.elevation != 0.0f) { f.Add("AC3 border cell is not ocean" + at); }
            }
            for (uint32_t d = 0; d < 6; ++d)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), d);
                if (nb == Dia::Pathfinding::kNoCell) { continue; }
                if (topo.NeighbourAt(nb, (d + 3) % 6) != static_cast<uint32_t>(i)) { f.Add("AC2 neighbour symmetry broken" + at); break; }
            }
        }
    }
    if (!anyWater) { f.Add("AC2 no cell has a waterSource"); }
    const float fraction = static_cast<float>(land) / static_cast<float>(W * H);
    if (!(std::abs(fraction - w.provenance.shape.landFraction) <= kShapeLandTolerance))
    {
        f.Add("AC4 land fraction " + std::to_string(fraction) + " vs target " + std::to_string(w.provenance.shape.landFraction));
    }
}

// AC10 reach and downhill rules, from the written Cells.
inline void CheckRiversReachAndDescend(const WorldStructure& w, ShapeFailures& f)
{
    const int W = w.dimensions.width, H = w.dimensions.height;
    const Dia::Pathfinding::HexTopology topo(W, H);
    const int n = W * H;
    for (int i = 0; i < n; ++i)
    {
        const Cell& c = w.Cells()[i];
        if (c.waterSource != WaterSource::River) { continue; }
        const std::string at = " at cell " + std::to_string(i);
        int cur = i;
        bool reached = false;
        for (int steps = 0; steps <= n; ++steps)
        {
            const Cell& cc = w.Cells()[cur];
            if (cc.downstream > 5) { f.Add("AC10 river cell without downstream" + at); break; }
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(cur), cc.downstream);
            if (nb == Dia::Pathfinding::kNoCell) { f.Add("AC10 downstream leaves the map" + at); break; }
            const Cell& next = w.Cells()[nb];
            if (next.elevation > cc.elevation) { f.Add("AC10 elevation rises toward the mouth" + at); }
            if (next.waterSource == WaterSource::Ocean || next.waterSource == WaterSource::Lake) { reached = true; break; }
            if (next.waterSource != WaterSource::River) { f.Add("AC10 river path leaves river/lake cells" + at); break; }
            cur = static_cast<int>(nb);
        }
        if (!reached) { f.Add("AC10 river never reaches an ocean or lake" + at); }
    }
}

// AC11: every lake's outflow path ends in the ocean; lake count never exceeds the request.
inline void CheckLakeOutflow(const TerrainGenerator& g, const WorldStructure& w, ShapeFailures& f)
{
    const Stage7Debug& d7 = g.GetStage7Debug();
    if (static_cast<int>(d7.lakes.size()) > static_cast<int>(w.provenance.shape.lakes)) { f.Add("AC11 more lakes than requested"); }
    for (size_t k = 0; k < d7.lakes.size(); ++k)
    {
        const Lake& l = d7.lakes[k];
        if (l.outflowPath.empty()) { f.Add("AC11 lake " + std::to_string(k) + " has no outflow path"); continue; }
        const int last = l.outflowPath.back();
        if (last < 0 || last >= w.CellCount() || w.Cells()[last].landmass != 0)
        {
            f.Add("AC11 lake " + std::to_string(k) + " outflow does not reach the ocean");
        }
    }
}

// AC13, from the zoneCost layers and the written Cells.
inline void CheckDepthZoneRules(const TerrainGenerator& g, const WorldStructure& w, ShapeFailures& f)
{
    const int n = w.CellCount();
    const uint32_t* coastDist = ShapeU(g, "coastDistance");
    const float* base = ShapeF(g, "zoneCostBase");
    const float* cost = ShapeF(g, "zoneCost");
    const Stage8Debug& d = g.GetStage8Debug();

    struct Range { float maxFirst = -1e30f, minWild = 1e30f, minAncient = 1e30f; };
    std::map<uint8_t, Range> perLandmass;
    for (int i = 0; i < n; ++i)
    {
        const Cell& c = w.Cells()[i];
        if (c.landmass == 0) { continue; }
        const std::string at = " at cell " + std::to_string(i);
        if (cost[i] > base[i]) { f.Add("AC13 zoneCost above zoneCostBase" + at); }
        if (coastDist[i] == 1u && c.depthZone != DepthZone::FirstShores) { f.Add("AC13 coast cell not First Shores" + at); }
        Range& r = perLandmass[c.landmass];
        switch (c.depthZone)
        {
        case DepthZone::FirstShores:      r.maxFirst = std::max(r.maxFirst, cost[i]); break;
        case DepthZone::WildReaches:      r.minWild = std::min(r.minWild, cost[i]); break;
        case DepthZone::AncientInteriors:
            r.minAncient = std::min(r.minAncient, cost[i]);
            if (c.landmass != w.mainLandmass) { f.Add("AC13 Ancient off the main landmass" + at); }
            if (coastDist[i] < 3u) { f.Add("AC13 Ancient within 3 steps of the coast" + at); }
            break;
        default: f.Add("AC13 land cell without a depth zone" + at); break;
        }
    }
    // Quantile thresholds: First <= t1 < Wild, Ancient >= t2 > t1 (the spec's strict First < Wild < Ancient
    // ordering does not hold across the depth-test demotion; see plan task 10).
    for (const auto& kv : perLandmass)
    {
        const Range& r = kv.second;
        if (r.maxFirst > d.t1) { f.Add("AC13 First Shores cost above t1"); }
        if (r.minWild <= d.t1) { f.Add("AC13 Wild Reaches cost at or below t1"); }
        if (r.minAncient < 1e29f && !(r.minAncient >= d.t2 && d.t2 > d.t1)) { f.Add("AC13 Ancient below t2 or t1 >= t2"); }
    }
    if (d.hasAncient && !(d.t1 < d.t2)) { f.Add("AC13 hasAncient but t1 >= t2"); }
}

// Everything checkable from one generated world (AC2, AC3, AC4, AC10, AC11, AC13).
inline ShapeFailures CheckWorldShape(const TerrainGenerator& g, const WorldStructure& w, const TerrainRules& rules)
{
    ShapeFailures f;
    CheckContractAndBorderAndLand(g, w, rules, f);
    if (f.Empty())
    {
        CheckRiversReachAndDescend(w, f);
        CheckLakeOutflow(g, w, f);
        CheckDepthZoneRules(g, w, f);
    }
    return f;
}

// AC9 for a passes = 0 world: the ridge cells form exactly one connected group.
inline void CheckSingleRidgeGroup(const TerrainGenerator& g, GridDimensions dims, ShapeFailures& f)
{
    const float* ridge = ShapeF(g, "ridgeMask");
    const int groups = CountConnectedGroups([&](int i) { return ridge[i] >= kShapeRidgeMin; }, dims);
    if (groups != 1) { f.Add("AC9 ridge has " + std::to_string(groups) + " connected groups with passes = 0"); }
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
