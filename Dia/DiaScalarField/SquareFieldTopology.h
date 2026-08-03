#pragma once

#include <DiaScalarField/CellIndex.h>
#include <DiaScalarField/CFieldTopology.h>

namespace Dia
{
    namespace ScalarField
    {
        enum class SquareConnectivity
        {
            k4Connected,
            k8Connected
        };

        // Stateless rectangular grid topology satisfying CFieldTopology.
        // Supports 4-connected (cardinal only) and 8-connected (cardinal + diagonal) modes.
        // Neighbour iteration skips out-of-bounds cells automatically.
        class SquareFieldTopology
        {
        public:
            SquareFieldTopology(int width, int height,
                                SquareConnectivity connectivity = SquareConnectivity::k8Connected)
                : mWidth(width)
                , mHeight(height)
                , mConnectivity(connectivity)
            {}

            // Total number of cells in the grid.
            int GetCellCount() const { return mWidth * mHeight; }

            // Iterates over all in-bounds neighbours of 'cell', invoking callback(CellIndex) for each.
            void ForEachNeighbour(CellIndex cell, auto&& callback) const
            {
                // Cardinal offsets (4-connected)
                static constexpr int kCardinalDx[4] = {  0,  0, -1,  1 };
                static constexpr int kCardinalDy[4] = { -1,  1,  0,  0 };

                // Diagonal offsets (8-connected extension)
                static constexpr int kDiagDx[4] = { -1, -1,  1,  1 };
                static constexpr int kDiagDy[4] = { -1,  1, -1,  1 };

                for (int i = 0; i < 4; ++i)
                {
                    const int nx = cell.x + kCardinalDx[i];
                    const int ny = cell.y + kCardinalDy[i];
                    if (IsInBounds(nx, ny))
                        callback(CellIndex{ nx, ny });
                }

                if (mConnectivity == SquareConnectivity::k8Connected)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        const int nx = cell.x + kDiagDx[i];
                        const int ny = cell.y + kDiagDy[i];
                        if (IsInBounds(nx, ny))
                            callback(CellIndex{ nx, ny });
                    }
                }
            }

            int GetWidth()  const { return mWidth;  }
            int GetHeight() const { return mHeight; }

        private:
            bool IsInBounds(int x, int y) const
            {
                return x >= 0 && x < mWidth && y >= 0 && y < mHeight;
            }

            int                mWidth;
            int                mHeight;
            SquareConnectivity mConnectivity;
        };

    } // namespace ScalarField
} // namespace Dia

static_assert(Dia::ScalarField::CFieldTopology<Dia::ScalarField::SquareFieldTopology>,
              "SquareFieldTopology must satisfy CFieldTopology");
