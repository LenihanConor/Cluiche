// S-1 Reachability Validator: navigation query. See NavigationQuery.h.
#pragma fp_contract(off)

#include <Shared/WorldGeneration/NavigationQuery.h>

#include <algorithm>
#include <limits>
#include <queue>
#include <utility>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexPathGrid;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::IPathCostProvider;

namespace
{
    int SafeDim(int v) { return v > 0 ? v : 1; }
}

NavigationQuery::NavigationQuery(const WorldStructure& world, std::span<const INavigationProfile* const> profiles)
    : mWorld(world)
    , mHasCells(world.dimensions.width > 0 && world.dimensions.height > 0 &&
                world.CellCount() == world.dimensions.width * world.dimensions.height)
    , mGrid(SafeDim(world.dimensions.width), SafeDim(world.dimensions.height))
    , mTopology(SafeDim(world.dimensions.width), SafeDim(world.dimensions.height))
{
    for (const INavigationProfile* p : profiles)
    {
        if (!p) { continue; }
        mCosts.push_back(p->CreateCostProvider(world));
        mProfileCrc.push_back(p->GetId().Value());
    }
}

NavigationQuery::NavigationQuery(const WorldStructure& world, const ValidationParams& params)
    : NavigationQuery(world, std::span<const INavigationProfile* const>(
          params.profiles.Size() > 0 ? &params.profiles.Front() : nullptr, params.profiles.Size()))
{
}

bool NavigationQuery::IsReachable(uint32_t from, uint32_t to) const
{
    ProofPath scratch;
    return FindProofPath(from, to, scratch);
}

bool NavigationQuery::FindProofPath(uint32_t from, uint32_t to, ProofPath& out) const
{
    const uint32_t targets[1] = { to };
    return FindProofPathToAny(from, targets, out);
}

bool NavigationQuery::FindProofPathToAny(uint32_t from, std::span<const uint32_t> targets, ProofPath& out) const
{
    if (!mHasCells || targets.empty() || from >= mTopology.CellCount()) { return false; }
    for (size_t i = 0; i < mCosts.size(); ++i)
    {
        if (!mCosts[i]) { continue; }
        if (Search(i, from, targets, out)) { return true; }
    }
    return false;
}

bool NavigationQuery::Search(size_t profileIndex, uint32_t from, std::span<const uint32_t> targets, ProofPath& out) const
{
    const uint32_t n = mTopology.CellCount();
    std::vector<uint8_t> isTarget(n, 0);
    bool anyTarget = false;
    for (uint32_t t : targets)
    {
        if (t < n) { isTarget[t] = 1; anyTarget = true; }
    }
    if (!anyTarget) { return false; }

    const IPathCostProvider& costs = *mCosts[profileIndex];
    constexpr float kInf = std::numeric_limits<float>::infinity();
    std::vector<float> dist(n, kInf);
    std::vector<uint32_t> prev(n, kNoCellIndex);
    std::vector<uint8_t> closed(n, 0);

    using Entry = std::pair<float, uint32_t>;   // (cost, cell): ties pop the lowest cell index
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    dist[from] = 0.0f;
    open.push({ 0.0f, from });

    Dia::Core::Containers::DynamicArrayC<CellCoord, 16> neighbours;
    while (!open.empty())
    {
        const Entry top = open.top();
        open.pop();
        const uint32_t cur = top.second;
        if (closed[cur]) { continue; }
        closed[cur] = 1;

        if (isTarget[cur])
        {
            out.from = from;
            out.to = cur;
            out.profileCrc = mProfileCrc[profileIndex];
            out.cells.Clear();
            std::vector<uint32_t> reversed;
            for (uint32_t c = cur; c != kNoCellIndex; c = prev[c]) { reversed.push_back(c); }
            for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) { out.cells.Add(*it); }
            return true;
        }

        const CellCoord curCoord = mTopology.ToCoord(cur);
        mGrid.GetNeighbours(curCoord, neighbours);
        for (int i = 0; i < neighbours.Size(); ++i)
        {
            const CellCoord nc = neighbours[i];
            const uint32_t ni = mTopology.ToIndex(nc);
            if (closed[ni]) { continue; }
            const float edge = costs.GetCost(curCoord, nc);
            if (edge < 0.0f) { continue; }   // IPathCostProvider::kImpassableCost
            const float cand = top.first + edge;
            if (cand < dist[ni])
            {
                dist[ni] = cand;
                prev[ni] = cur;
                open.push({ cand, ni });
            }
        }
    }
    return false;
}

} // namespace WorldGeneration
} // namespace CoW
