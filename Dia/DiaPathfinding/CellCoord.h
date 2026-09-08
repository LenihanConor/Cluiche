#pragma once
namespace Dia::Pathfinding {
    struct CellCoord {
        int x;  // col (square) or q (hex axial)
        int y;  // row (square) or r (hex axial)
        bool operator==(const CellCoord&) const = default;
    };
}
