#pragma once
#include "DiaPathfinding/CellCoord.h"
#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Pathfinding {

    enum class SquareConnectivity { k4Connected, k8Connected };

    // Rectangular square-grid pathfinding graph.
    // Storage is heap-allocated, sized to width * height.
    class SquarePathGrid {
    public:
        // Construct a width × height grid with all cells passable.
        // Asserts: width >= 1, height >= 1, width * height fits int.
        SquarePathGrid(int width, int height,
                       SquareConnectivity connectivity = SquareConnectivity::k8Connected);

        void SetPassable(CellCoord cell, bool passable);
        bool IsPassable(CellCoord cell) const;

        void GetNeighbours(CellCoord cell,
                           Dia::Core::Containers::DynamicArrayC<CellCoord, 16>& outNeighbours) const;

        int GetWidth()  const;
        int GetHeight() const;
        SquareConnectivity GetConnectivity() const;

#ifdef DIA_DEBUG
        template<typename Fn>
        void VisitCells(Fn&& fn) const
        {
            for (int y = 0; y < mHeight; ++y)
                for (int x = 0; x < mWidth; ++x)
                {
                    CellCoord coord{ x, y };
                    fn(coord, IsPassable(coord));
                }
        }
#endif

    private:
        bool IsInBounds(CellCoord cell) const;
        int  CellIndex(CellCoord cell) const;

        int mWidth;
        int mHeight;
        SquareConnectivity mConnectivity;

        Dia::Core::Containers::DynamicArray<bool> mPassable;
    };

} // namespace Dia::Pathfinding
