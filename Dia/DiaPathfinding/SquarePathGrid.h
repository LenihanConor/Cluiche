#pragma once
#include "DiaPathfinding/CellCoord.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Pathfinding {

    enum class SquareConnectivity { k4Connected, k8Connected };

    class SquarePathGrid {
    public:
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

        static const unsigned int kMaxCells = 4096;
        Dia::Core::Containers::DynamicArrayC<bool, kMaxCells> mPassable;
    };

} // namespace Dia::Pathfinding
