#pragma once

#include <DiaScalarField/CellIndex.h>
#include <DiaScalarField/CFieldTopology.h>

namespace Dia
{
    namespace ScalarField
    {
        // Stateless hexagonal grid topology satisfying CFieldTopology.
        // Uses axial coordinates (q, r). The grid is a regular hexagon of radius R:
        //   a cell (q, r) is in bounds iff |q| <= R && |r| <= R && |q+r| <= R.
        // Cell count for radius R: 3*R*R + 3*R + 1.
        // Neighbour iteration is 6-connected; out-of-bounds neighbours are skipped.
        class HexFieldTopology
        {
        public:
            explicit HexFieldTopology(int radius)
                : mRadius(radius)
            {}

            // Total number of cells: 3*R*R + 3*R + 1.
            int GetCellCount() const
            {
                return 3 * mRadius * mRadius + 3 * mRadius + 1;
            }

            // Iterates over all in-bounds axial neighbours of 'cell',
            // invoking callback(CellIndex) for each of the 6 directions.
            void ForEachNeighbour(CellIndex cell, auto&& callback) const
            {
                // Standard axial direction vectors for a hex grid (6-connected).
                static constexpr int kDq[6] = {  1, -1,  0,  0,  1, -1 };
                static constexpr int kDr[6] = {  0,  0,  1, -1, -1,  1 };

                for (int i = 0; i < 6; ++i)
                {
                    const CellIndex neighbour{ cell.x + kDq[i], cell.y + kDr[i] };
                    if (IsInBounds(neighbour))
                        callback(neighbour);
                }
            }

            int GetRadius() const { return mRadius; }

        private:
            // Returns true if the axial cell lies within the hexagonal grid bounds.
            bool IsInBounds(CellIndex cell) const
            {
                const int q = cell.x;
                const int r = cell.y;
                const int absQ   = q   < 0 ? -q   : q;
                const int absR   = r   < 0 ? -r   : r;
                const int absQR  = (q + r) < 0 ? -(q + r) : (q + r);
                return absQ <= mRadius && absR <= mRadius && absQR <= mRadius;
            }

            int mRadius;
        };

    } // namespace ScalarField
} // namespace Dia

static_assert(Dia::ScalarField::CFieldTopology<Dia::ScalarField::HexFieldTopology>,
              "HexFieldTopology must satisfy CFieldTopology");
