#pragma once

namespace Dia
{
    namespace ScalarField
    {
        // Grid cell coordinate.
        // For square grids: x = column, y = row.
        // For hex grids: x = q (axial column), y = r (axial row).
        struct CellIndex
        {
            int x;  // col (square) or q (hex axial)
            int y;  // row (square) or r (hex axial)

            bool operator==(const CellIndex&) const = default;
        };
    }
}
