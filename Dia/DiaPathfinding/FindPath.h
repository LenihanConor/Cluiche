#pragma once

#include "DiaPathfinding/CPathGraph.h"
#include "DiaPathfinding/PathResult.h"
#include "DiaPathfinding/IPathCostProvider.h"
#include "DiaPathfinding/SquarePathGrid.h"
#include "DiaPathfinding/HexPathGrid.h"

#include <unordered_map>
#include <queue>
#include <vector>
#include <cmath>
#include <algorithm>
#include <type_traits>

namespace {

    struct CellCoordHash {
        size_t operator()(Dia::Pathfinding::CellCoord c) const noexcept {
            return std::hash<int>{}(c.x) ^ (std::hash<int>{}(c.y) << 16);
        }
    };

} // anonymous namespace

namespace Dia::Pathfinding {

    template<CPathGraph TGraph>
    PathResult FindPath(const TGraph& graph, CellCoord from, CellCoord to, IPathCostProvider& costs)
    {
        // --- heuristic selection via if constexpr ---
        auto heuristic = [&](CellCoord a, CellCoord b) -> float
        {
            const int dx = std::abs(a.x - b.x);
            const int dy = std::abs(a.y - b.y);

            if constexpr (std::is_same_v<TGraph, SquarePathGrid>)
            {
                if (graph.GetConnectivity() == SquareConnectivity::k8Connected)
                {
                    // Chebyshev distance
                    return static_cast<float>(std::max(dx, dy));
                }
                else
                {
                    // Manhattan distance (4-connected)
                    return static_cast<float>(dx + dy);
                }
            }
            else if constexpr (std::is_same_v<TGraph, HexPathGrid>)
            {
                // Axial hex cube distance: (|dq| + |dr| + |dq+dr|) / 2
                return static_cast<float>((dx + dy + std::abs(dx - dy)) / 2);
            }
            else
            {
                // Safe default: Manhattan
                return static_cast<float>(dx + dy);
            }
        };

        // --- early-out: trivial path ---
        if (from == to)
        {
            PathResult result;
            result.success   = true;
            result.totalCost = 0.0f;
            result.cells.Add(from);
            return result;
        }

        // --- A* open set: (f_score, CellCoord), min-heap ---
        using OpenEntry = std::pair<float, CellCoord>;
        struct MinHeap {
            bool operator()(const OpenEntry& a, const OpenEntry& b) const noexcept {
                return a.first > b.first; // greater = lower priority in std::priority_queue
            }
        };
        std::priority_queue<OpenEntry, std::vector<OpenEntry>, MinHeap> openSet;

        std::unordered_map<CellCoord, float,     CellCoordHash> gScore;
        std::unordered_map<CellCoord, CellCoord, CellCoordHash> cameFrom;

        gScore[from] = 0.0f;
        openSet.push({ heuristic(from, to), from });

        Dia::Core::Containers::DynamicArrayC<CellCoord, 16> neighbours;

        while (!openSet.empty())
        {
            const auto [fCurrent, current] = openSet.top();
            openSet.pop();

            // Reached the goal
            if (current == to)
            {
                // Reconstruct path from `to` back to `from`
                std::vector<CellCoord> reversed;
                CellCoord step = to;
                while (!(step == from))
                {
                    reversed.push_back(step);
                    step = cameFrom[step];
                }
                reversed.push_back(from);

                std::reverse(reversed.begin(), reversed.end());

                PathResult result;
                result.success   = true;
                result.totalCost = gScore[to];
                for (const CellCoord& cell : reversed)
                {
                    result.cells.Add(cell);
                }
                return result;
            }

            // Stale entry guard: if we already found a better path to `current`, skip
            const float gCurrent = gScore.count(current) ? gScore[current] : std::numeric_limits<float>::infinity();
            if (fCurrent > gCurrent + heuristic(current, to) + 1e-6f)
            {
                continue;
            }

            // Expand neighbours
            graph.GetNeighbours(current, neighbours);
            const int neighbourCount = neighbours.Size();
            for (int i = 0; i < neighbourCount; ++i)
            {
                const CellCoord neighbour = neighbours[i];

                const float edgeCost = costs.GetCost(current, neighbour);
                if (edgeCost < 0.0f) // IPathCostProvider::kImpassableCost sentinel
                {
                    continue;
                }

                const float tentativeG = gCurrent + edgeCost;

                auto it = gScore.find(neighbour);
                if (it == gScore.end() || tentativeG < it->second)
                {
                    gScore[neighbour]   = tentativeG;
                    cameFrom[neighbour] = current;
                    const float f = tentativeG + heuristic(neighbour, to);
                    openSet.push({ f, neighbour });
                }
            }
        }

        // Open set exhausted — no path found
        return PathResult{};
    }

} // namespace Dia::Pathfinding
