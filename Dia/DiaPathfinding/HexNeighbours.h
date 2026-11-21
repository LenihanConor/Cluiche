#pragma once
#include "DiaPathfinding/HexTopology.h"
#include <cstdint>

namespace Dia::Pathfinding {
    // CIndexGraph adapter over HexTopology. Neighbours are visited in direction order 0..5
    // (NE,N,NW,SW,S,SE); off-grid directions (kNoCell) are skipped.
    class HexNeighbours {
    public:
        explicit HexNeighbours(const HexTopology& topology) : mTopology(&topology) {}

        uint32_t CellCount() const { return mTopology->CellCount(); }

        template<class Fn>
        void ForEachNeighbour(uint32_t cell, Fn&& fn) const {
            for (uint32_t dir = 0; dir < kHexDirections; ++dir) {
                const uint32_t nb = mTopology->NeighbourAt(cell, dir);
                if (nb != kNoCell) {
                    fn(nb);
                }
            }
        }

    private:
        const HexTopology* mTopology;
    };
}
