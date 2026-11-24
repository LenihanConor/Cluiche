#pragma once

#include <gtest/gtest.h>
#include <DiaFlowField/CFlowFieldGraph.h>
#include <DiaFlowField/FlowCell.h>
#include <DiaFlowField/FlowField.h>
#include <DiaGeometry2D/Grid/CellCoord.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <cmath>

namespace Dia::FlowField::Testing {

    // A configurable mock graph satisfying CFlowFieldGraph.
    // Wraps a fixed 8x8 grid with all cells passable by default.
    // Test code can call SetPassable(cell, false) to block cells.
    class MockFlowFieldGraph {
    public:
        static const int kWidth  = 8;
        static const int kHeight = 8;

        MockFlowFieldGraph() {
            for (int i = 0; i < kWidth * kHeight; ++i)
                mPassable.Add(true);
        }

        void SetPassable(Dia::Geometry2D::CellCoord cell, bool passable) {
            if (IsInBounds(cell))
                mPassable[CellIndex(cell)] = passable;
        }

        // CPathGraph requirements:
        void GetNeighbours(Dia::Geometry2D::CellCoord cell,
                           Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::CellCoord, 16>& out) const {
            out.RemoveAll();
            const int dx[] = {0, 1, 0, -1};
            const int dy[] = {-1, 0, 1, 0};
            for (int i = 0; i < 4; ++i) {
                Dia::Geometry2D::CellCoord nb{cell.x + dx[i], cell.y + dy[i]};
                if (IsInBounds(nb))
                    out.Add(nb);
            }
        }

        bool IsPassable(Dia::Geometry2D::CellCoord cell) const {
            if (!IsInBounds(cell)) return false;
            return mPassable[CellIndex(cell)];
        }

        // CFlowFieldGraph additions (cell-centred, 1.0f cell size by default):
        Dia::Maths::Vector2D CellToWorldPosition(Dia::Geometry2D::CellCoord cell, float cellSize) const {
            return Dia::Maths::Vector2D(
                cell.x * cellSize + cellSize * 0.5f,
                cell.y * cellSize + cellSize * 0.5f
            );
        }

        Dia::Geometry2D::CellCoord WorldToCell(Dia::Maths::Vector2D worldPos, float cellSize) const {
            return Dia::Geometry2D::CellCoord{
                (int)(worldPos.X() / cellSize),
                (int)(worldPos.Y() / cellSize)
            };
        }

    private:
        bool IsInBounds(Dia::Geometry2D::CellCoord cell) const {
            return cell.x >= 0 && cell.x < kWidth && cell.y >= 0 && cell.y < kHeight;
        }

        int CellIndex(Dia::Geometry2D::CellCoord cell) const {
            return cell.y * kWidth + cell.x;
        }

        // 8x8 = 64 cells
        Dia::Core::Containers::DynamicArrayC<bool, 64> mPassable;
    };

    static_assert(CFlowFieldGraph<MockFlowFieldGraph>, "MockFlowFieldGraph must satisfy CFlowFieldGraph");

    // Assert that the field cell at `cell` has a direction within `toleranceDeg` degrees of `expectedDir`.
    // Call from within a TEST() body — uses EXPECT_NEAR.
    inline void AssertCellDirection(const FlowField& field,
                                    Dia::Geometry2D::CellCoord cell,
                                    Dia::Maths::Vector2D expectedDir,
                                    float toleranceDeg = 10.0f) {
        const FlowCell& fc = field.Sample(cell);
        EXPECT_TRUE(fc.reachable) << "Cell (" << cell.x << "," << cell.y << ") is not reachable";
        // Compare angle: dot product > cos(toleranceDeg)
        float cosThresh = std::cosf(toleranceDeg * 3.14159265f / 180.0f);
        float dot = fc.direction.Dot(expectedDir);
        EXPECT_GE(dot, cosThresh) << "Cell (" << cell.x << "," << cell.y << ") direction mismatch";
    }

    // Assert that the field cell at `cell` is reachable.
    inline void AssertReachable(const FlowField& field, Dia::Geometry2D::CellCoord cell) {
        EXPECT_TRUE(field.Sample(cell).reachable)
            << "Expected cell (" << cell.x << "," << cell.y << ") to be reachable";
    }

    // Assert that the field cell at `cell` is NOT reachable.
    inline void AssertUnreachable(const FlowField& field, Dia::Geometry2D::CellCoord cell) {
        EXPECT_FALSE(field.Sample(cell).reachable)
            << "Expected cell (" << cell.x << "," << cell.y << ") to be unreachable";
    }

} // namespace Dia::FlowField::Testing
