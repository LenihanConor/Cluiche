#pragma once
#include "DiaGeometry2D/Grid/HexTopology.h"
#include <cstdint>

namespace Dia::Pathfinding {
    // CIndexGraph adapter over Dia::Geometry2D::HexTopology. Neighbours are visited in direction order 0..5
    // (NE,N,NW,SW,S,SE); off-grid directions (Dia::Geometry2D::kNoCell) are skipped.
    class HexNeighbours {
    public:
        explicit HexNeighbours(const Dia::Geometry2D::HexTopology& topology) : mTopology(&topology) {}

        uint32_t CellCount() const { return mTopology->CellCount(); }

        template<class Fn>
        void ForEachNeighbour(uint32_t cell, Fn&& fn) const {
            for (uint32_t dir = 0; dir < Dia::Geometry2D::kHexDirections; ++dir) {
                const uint32_t nb = mTopology->NeighbourAt(cell, dir);
                if (nb != Dia::Geometry2D::kNoCell) {
                    fn(nb);
                }
            }
        }

    private:
        const Dia::Geometry2D::HexTopology* mTopology;
    };
}
