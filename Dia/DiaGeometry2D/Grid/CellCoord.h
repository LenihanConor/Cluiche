#pragma once
namespace Dia::Geometry2D {
    struct CellCoord {
        int x;  // col (square) or q (hex axial)
        int y;  // row (square) or r (hex axial)
        bool operator==(const CellCoord&) const = default;
    };
}
