#include "DiaFlowField/FlowField.h"
#include <DiaCore/Core/Assert.h>

namespace Dia::FlowField {

    static const FlowCell kDefaultCell{};

    FlowField::FlowField(int width, int height)
        : mWidth(width)
        , mHeight(height)
    {
        DIA_ASSERT(width * height <= (int)kMaxFlowFieldCells,
                   "FlowField: grid %dx%d exceeds kMaxFlowFieldCells (%u)", width, height, kMaxFlowFieldCells);
        const int total = width * height;
        for (int i = 0; i < total; ++i)
            mCells.Add(FlowCell{});
    }

    const FlowCell& FlowField::Sample(Dia::Pathfinding::CellCoord cell) const
    {
        if (cell.x < 0 || cell.x >= mWidth || cell.y < 0 || cell.y >= mHeight)
            return kDefaultCell;
        return mCells[cell.y * mWidth + cell.x];
    }

    FlowCell& FlowField::AccessCell(Dia::Pathfinding::CellCoord cell)
    {
        DIA_ASSERT(cell.x >= 0 && cell.x < mWidth && cell.y >= 0 && cell.y < mHeight,
                   "FlowField::AccessCell out of bounds (%d,%d) for %dx%d grid", cell.x, cell.y, mWidth, mHeight);
        return mCells[cell.y * mWidth + cell.x];
    }

    bool FlowField::IsComplete() const
    {
        const int total = mWidth * mHeight;
        for (int i = 0; i < total; ++i)
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
