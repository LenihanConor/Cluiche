#include "DiaPathfinding/GridSearchScratch.h"

namespace Dia::Pathfinding {

    void GridSearchScratch::Reserve(uint32_t cellCount)
    {
        mQueue.Reserve(cellCount);
        EnsureCapacity(mFrontier, cellCount);
        EnsureCapacity(mNextFrontier, cellCount);
        EnsureCapacity(mPendingSeeds, cellCount);
        mFloodHeap.Reserve(cellCount);
        EnsureCapacity(mSettled, cellCount);
        mDijkstraFloat.heap.Reserve(cellCount);
        EnsureCapacity(mDijkstraFloat.parked, cellCount);
        mDijkstraDouble.heap.Reserve(cellCount);
        EnsureCapacity(mDijkstraDouble.parked, cellCount);
    }

    void GridSearchScratch::Reset()
    {
        mQueue.Clear();
        mFrontier.RemoveAll();
        mNextFrontier.RemoveAll();
        mPendingSeeds.RemoveAll();
        mFloodHeap.Clear();
        mSettled.RemoveAll();
        mDijkstraFloat.heap.Clear();
        mDijkstraFloat.parked.RemoveAll();
        mDijkstraDouble.heap.Clear();
        mDijkstraDouble.parked.RemoveAll();
    }

    void GridSearchScratch::PrepareSettled(uint32_t cellCount)
    {
        mSettled.RemoveAll();
        EnsureCapacity(mSettled, cellCount);
        for (uint32_t i = 0; i < cellCount; ++i) { mSettled.Add(0); }
    }

    uint64_t GridSearchScratch::TotalCapacity() const
    {
        return static_cast<uint64_t>(mQueue.Capacity()) + mFrontier.Capacity() + mNextFrontier.Capacity()
            + mPendingSeeds.Capacity() + mFloodHeap.Capacity() + mSettled.Capacity()
            + mDijkstraFloat.heap.Capacity() + mDijkstraFloat.parked.Capacity()
            + mDijkstraDouble.heap.Capacity() + mDijkstraDouble.parked.Capacity();
    }

} // namespace Dia::Pathfinding
