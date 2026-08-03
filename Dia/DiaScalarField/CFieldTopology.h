#pragma once

#include <DiaScalarField/CellIndex.h>
#include <concepts>

namespace Dia
{
    namespace ScalarField
    {
        // Concept for scalar field topology types.
        // A conforming type must provide:
        //   GetCellCount() -> convertible to int  (total number of cells in the field)
        //   ForEachNeighbour(CellIndex, callback) -> void  (visits each in-bounds neighbour)
        template<typename T>
        concept CFieldTopology = requires(const T& t, CellIndex c)
        {
            { t.GetCellCount() } -> std::convertible_to<int>;
            { t.ForEachNeighbour(c, [](CellIndex){}) } -> std::same_as<void>;
        };
    }
}
