#pragma once
#include "DiaFlowField/CFlowFieldGraph.h"
#include "DiaFlowField/FlowField.h"
#include "DiaFlowField/FlowFieldLogChannel.h"
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <limits>
#include <cmath>

namespace Dia::FlowField {

    //-------------------------------------------------------------------------
    // Internal hasher for CellCoord — used only within ComputeFlowField.
    //-------------------------------------------------------------------------
    struct CellCoordHash
    {
        size_t operator()(Dia::Pathfinding::CellCoord c) const noexcept
        {
            return std::hash<long long>()(((long long)c.x << 32) | (unsigned int)c.y);
        }
    };

    //-------------------------------------------------------------------------
    // ComputeFlowField
    //
    // Runs a synchronous Dijkstra sweep from goalCell outward across the graph.
    // All passable cells receive a direction (unit vector in grid-space) pointing
    // toward the lowest-cost path to goal.  Impassable cells and cells that
    // cannot be reached from goal have reachable = false.
    //
    // Parameters:
    //   graph      - satisfies CFlowFieldGraph (GetNeighbours, IsPassable,
    //                CellToWorldPosition, WorldToCell)
    //   goalCell   - the destination cell (cost = 0, direction = {0,0})
    //   costs      - edge-cost provider; GetCost < 0 means impassable edge
    //   width      - grid width  (columns); used to size the returned FlowField
    //   height     - grid height (rows);   used to size the returned FlowField
    //
    // Returns a FlowField with per-cell direction vectors and reachability flags.
    //-------------------------------------------------------------------------
    template<CFlowFieldGraph TGraph>
    FlowField ComputeFlowField(const TGraph&                         graph,
                               Dia::Pathfinding::CellCoord           goalCell,
                               Dia::Pathfinding::IPathCostProvider&  costs,
                               int                                   width,
                               int                                   height)
    {
        DIA_LOG_INFO("FlowField", "FlowField: compute start goal=(%d,%d)", goalCell.x, goalCell.y);
        auto t0 = std::chrono::steady_clock::now();

        FlowField field(width, height);

        // Distance map: cell -> best cost from goalCell
        std::unordered_map<Dia::Pathfinding::CellCoord, float, CellCoordHash> dist;

        // Parent map: cell -> the neighbour that relaxed it (one step closer to goal)
        std::unordered_map<Dia::Pathfinding::CellCoord,
                           Dia::Pathfinding::CellCoord,
                           CellCoordHash> parent;

        // Min-heap ordered by cost
        using Entry = std::pair<float, Dia::Pathfinding::CellCoord>;
        auto cmp = [](const Entry& a, const Entry& b) { return a.first > b.first; };
        std::priority_queue<Entry, std::vector<Entry>, decltype(cmp)> open(cmp);

        dist[goalCell] = 0.0f;
        open.push({ 0.0f, goalCell });

        Dia::Core::Containers::DynamicArrayC<Dia::Pathfinding::CellCoord, 16> neighbours;
        int cellsVisited = 0;

        while (!open.empty())
        {
            auto [currentCost, current] = open.top();
            open.pop();

            // Skip stale entries (cell was already settled at a lower cost)
            auto distIt = dist.find(current);
            if (distIt != dist.end() && currentCost > distIt->second)
                continue;

            ++cellsVisited;

            neighbours.RemoveAll();
            graph.GetNeighbours(current, neighbours);

            for (unsigned int i = 0; i < neighbours.Size(); ++i)
            {
                Dia::Pathfinding::CellCoord nb = neighbours[i];

                if (!graph.IsPassable(nb))
                    continue;

                float edgeCost = costs.GetCost(current, nb);
                if (edgeCost < 0.0f)   // kImpassableCost sentinel
                    continue;

                float tentative = currentCost + edgeCost;

                auto it = dist.find(nb);
                if (it == dist.end() || tentative < it->second)
                {
                    dist[nb]   = tentative;
                    parent[nb] = current;
                    open.push({ tentative, nb });
                }
            }
        }

        // Build per-cell direction vectors from the parent map.
        // Direction is the unit vector (in grid-space) pointing from this cell
        // toward its parent (i.e. the next step toward the goal).
        for (auto& [cell, p] : parent)
        {
            float dx  = static_cast<float>(p.x - cell.x);
            float dy  = static_cast<float>(p.y - cell.y);
            float len = std::sqrtf(dx * dx + dy * dy);
            if (len > 0.0f) { dx /= len; dy /= len; }

            FlowCell& fc   = field.AccessCell(cell);
            fc.direction   = Dia::Maths::Vector2D(dx, dy);
            fc.reachable   = true;
        }

        // Goal cell itself: reachable, direction is zero vector (agent has arrived)
        field.AccessCell(goalCell).reachable = true;

        auto t1 = std::chrono::steady_clock::now();
        int elapsedMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

        DIA_LOG_INFO("FlowField", "FlowField: compute done cells=%d ms=%d", cellsVisited, elapsedMs);

        return field;
    }

} // namespace Dia::FlowField
