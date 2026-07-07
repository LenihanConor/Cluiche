#pragma once
#include "DiaPathfinding/CellCoord.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Pathfinding {

    class HexPathGrid {
    public:
        explicit HexPathGrid(int radius);

        void SetPassable(CellCoord cell, bool passable);
        bool IsPassable(CellCoord cell) const;

        void GetNeighbours(CellCoord cell,
                           Dia::Core::Containers::DynamicArrayC<CellCoord, 16>& outNeighbours) const;

        int GetRadius() const;

    private:
        bool IsInBounds(CellCoord cell) const;
        int  CellIndex(CellCoord cell) const;

        int mRadius;

        // Bounding-box storage: (2R+1)*(2R+1) entries.
        // For radius up to 20: (2*20+1)^2 = 41*41 = 1681.
        static const unsigned int kMaxCells = 1681;
        Dia::Core::Containers::DynamicArrayC<bool, kMaxCells> mPassable;
    };

} // namespace Dia::Pathfinding
