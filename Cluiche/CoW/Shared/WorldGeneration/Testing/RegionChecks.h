// S-1 Regions test support: independent checkers for the final region partition and records.
// Header-only. Deliberately shares NO code with RegionPartition / RegionRepair (own odd-q hex neighbour
// function, own land test, own recomputation of biome/purity/neighbours).
// Each Check* returns a list of failure messages; empty = pass.
#pragma once

#include <Shared/WorldGeneration/TerrainTypes.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

using CheckErrors = std::vector<std::string>;

inline bool ChkIsLand(const Cell& c) { return c.landmass != 0 && c.waterSource != WaterSource::Ocean; }

// Flat-top odd-q; directions NE N NW SW S SE. Returns -1 off the grid.
inline int ChkNeighbour(const WorldStructure& w, int idx, int dir)
{
    static const int dx[6]     = { 1, 0, -1, -1, 0, 1 };
    static const int dyEven[6] = { -1, -1, -1, 0, 1, 0 };
    static const int dyOdd[6]  = { 0, -1, 0, 1, 1, 1 };
    const int W = w.dimensions.width, H = w.dimensions.height;
    const int x = idx % W, y = idx / W;
    const int nx = x + dx[dir];
    const int ny = y + ((x & 1) ? dyOdd[dir] : dyEven[dir]);
    if (nx < 0 || ny < 0 || nx >= W || ny >= H) { return -1; }
    return ny * W + nx;
}

inline std::string ChkJoin(const CheckErrors& e, size_t maxLines = 8)
{
    std::ostringstream os;
    for (size_t i = 0; i < e.size() && i < maxLines; ++i) { os << "\n  " << e[i]; }
    if (e.size() > maxLines) { os << "\n  ... " << (e.size() - maxLines) << " more"; }
    return os.str();
}

inline int ChkLandCount(const WorldStructure& w)
{
    int n = 0;
    for (int c = 0; c < w.CellCount(); ++c) { n += ChkIsLand(w.Cells()[c]); }
    return n;
}

// Land has region in [1, regions.Size()], everything else 0.
inline CheckErrors CheckPartitionCoverage(const WorldStructure& w)
{
    CheckErrors e;
    const unsigned n = w.regions.regions.Size();
    for (int c = 0; c < w.CellCount(); ++c)
    {
        const Cell& cell = w.Cells()[c];
        if (ChkIsLand(cell))
        {
            if (cell.region < 1 || cell.region > n) { e.push_back("land cell " + std::to_string(c) + " has region " + std::to_string(cell.region)); }
        }
        else if (cell.region != 0) { e.push_back("non-land cell " + std::to_string(c) + " has region " + std::to_string(cell.region)); }
    }
    return e;
}

// Main-landmass regions are connected; no region spans two landmasses; island regions are whole landmasses.
inline CheckErrors CheckRegionShape(const WorldStructure& w)
{
    CheckErrors e;
    const int n = w.CellCount();
    const unsigned count = w.regions.regions.Size();
    std::vector<std::vector<int>> cellsOf(count + 1);
    for (int c = 0; c < n; ++c)
    {
        const unsigned r = w.Cells()[c].region;
        if (r >= 1 && r <= count) { cellsOf[r].push_back(c); }
    }
    for (unsigned r = 1; r <= count; ++r)
    {
        const RegionRecord& rec = w.regions.regions[r - 1];
        if (cellsOf[r].empty()) { e.push_back("region " + std::to_string(r) + " has no cells"); continue; }
        const uint8_t lm = w.Cells()[cellsOf[r][0]].landmass;
        for (int c : cellsOf[r])
        {
            if (w.Cells()[c].landmass != lm)
            {
                e.push_back("region " + std::to_string(r) + " spans two landmasses");
                break;
            }
        }
        if (rec.landmass != lm) { e.push_back("region " + std::to_string(r) + " record landmass mismatch"); }
        if (rec.isIsland != (lm != w.mainLandmass)) { e.push_back("region " + std::to_string(r) + " isIsland mismatch"); }
        if (lm == w.mainLandmass)
        {
            std::set<int> seen{ cellsOf[r][0] };
            std::vector<int> stack{ cellsOf[r][0] };
            while (!stack.empty())
            {
                const int c = stack.back(); stack.pop_back();
                for (int d = 0; d < 6; ++d)
                {
                    const int nb = ChkNeighbour(w, c, d);
                    if (nb < 0 || w.Cells()[nb].region != r || seen.count(nb)) { continue; }
                    seen.insert(nb); stack.push_back(nb);
                }
            }
            if (seen.size() != cellsOf[r].size()) { e.push_back("region " + std::to_string(r) + " is not connected"); }
        }
    }
    // Every non-main landmass is exactly one region.
    std::map<uint8_t, std::set<unsigned>> regionsOfLandmass;
    for (int c = 0; c < n; ++c)
    {
        const Cell& cell = w.Cells()[c];
        if (ChkIsLand(cell) && cell.landmass != w.mainLandmass) { regionsOfLandmass[cell.landmass].insert(cell.region); }
    }
    for (const auto& kv : regionsOfLandmass)
    {
        if (kv.second.size() != 1) { e.push_back("island landmass " + std::to_string(kv.first) + " spans " + std::to_string(kv.second.size()) + " regions"); }
    }
    return e;
}

// Ids 1..N appear in order of smallest cell index.
inline CheckErrors CheckIdOrder(const WorldStructure& w)
{
    CheckErrors e;
    unsigned next = 1;
    std::set<unsigned> seen;
    for (int c = 0; c < w.CellCount(); ++c)
    {
        const unsigned r = w.Cells()[c].region;
        if (r == 0 || seen.count(r)) { continue; }
        seen.insert(r);
        if (r != next) { e.push_back("cell " + std::to_string(c) + " first of region " + std::to_string(r) + ", expected " + std::to_string(next)); }
        ++next;
    }
    if (seen.size() != w.regions.regions.Size()) { e.push_back("records " + std::to_string(w.regions.regions.Size()) + " vs ids used " + std::to_string(seen.size())); }
    return e;
}

// Cells of non-island regions whose zone differs from the region's zone (absorbed + fragment cells).
inline uint32_t CountAbsorbedCells(const WorldStructure& w)
{
    uint32_t n = 0;
    for (int c = 0; c < w.CellCount(); ++c)
    {
        const Cell& cell = w.Cells()[c];
        if (cell.region == 0 || cell.region > w.regions.regions.Size()) { continue; }
        const RegionRecord& rec = w.regions.regions[cell.region - 1];
        if (!rec.isIsland && cell.depthZone != rec.zone) { ++n; }
    }
    return n;
}

// Zone purity excluding absorbed cells: zone-mismatched cells recounted independently equal the table's
// absorbedCells + fragmentCells (AC3 amendment: tiny-fragment cells are counted apart), and (maxShare >= 0)
// absorbedCells stays within maxShare * land.
inline CheckErrors CheckZonePurity(const WorldStructure& w, float maxShare)
{
    CheckErrors e;
    const uint32_t mismatched = CountAbsorbedCells(w);
    const uint32_t absorbed = w.regions.absorbedCells;
    if (mismatched != absorbed + w.regions.fragmentCells)
    {
        e.push_back("absorbedCells " + std::to_string(absorbed) + " + fragmentCells " + std::to_string(w.regions.fragmentCells) +
                    " vs recount " + std::to_string(mismatched));
    }
    if (maxShare >= 0.0f && absorbed > maxShare * static_cast<float>(ChkLandCount(w)))
    {
        e.push_back("absorbed " + std::to_string(absorbed) + " exceeds " + std::to_string(maxShare) + " of land " + std::to_string(ChkLandCount(w)));
    }
    return e;
}

// Neighbour sets from an independent hex-edge scan. Lists equal the scan (lowest kMaxNeighbours when the scan is
// bigger); symmetric when nothing was truncated.
inline CheckErrors CheckNeighbours(const WorldStructure& w)
{
    CheckErrors e;
    const unsigned count = w.regions.regions.Size();
    std::vector<std::set<unsigned>> scan(count + 1);
    for (int c = 0; c < w.CellCount(); ++c)
    {
        const unsigned r = w.Cells()[c].region;
        if (r == 0) { continue; }
        for (int d = 0; d < 6; ++d)
        {
            const int nb = ChkNeighbour(w, c, d);
            if (nb < 0) { continue; }
            const unsigned o = w.Cells()[nb].region;
            if (o != 0 && o != r) { scan[r].insert(o); }
        }
    }
    bool truncated = false;
    for (unsigned r = 1; r <= count; ++r)
    {
        const RegionRecord& rec = w.regions.regions[r - 1];
        std::vector<unsigned> expect(scan[r].begin(), scan[r].end());
        if (expect.size() > RegionRecord::kMaxNeighbours) { expect.resize(RegionRecord::kMaxNeighbours); truncated = true; }
        std::vector<unsigned> got;
        for (unsigned i = 0; i < rec.neighbours.Size(); ++i) { got.push_back(rec.neighbours[i]); }
        if (got != expect) { e.push_back("region " + std::to_string(r) + " neighbour list differs from edge scan"); }
        if (!std::is_sorted(got.begin(), got.end())) { e.push_back("region " + std::to_string(r) + " neighbours not ascending"); }
    }
    if (!truncated)
    {
        for (unsigned r = 1; r <= count; ++r)
        {
            for (unsigned o : scan[r])
            {
                if (!scan[o].count(r)) { e.push_back("neighbours not symmetric " + std::to_string(r) + "/" + std::to_string(o)); }
            }
        }
    }
    return e;
}

// Records: cellCount, primaryBiome (majority, Lake ignored, ties lowest enum), purity, centreCell.
inline CheckErrors CheckRecords(const WorldStructure& w)
{
    CheckErrors e;
    const unsigned count = w.regions.regions.Size();
    std::vector<std::vector<int>> cellsOf(count + 1);
    for (int c = 0; c < w.CellCount(); ++c)
    {
        const unsigned r = w.Cells()[c].region;
        if (r >= 1 && r <= count) { cellsOf[r].push_back(c); }
    }
    for (unsigned r = 1; r <= count; ++r)
    {
        const RegionRecord& rec = w.regions.regions[r - 1];
        const std::string tag = "region " + std::to_string(r) + ": ";
        if (rec.id != r) { e.push_back(tag + "id " + std::to_string(rec.id)); }
        if (rec.cellCount != cellsOf[r].size()) { e.push_back(tag + "cellCount " + std::to_string(rec.cellCount) + " vs " + std::to_string(cellsOf[r].size())); }
        if (cellsOf[r].empty()) { continue; }
        const int lakeIdx = static_cast<int>(::CoW::World::Testing::B("Biome.Lake"));
        int hist[256] = {};   // BiomeIndex is a uint8_t
        for (int c : cellsOf[r]) { ++hist[static_cast<int>(w.Cells()[c].biome)]; }
        int best = -1;
        for (int b = 0; b < 256; ++b)
        {
            if (b == lakeIdx || hist[b] == 0) { continue; }
            if (best < 0 || hist[b] > hist[best]) { best = b; }
        }
        if (best < 0) { best = lakeIdx; }
        if (static_cast<int>(rec.primaryBiome) != best) { e.push_back(tag + "primaryBiome " + std::to_string(int(rec.primaryBiome)) + " vs " + std::to_string(best)); }
        const float purity = static_cast<float>(hist[best]) / static_cast<float>(cellsOf[r].size());
        if (rec.purity < purity - 1e-5f || rec.purity > purity + 1e-5f) { e.push_back(tag + "purity " + std::to_string(rec.purity) + " vs " + std::to_string(purity)); }
        if (rec.centreCell >= static_cast<uint32_t>(w.CellCount()) || w.Cells()[rec.centreCell].region != r ||
            !ChkIsLand(w.Cells()[rec.centreCell]))
        {
            e.push_back(tag + "centreCell " + std::to_string(rec.centreCell) + " is not a land cell of the region");
        }
    }
    return e;
}

// Everything above. maxAbsorbedShare < 0 skips the share limit (hand-built worlds).
inline CheckErrors CheckAllRegions(const WorldStructure& w, float maxAbsorbedShare)
{
    CheckErrors all;
    for (const CheckErrors& part : { CheckPartitionCoverage(w), CheckRegionShape(w), CheckIdOrder(w),
                                     CheckZonePurity(w, maxAbsorbedShare), CheckNeighbours(w), CheckRecords(w) })
    {
        all.insert(all.end(), part.begin(), part.end());
    }
    return all;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
