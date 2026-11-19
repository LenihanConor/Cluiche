#pragma once
// AdjacencyGraph.h - directed CSR adjacency graph over dense cell indices, satisfying CIndexGraph.
// Storage is DynamicArray based (no STL, PD-004). Edges are sorted by (from, to) at Build time, so
// neighbour order is deterministic whatever the input order. Duplicate edges are kept.

#include <cstdint>

#include "DiaCore/Containers/Arrays/DynamicArray.h"
#include "DiaCore/Core/Assert.h"
#include "DiaPathfinding/GridSearch.h"

namespace Dia::Pathfinding {

    struct AdjacencyEdge {
        uint32_t from;
        uint32_t to;
    };

    class AdjacencyGraph {
    public:
        AdjacencyGraph() : mCellCount(0) {}

        // Builds (or rebuilds, reusing capacity) from `edgeCount` directed edges. Edges with from or
        // to >= cellCount assert in Debug and are ignored in Release.
        void Build(uint32_t cellCount, const AdjacencyEdge* edges, uint32_t edgeCount)
        {
            mCellCount = cellCount;
            Assign(mOffsets, cellCount + 1u);
            Assign(mCursor, cellCount + 1u);

            // Pass 1: stable counting sort by `to` into mPairs (flattened from, to).
            uint32_t valid = 0;
            for (uint32_t i = 0; i < edgeCount; ++i)
            {
                const bool ok = edges[i].from < cellCount && edges[i].to < cellCount;
                DIA_ASSERT(ok, "AdjacencyGraph::Build edge index out of range");
                if (ok) { ++mOffsets[edges[i].to + 1u]; ++valid; }
            }
            for (uint32_t c = 0; c < cellCount; ++c) { mOffsets[c + 1u] += mOffsets[c]; }
            Assign(mPairs, valid * 2u);
            for (uint32_t i = 0; i < edgeCount; ++i)
            {
                if (edges[i].from >= cellCount || edges[i].to >= cellCount) { continue; }
                const uint32_t slot = mOffsets[edges[i].to]++;
                mPairs[slot * 2u] = edges[i].from;
                mPairs[slot * 2u + 1u] = edges[i].to;
            }

            // Pass 2: stable counting sort by `from` into CSR.
            for (uint32_t c = 0; c <= cellCount; ++c) { mOffsets[c] = 0; }
            for (uint32_t i = 0; i < valid; ++i) { ++mOffsets[mPairs[i * 2u] + 1u]; }
            for (uint32_t c = 0; c < cellCount; ++c) { mOffsets[c + 1u] += mOffsets[c]; }
            for (uint32_t c = 0; c < cellCount; ++c) { mCursor[c] = mOffsets[c]; }
            Assign(mNeighbours, valid);
            for (uint32_t i = 0; i < valid; ++i)
            {
                mNeighbours[mCursor[mPairs[i * 2u]]++] = mPairs[i * 2u + 1u];
            }
        }

        uint32_t CellCount() const { return mCellCount; }

        uint32_t NeighbourCount(uint32_t c) const
        {
            DIA_ASSERT(c < mCellCount, "AdjacencyGraph::NeighbourCount cell out of range");
            return c < mCellCount ? mOffsets[c + 1u] - mOffsets[c] : 0u;
        }

        // Visits `to` of every edge with from == c, ascending `to` (duplicates repeated).
        template<class Fn>
        void ForEachNeighbour(uint32_t c, Fn&& fn) const
        {
            if (c >= mCellCount) { return; }
            const uint32_t end = mOffsets[c + 1u];
            for (uint32_t i = mOffsets[c]; i < end; ++i) { fn(mNeighbours[i]); }
        }

    private:
        using Array = Dia::Core::Containers::DynamicArray<uint32_t>;

        // Sets the array to `count` zeroed elements, keeping capacity when large enough.
        static void Assign(Array& array, uint32_t count)
        {
            array.RemoveAll();
            EnsureCapacity(array, count);
            for (uint32_t i = 0; i < count; ++i) { array.Add(0u); }
        }

        uint32_t mCellCount;
        Array mOffsets;     // cellCount + 1
        Array mNeighbours;  // valid edge count
        Array mCursor;      // build scratch
        Array mPairs;       // build scratch
    };

    static_assert(CIndexGraph<AdjacencyGraph>);
}
