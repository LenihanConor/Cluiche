#pragma once

#include <DiaFlowField/CFlowFieldGraph.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <cmath>

namespace Dia::FlowField {

    // Thin adapter that wraps HexPathGrid and satisfies CFlowFieldGraph.
    // Uses flat-top hex axial coordinates (q, r).
    // The caller owns the grid's lifetime; this adapter holds a const reference.
    class HexFlowAdapter {
    public:
        explicit HexFlowAdapter(const Dia::Pathfinding::HexPathGrid& grid)
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

        // Flat-top hex axial to world:
        //   x = cellSize * (q + r / 2)
        //   y = cellSize * (r * sqrt(3) / 2)   [sqrt(3)/2 ≈ 0.866025]
        Dia::Maths::Vector2D CellToWorldPosition(Dia::Pathfinding::CellCoord cell, float cellSize) const
        {
            float x = cellSize * (cell.x + cell.y * 0.5f);
            float y = cellSize * (cell.y * 0.866025f);  // sqrt(3)/2
            return Dia::Maths::Vector2D(x, y);
        }

        // Reverse flat-top formula with cube-rounding to nearest hex.
        // q_frac = (2/3 * wx) / cellSize
        // r_frac = (-1/3 * wx + (1/sqrt(3)) * wy) / cellSize   [1/sqrt(3) ≈ 0.577350]
        Dia::Pathfinding::CellCoord WorldToCell(Dia::Maths::Vector2D worldPos, float cellSize) const
        {
            float wx = worldPos.X();
            float wy = worldPos.Y();

            float q_frac = (2.0f / 3.0f * wx) / cellSize;
            float r_frac = (-1.0f / 3.0f * wx + 0.577350f * wy) / cellSize;  // 1/sqrt(3)
            float s_frac = -q_frac - r_frac;

            int q = (int)std::roundf(q_frac);
            int r = (int)std::roundf(r_frac);
            int s = (int)std::roundf(s_frac);

            float dq = std::fabsf((float)q - q_frac);
            float dr = std::fabsf((float)r - r_frac);
            float ds = std::fabsf((float)s - s_frac);

            if (dq > dr && dq > ds)
                q = -r - s;
            else if (dr > ds)
                r = -q - s;
            // s is derived; no need to recompute it

            return Dia::Pathfinding::CellCoord{ q, r };
        }

        // --- Convenience accessor ---

        int GetRadius() const { return mGrid.GetRadius(); }

    private:
        const Dia::Pathfinding::HexPathGrid& mGrid;
    };

} // namespace Dia::FlowField

static_assert(Dia::FlowField::CFlowFieldGraph<Dia::FlowField::HexFlowAdapter>,
              "HexFlowAdapter must satisfy CFlowFieldGraph");
