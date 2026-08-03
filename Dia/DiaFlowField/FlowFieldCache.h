#pragma once
#include "DiaFlowField/CFlowFieldGraph.h"
#include "DiaFlowField/FlowField.h"
#include "DiaFlowField/ComputeFlowField.h"
#include "DiaFlowField/FlowFieldLogChannel.h"
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>
#include <unordered_map>

namespace Dia::FlowField {

    using FlowFieldKey = Dia::Core::StringCRC;

    // Caches multiple named flow fields over a shared graph.
    // One cache per graph instance; all fields share the same topology and cost source.
    // Fields are computed lazily on first access and recomputed when marked dirty.
    template<CFlowFieldGraph TGraph>
    class FlowFieldCache
    {
    public:
        FlowFieldCache(const TGraph& graph,
                       Dia::Pathfinding::IPathCostProvider& costs,
                       int width, int height)
            : mGraph(graph)
            , mCosts(costs)
            , mWidth(width)
            , mHeight(height)
        {}

        // Returns existing field if clean; recomputes if dirty or absent.
        const FlowField& GetOrCompute(FlowFieldKey key, Dia::Pathfinding::CellCoord goalCell)
        {
            auto it = mEntries.find(key);
            if (it == mEntries.end())
            {
                // New entry: compute and store
                CacheEntry entry;
                entry.goalCell = goalCell;
                entry.field    = ComputeFlowField(mGraph, goalCell, mCosts, mWidth, mHeight);
                entry.dirty    = false;
                auto [inserted_it, ok] = mEntries.emplace(key, std::move(entry));
                DIA_LOG_INFO("FlowField", "FlowFieldCache: computed new field key=%s goal=(%d,%d)",
                             key.AsChar(), goalCell.x, goalCell.y);
                return inserted_it->second.field;
            }
            if (it->second.dirty)
            {
                // Recompute dirty field
                it->second.goalCell = goalCell;
                it->second.field    = ComputeFlowField(mGraph, goalCell, mCosts, mWidth, mHeight);
                it->second.dirty    = false;
                DIA_LOG_INFO("FlowField", "FlowFieldCache: recomputed dirty field key=%s goal=(%d,%d)",
                             key.AsChar(), goalCell.x, goalCell.y);
            }
            return it->second.field;
        }

        // Mark a specific field dirty — recomputed on next GetOrCompute.
        void Invalidate(FlowFieldKey key)
        {
            auto it = mEntries.find(key);
            if (it != mEntries.end())
            {
                it->second.dirty = true;
                DIA_LOG_INFO("FlowField", "FlowFieldCache: invalidated key=%s", key.AsChar());
            }
        }

        // Mark all fields dirty — use when a broad terrain change affects many cells.
        void InvalidateAll()
        {
            for (auto& [key, entry] : mEntries)
                entry.dirty = true;
            DIA_LOG_INFO("FlowField", "FlowFieldCache: invalidated all (%d fields)", (int)mEntries.size());
        }

        // Mark all fields dirty whose goal cell falls within [topLeft, bottomRight] inclusive.
        void InvalidateRegion(Dia::Pathfinding::CellCoord topLeft,
                              Dia::Pathfinding::CellCoord bottomRight)
        {
            int count = 0;
            for (auto& [key, entry] : mEntries)
            {
                if (entry.goalCell.x >= topLeft.x && entry.goalCell.x <= bottomRight.x
                 && entry.goalCell.y >= topLeft.y && entry.goalCell.y <= bottomRight.y)
                {
                    entry.dirty = true;
                    ++count;
                }
            }
            DIA_LOG_INFO("FlowField", "FlowFieldCache: invalidate region (%d,%d)-(%d,%d) marked %d dirty",
                         topLeft.x, topLeft.y, bottomRight.x, bottomRight.y, count);
        }

        int GetCachedCount() const { return (int)mEntries.size(); }

    private:
        struct CacheEntry
        {
            FlowField field;
            bool dirty = true;
            Dia::Pathfinding::CellCoord goalCell{};

            CacheEntry() : field(1, 1) {}  // dummy default — replaced on first compute
        };

        const TGraph&                          mGraph;
        Dia::Pathfinding::IPathCostProvider&   mCosts;
        int                                    mWidth;
        int                                    mHeight;
        // std::hash<Dia::Core::StringCRC> is already specialised in StringCRC.h
        std::unordered_map<FlowFieldKey, CacheEntry> mEntries;
    };

} // namespace Dia::FlowField
