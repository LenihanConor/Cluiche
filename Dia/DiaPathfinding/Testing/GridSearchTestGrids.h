#pragma once
// GridSearchTestGrids.h - hand-built graphs with known answers for the grid search tests.

#include "DiaPathfinding/GridSearch.h"
#include "DiaPathfinding/HexTopology.h"
#include <cstdint>
#include <utility>
#include <vector>

namespace Dia::Pathfinding::Testing {
    // Fixed small hex grid used across the grid search tests: 4 wide, 4 tall, index = y * 4 + x.
    inline HexTopology MakeSmallHexGrid() { return HexTopology(4, 4); }

    // N cells in a line; neighbours of i are i-1 then i+1 (when they exist).
    class ChainGraph {
    public:
        explicit ChainGraph(uint32_t cellCount) : mCount(cellCount) {}

        uint32_t CellCount() const { return mCount; }

        template<class Fn>
        void ForEachNeighbour(uint32_t c, Fn&& fn) const {
            if (c > 0) { fn(c - 1); }
            if (c + 1 < mCount) { fn(c + 1); }
        }

    private:
        uint32_t mCount;
    };

    // Directed child graph from a parent-index array (parent[i] == kNoCell for roots):
    // neighbours of c are its children in ascending index. A thin fixture; later wraps AdjacencyGraph.
    class TreeGraph {
    public:
        explicit TreeGraph(std::vector<uint32_t> parent) : mParent(std::move(parent)) {}

        uint32_t CellCount() const { return static_cast<uint32_t>(mParent.size()); }

        template<class Fn>
        void ForEachNeighbour(uint32_t c, Fn&& fn) const {
            for (uint32_t i = 0; i < CellCount(); ++i) {
                if (mParent[i] == c) { fn(i); }
            }
        }

    private:
        std::vector<uint32_t> mParent;
    };
}
