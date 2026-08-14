#pragma once

#include <DiaFlowField/FlowCell.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace FlowField
    {
        // Maximum number of cells a FlowField can hold.
        // Matches DiaPathfinding's SquarePathGrid upper limit (64x64).
        static const unsigned int kMaxFlowFieldCells = 4096;

        // A baked flow field over a grid.
        // Produced by ComputeFlowField; consumed by steering/agent code.
        class FlowField
        {
        public:
            // --- Public query API ---

            // Sample the flow direction at a cell.
            // Returns a reference to a static default unreachable FlowCell when
            // the coord is out of bounds.
            const FlowCell& Sample(Dia::Pathfinding::CellCoord cell) const;

            // Convenience: sample direction from a world position using a uniform
            // square-grid cell size.  Does not need graph access — uses integer
            // division directly (col = worldPos.x / cellSize, row = worldPos.y / cellSize).
            Dia::Maths::Vector2D SampleWorld(Dia::Maths::Vector2D worldPos, float cellSize) const
            {
                int col = static_cast<int>(worldPos.X() / cellSize);
                int row = static_cast<int>(worldPos.Y() / cellSize);
                return Sample(Dia::Pathfinding::CellCoord{col, row}).direction;
            }

            // Conservative completeness check: returns true if every cell in
            // [0, width*height) is reachable.  (Passability is tracked by the
            // graph, not the field, so this is a lower-bound check.)
            bool IsComplete() const;

            // Total number of cells (width * height).
            int GetCellCount() const;

            // Grid dimensions — needed to convert flat index to (col, row) for world-position.
            int GetWidth()  const { return mWidth; }
            int GetHeight() const { return mHeight; }

            // --- Internal API (used by ComputeFlowField only) ---

            // Construct an empty field of the given dimensions.
            // All cells are default-initialised (direction={0,0}, reachable=false).
            explicit FlowField(int width, int height);

            // Return a mutable reference to the cell at (cell.x, cell.y).
            // Called by ComputeFlowField while building the field.
            FlowCell& AccessCell(Dia::Pathfinding::CellCoord cell);

        private:
            int mWidth;
            int mHeight;
            Dia::Core::Containers::DynamicArrayC<FlowCell, kMaxFlowFieldCells> mCells;
        };
    }
}
