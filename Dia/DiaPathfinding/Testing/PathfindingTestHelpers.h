#pragma once
#include "DiaPathfinding/PathResult.h"
#include "DiaPathfinding/IPathCostProvider.h"
#include "DiaPathfinding/CellCoord.h"
#include <DiaCore/Core/Assert.h>

namespace Dia { namespace Pathfinding { namespace Testing {

    inline void AssertPathFound(const PathResult& result)
    {
        DIA_ASSERT(result.success, "Expected path to be found but PathResult::success was false");
        DIA_ASSERT(result.cells.Size() > 0, "Expected non-empty cell list in PathResult");
    }

    inline void AssertPathCells(const PathResult& result, const CellCoord* expectedCells, unsigned int count)
    {
        AssertPathFound(result);
        DIA_ASSERT(result.cells.Size() == count, "PathResult cell count mismatch");
        for (unsigned int i = 0; i < count; ++i)
        {
            DIA_ASSERT(result.cells[i] == expectedCells[i], "PathResult cell mismatch at index");
        }
    }

    class MockCostProvider : public IPathCostProvider
    {
    public:
        explicit MockCostProvider(float cost = 1.0f) : mCost(cost) {}

        float GetCost(CellCoord /*from*/, CellCoord /*to*/) const override
        {
            return mCost;
        }

        void SetCost(float c) { mCost = c; }
        void SetImpassable() { mCost = IPathCostProvider::kImpassableCost; }

    private:
        float mCost;
    };

}}} // namespace Dia::Pathfinding::Testing
