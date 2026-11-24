#pragma once

#include <DiaPathfinding/CPathGraph.h>
#include <DiaGeometry2D/Grid/CellCoord.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <concepts>

namespace Dia
{
    namespace FlowField
    {
        // CFlowFieldGraph extends CPathGraph with world-space coordinate helpers.
        // Both SquarePathGrid and HexPathGrid satisfy this via thin adapter wrappers.
        template<typename T>
        concept CFlowFieldGraph = Dia::Pathfinding::CPathGraph<T>
            && requires(const T& g,
                        Dia::Geometry2D::CellCoord c,
                        float cellSize,
                        Dia::Maths::Vector2D worldPos)
        {
            { g.CellToWorldPosition(c, cellSize) } -> std::convertible_to<Dia::Maths::Vector2D>;
            { g.WorldToCell(worldPos, cellSize)  } -> std::convertible_to<Dia::Geometry2D::CellCoord>;
        };
    }
}
