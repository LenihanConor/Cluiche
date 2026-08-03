#pragma once

#include <DiaFlowField/CFlowFieldGraph.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::FlowField {

    // Thin adapter that wraps SquarePathGrid and satisfies CFlowFieldGraph.
    // The caller owns the grid's lifetime; this adapter holds a const reference.
    class SquareFlowAdapter {
    public:
        explicit SquareFlowAdapter(const Dia::Pathfinding::SquarePathGrid& grid)
            : mGrid(grid)
        {}

        // --- CPathGraph forwarding ---

        void GetNeighbours(Dia::Pathfinding::CellCoord cell,
                           Dia::Core::Containers::DynamicArrayC<Dia::Pathfinding::CellCoord, 16>& out) const
        {
            mGrid.GetNeighbours(cell, out);
        }

        bool IsPassable(Dia::Pathfinding::CellCoord cell) const
        {
            return mGrid.IsPassable(cell);
        }

        // --- CFlowFieldGraph additions ---

        // World position is cell-centred:
        //   x = col * cellSize + cellSize / 2
        //   y = row * cellSize + cellSize / 2
        Dia::Maths::Vector2D CellToWorldPosition(Dia::Pathfinding::CellCoord cell, float cellSize) const
        {
            return Dia::Maths::Vector2D(
                cell.x * cellSize + cellSize * 0.5f,
                cell.y * cellSize + cellSize * 0.5f
            );
        }

        // Inverse: col = floor(worldPos.X / cellSize), row = floor(worldPos.Y / cellSize)
        Dia::Pathfinding::CellCoord WorldToCell(Dia::Maths::Vector2D worldPos, float cellSize) const
        {
            return Dia::Pathfinding::CellCoord{
                (int)(worldPos.X() / cellSize),
                (int)(worldPos.Y() / cellSize)
            };
        }

        // --- Convenience accessors ---

        int GetWidth()  const { return mGrid.GetWidth();  }
        int GetHeight() const { return mGrid.GetHeight(); }

    private:
        const Dia::Pathfinding::SquarePathGrid& mGrid;
    };

} // namespace Dia::FlowField

static_assert(Dia::FlowField::CFlowFieldGraph<Dia::FlowField::SquareFlowAdapter>,
              "SquareFlowAdapter must satisfy CFlowFieldGraph");
