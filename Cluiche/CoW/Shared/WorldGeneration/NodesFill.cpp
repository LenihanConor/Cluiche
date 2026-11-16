// S-1 Nodes: Hunting and Resource fill. See NodesFill.h.
#include <Shared/WorldGeneration/NodesFill.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <numeric>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;

namespace
{
    constexpr int kMinQuota = 6;
    constexpr int kMaxQuota = 15;
    constexpr uint32_t kMinTotal = 80;
    constexpr uint32_t kMaxTotal = 150;

    // GDD yield table (High 3, Medium 2, Low 1), indexed by Biome: { Food, Materials }.
    constexpr int kYield[kBiomeCount][2] = {
        { 0, 0 },   // Ocean
        { 0, 0 },   // Lake
        { 3, 1 },   // GrasslandSteppe
        { 2, 3 },   // TemperateForest
        { 2, 1 },   // WetlandDelta
        { 1, 2 },   // DesertBasin
        { 1, 3 },   // AlpineMountain
        { 1, 2 },   // TundraBoreal
        { 3, 2 },   // Rainforest
        { 1, 3 },   // VolcanicAshland
        { 0, 0 },   // Scrub (succession; no nodes placed on generated worlds)
        { 0, 0 },   // BurnedAshland
        { 0, 0 },   // DesertifiedPlain
    };

    int Clamp(int v) { return std::clamp(v, kMinQuota, kMaxQuota); }
}

int BiomeFood(Biome b)
{
    const int i = static_cast<int>(b);
    return (i >= 0 && i < kBiomeCount) ? kYield[i][0] : 0;
}

int BiomeMaterials(Biome b)
{
    const int i = static_cast<int>(b);
    return (i >= 0 && i < kBiomeCount) ? kYield[i][1] : 0;
}

float BiomeFoodShare(Biome b)
{
    const int f = BiomeFood(b), m = BiomeMaterials(b);
    return (f + m) > 0 ? static_cast<float>(f) / static_cast<float>(f + m) : 0.0f;
}

int TerrainFactor(FillKind kind, TerrainType t)
{
    if (kind == FillKind::Hunting)
    {
        switch (t)
        {
        case TerrainType::Lowland:
        case TerrainType::Hills:
        case TerrainType::Highland: return 3;
        case TerrainType::Mountain:
        case TerrainType::Cliff:    return 1;
        default:                    return 0;
        }
    }
    switch (t)
    {
    case TerrainType::Hills:
    case TerrainType::Highland:
    case TerrainType::Mountain:
    case TerrainType::Cliff:    return 3;
    case TerrainType::Lowland:  return 1;
    default:                    return 0;
    }
}

int CellWeight(FillKind kind, const Cell& cell)
{
    if (cell.landmass == 0 || cell.waterSource != WaterSource::None) { return 0; }
    const int yield = kind == FillKind::Hunting ? BiomeFood(cell.biome) : BiomeMaterials(cell.biome);
    return yield * TerrainFactor(kind, cell.terrain);
}

QuotaResult ComputeNodeQuotas(std::span<const uint16_t> cellCounts, int cellsPerNode)
{
    QuotaResult out;
    const int cpn = std::max(1, cellsPerNode);
    const size_t n = cellCounts.size();
    out.quota.resize(n);
    uint32_t total = 0;
    for (size_t i = 0; i < n; ++i)
    {
        out.quota[i] = static_cast<uint16_t>(Clamp((2 * static_cast<int>(cellCounts[i]) + cpn) / (2 * cpn)));   // round half up
        total += out.quota[i];
    }

    if (total < kMinTotal || total > kMaxTotal)
    {
        // Proportional scale toward the nearest bound (integer rounding half up), re-clamped.
        const uint64_t target = total < kMinTotal ? kMinTotal : kMaxTotal;
        const uint64_t sum = std::max<uint32_t>(total, 1);
        total = 0;
        for (size_t i = 0; i < n; ++i)
        {
            out.quota[i] = static_cast<uint16_t>(Clamp(static_cast<int>((2 * out.quota[i] * target + sum) / (2 * sum))));
            total += out.quota[i];
        }

        // Nudge +-1 per region (largest cellCount first, ties lowest id), sweeping until in range or stuck.
        std::vector<size_t> order(n);
        std::iota(order.begin(), order.end(), size_t{ 0 });
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return cellCounts[a] > cellCounts[b]; });
        bool moved = true;
        while (total < kMinTotal && moved)
        {
            moved = false;
            for (size_t i : order)
            {
                if (total >= kMinTotal) { break; }
                if (out.quota[i] < kMaxQuota) { ++out.quota[i]; ++total; moved = true; }
            }
        }
        moved = true;
        while (total > kMaxTotal && moved)
        {
            moved = false;
            for (size_t i : order)
            {
                if (total <= kMaxTotal) { break; }
                if (out.quota[i] > kMinQuota) { --out.quota[i]; --total; moved = true; }
            }
        }
    }
    out.total = total;
    out.nodeLimited = total < kMinTotal || total > kMaxTotal;
    return out;
}

FillResult PlaceFillNodes(WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng)
{
    FillResult out;
    const int n = world.CellCount();
    const unsigned nReg = world.regions.regions.Size();

    std::vector<uint16_t> counts(nReg);
    for (unsigned r = 0; r < nReg; ++r) { counts[r] = world.regions.regions[r].cellCount; }
    QuotaResult q = ComputeNodeQuotas(counts, rules.GetInt(Rule::kCellsPerNode));
    out.quota        = std::move(q.quota);
    out.quotaTotal   = q.total;
    out.quotaLimited = q.nodeLimited;
    out.nodeLimited  = q.nodeLimited;
    out.existing.assign(nReg, 0);
    out.placed.assign(nReg, 0);
    if (n == 0 || nReg == 0) { return out; }

    const HexTopology topo(world.dimensions.width, world.dimensions.height);
    const Cell* cells = world.Cells();
    const int spacing = rules.GetInt(Rule::kNodeSpacing);
    const uint32_t kinds[4] = { SiteKindId(BuiltinNodeKinds::Corridor()), SiteKindId(BuiltinNodeKinds::River()),
                                SiteKindId(BuiltinNodeKinds::Hunting()),  SiteKindId(BuiltinNodeKinds::Resource()) };

    // blocked[c] = a built-in node lies within fewer than kNodeSpacing steps of c (itself included).
    std::vector<uint8_t> blocked(static_cast<size_t>(n), 0);
    auto block = [&](uint32_t cell) {
        const CellCoord o = topo.ToCoord(cell);
        for (int dy = -spacing; dy <= spacing; ++dy)
        {
            for (int dx = -spacing; dx <= spacing; ++dx)
            {
                const int x = o.x + dx, y = o.y + dy;
                if (x < 0 || y < 0 || x >= world.dimensions.width || y >= world.dimensions.height) { continue; }
                if (static_cast<int>(HexTopology::Distance(o, CellCoord{ x, y })) < spacing)
                {
                    blocked[static_cast<size_t>(y) * world.dimensions.width + x] = 1;
                }
            }
        }
    };
    for (uint32_t kind : kinds)
    {
        for (const Site& s : world.sites.Of(kind))
        {
            block(s.cell);
            const RegionId r = cells[s.cell].region;
            if (r >= 1 && r <= nReg) { ++out.existing[r - 1u]; }
        }
    }

    std::vector<std::vector<uint32_t>> cellsOf(nReg);
    for (int i = 0; i < n; ++i)
    {
        const RegionId r = cells[i].region;
        if (r >= 1 && r <= nReg) { cellsOf[r - 1u].push_back(static_cast<uint32_t>(i)); }
    }

    for (unsigned r = 0; r < nReg; ++r)
    {
        const int slots = static_cast<int>(out.quota[r]) - static_cast<int>(out.existing[r]);
        const float share = BiomeFoodShare(world.regions.regions[r].primaryBiome);
        for (int slot = 0; slot < slots; ++slot)
        {
            const FillKind kind = DrawIsHunting(rng.RandomUnit(), share) ? FillKind::Hunting : FillKind::Resource;
            int64_t totalWeight = 0;
            for (uint32_t c : cellsOf[r]) { if (!blocked[c]) { totalWeight += CellWeight(kind, cells[c]); } }
            if (totalWeight == 0)
            {
                ++out.dropped;
                out.nodeLimited = true;
                continue;
            }
            int64_t pick = static_cast<int64_t>(rng.RandomInt()) % totalWeight;
            uint32_t chosen = cellsOf[r].front();
            for (uint32_t c : cellsOf[r])
            {
                if (blocked[c]) { continue; }
                const int w = CellWeight(kind, cells[c]);
                if (pick < w) { chosen = c; break; }
                pick -= w;
            }
            Site s;
            s.kindCrc = SiteKindId(kind == FillKind::Hunting ? BuiltinNodeKinds::Hunting() : BuiltinNodeKinds::Resource());
            s.cell    = chosen;
            s.region  = cells[chosen].region;
            if (world.sites.Add(s))
            {
                block(chosen);
                ++out.placed[r];
                if (kind == FillKind::Hunting) { ++out.placedHunting; } else { ++out.placedResource; }
            }
        }
    }
    return out;
}

} // namespace WorldGeneration
} // namespace CoW
