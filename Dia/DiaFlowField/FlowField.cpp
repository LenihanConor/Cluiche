#include "DiaFlowField/FlowField.h"
#include <DiaCore/Core/Assert.h>
#include <limits>

namespace Dia::FlowField {

    static const FlowCell kDefaultCell{};

    FlowField::FlowField(int width, int height)
        : mWidth(width)
        , mHeight(height)
    {
        DIA_ASSERT(width >= 1 && height >= 1,
                   "FlowField: invalid dimensions %dx%d (must be >= 1)", width, height);
        // Check that width * height fits int (avoid overflow)
        DIA_ASSERT(width <= std::numeric_limits<int>::max() / height,
                   "FlowField: grid %dx%d overflows int", width, height);

        const int total = width * height;
        mCells.Reserve(total);
        for (int i = 0; i < total; ++i)
            mCells.Add(FlowCell{});
    }

    const FlowCell& FlowField::Sample(Dia::Pathfinding::CellCoord cell) const
    {
        if (cell.x < 0 || cell.x >= mWidth || cell.y < 0 || cell.y >= mHeight)
            return kDefaultCell;
        return mCells[(unsigned int)(cell.y * mWidth + cell.x)];
    }

    FlowCell& FlowField::AccessCell(Dia::Pathfinding::CellCoord cell)
    {
        DIA_ASSERT(cell.x >= 0 && cell.x < mWidth && cell.y >= 0 && cell.y < mHeight,
                   "FlowField::AccessCell out of bounds (%d,%d) for %dx%d grid", cell.x, cell.y, mWidth, mHeight);
        return mCells[(unsigned int)(cell.y * mWidth + cell.x)];
    }

    bool FlowField::IsComplete() const
    {
        const unsigned int total = (unsigned int)(mWidth * mHeight);
        for (unsigned int i = 0; i < total; ++i)
        {
            if (!mCells[i].reachable)
                return false;
        }
        return true;
    }

    int FlowField::GetCellCount() const
    {
        return mWidth * mHeight;
    }

} // namespace Dia::FlowField
