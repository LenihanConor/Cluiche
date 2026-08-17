#pragma once

#include <gtest/gtest.h>
#include <DiaGridVisibility/GridVisibilitySystem.h>
#include <DiaGridVisibility/VisibilityGroupId.h>
#include <DiaGridVisibility/VisibilityState.h>
#include <DiaPathfinding/CPathGraph.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaEntity/Entity.h>

namespace Dia::GridVisibility::Testing {

    // ---------------------------------------------------------------------------------
    // MockVisibilityGraph
    //
    // Satisfies CVisibilityGraph (which extends CPathGraph) for use in unit tests.
    // Defaults to an 8x8 grid with all cells passable.
    // Call SetPassable(cell, false) to introduce obstacles.
    // Uses 4-connected neighbours (N/E/S/W), matching SquarePathGrid k4Connected.
    // ---------------------------------------------------------------------------------
    class MockVisibilityGraph {
    public:
        static const int kDefaultWidth  = 8;
        static const int kDefaultHeight = 8;

        // All cells passable.
        MockVisibilityGraph()
            : mWidth(kDefaultWidth)
            , mHeight(kDefaultHeight)
            , mPassable()
        {
            for (int i = 0; i < kDefaultWidth * kDefaultHeight; ++i)
                mPassable.Add(true);
        }

        // Custom size, all cells passable.
        MockVisibilityGraph(int width, int height)
            : mWidth(width > 0 ? width : 1)
            , mHeight(height > 0 ? height : 1)
            , mPassable()
        {
            const int count = mWidth * mHeight;
            for (int i = 0; i < count; ++i)
                mPassable.Add(true);
        }

        void SetPassable(Dia::Pathfinding::CellCoord cell, bool passable) {
            if (IsInBounds(cell))
                mPassable[static_cast<unsigned int>(CellIndex(cell))] = passable;
        }

        // CPathGraph requirements:
        void GetNeighbours(Dia::Pathfinding::CellCoord cell,
                           Dia::Core::Containers::DynamicArrayC<Dia::Pathfinding::CellCoord, 16>& out) const {
            out.RemoveAll();
            const int dx[] = { 0, 1, 0, -1 };
            const int dy[] = { -1, 0, 1, 0 };
            for (int i = 0; i < 4; ++i) {
                Dia::Pathfinding::CellCoord nb{ cell.x + dx[i], cell.y + dy[i] };
                if (IsInBounds(nb))
                    out.Add(nb);
            }
        }

        bool IsPassable(Dia::Pathfinding::CellCoord cell) const {
            if (!IsInBounds(cell)) return false;
            return mPassable[static_cast<unsigned int>(CellIndex(cell))];
        }

        // CVisibilityGraph requirements (in addition to CPathGraph):
        int GetWidth()  const { return mWidth; }
        int GetHeight() const { return mHeight; }

    private:
        bool IsInBounds(Dia::Pathfinding::CellCoord cell) const {
            return cell.x >= 0 && cell.y >= 0 && cell.x < mWidth && cell.y < mHeight;
        }

        int CellIndex(Dia::Pathfinding::CellCoord cell) const {
            return cell.y * mWidth + cell.x;
        }

        int mWidth;
        int mHeight;

        // Max capacity: kDefaultWidth x kDefaultHeight = 64 for the default case.
        // For custom sizes keep kMaxCells generous (64x64 vis grid max).
        static constexpr int kMaxCells = 4096;
        Dia::Core::Containers::DynamicArrayC<bool, kMaxCells> mPassable;
    };

    static_assert(CVisibilityGraph<MockVisibilityGraph>,
                  "MockVisibilityGraph must satisfy CVisibilityGraph");

    // ---------------------------------------------------------------------------------
    // Assert helpers
    //
    // All helpers use EXPECT_* (not ASSERT_*) so a test keeps running after a failure.
    // Call from inside a TEST() body.
    // ---------------------------------------------------------------------------------

    template<CVisibilityGraph TGraph>
    inline void AssertCellVisible(const GridVisibilitySystem<TGraph>& sys,
                                  Dia::Pathfinding::CellCoord cell,
                                  VisibilityGroupId groupId) {
        EXPECT_EQ(sys.GetCellState(cell, groupId), VisibilityState::Visible)
            << "Expected cell (" << cell.x << "," << cell.y << ") to be Visible";
    }

    template<CVisibilityGraph TGraph>
    inline void AssertCellRevealed(const GridVisibilitySystem<TGraph>& sys,
                                   Dia::Pathfinding::CellCoord cell,
                                   VisibilityGroupId groupId) {
        EXPECT_EQ(sys.GetCellState(cell, groupId), VisibilityState::Revealed)
            << "Expected cell (" << cell.x << "," << cell.y << ") to be Revealed";
    }

    template<CVisibilityGraph TGraph>
    inline void AssertCellUnexplored(const GridVisibilitySystem<TGraph>& sys,
                                     Dia::Pathfinding::CellCoord cell,
                                     VisibilityGroupId groupId) {
        EXPECT_EQ(sys.GetCellState(cell, groupId), VisibilityState::Unexplored)
            << "Expected cell (" << cell.x << "," << cell.y << ") to be Unexplored";
    }

    template<CVisibilityGraph TGraph>
    inline void AssertCanSee(const GridVisibilitySystem<TGraph>& sys,
                             Dia::Entity::Entity entity,
                             VisibilityGroupId groupId) {
        EXPECT_TRUE(sys.CanSee(entity, groupId))
            << "Expected group to see entity";
    }

    template<CVisibilityGraph TGraph>
    inline void AssertCannotSee(const GridVisibilitySystem<TGraph>& sys,
                                Dia::Entity::Entity entity,
                                VisibilityGroupId groupId) {
        EXPECT_FALSE(sys.CanSee(entity, groupId))
            << "Expected group NOT to see entity";
    }

} // namespace Dia::GridVisibility::Testing
