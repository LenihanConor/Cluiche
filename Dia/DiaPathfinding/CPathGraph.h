#pragma once
#include "DiaPathfinding/CellCoord.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <concepts>

namespace Dia::Pathfinding {
    template<typename T>
    concept CPathGraph = requires(const T& g, CellCoord c,
                                   Dia::Core::Containers::DynamicArrayC<CellCoord, 16>& out) {
        { g.GetNeighbours(c, out) } -> std::same_as<void>;
        { g.IsPassable(c) }         -> std::convertible_to<bool>;
    };
}
