#pragma once

#include <stdint.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace CoW {
namespace WorldStep {

// Fixed-capacity FIFO of mutation entries (ring buffer over a DynamicArrayC).
template <class TEntry, uint32_t N>
class MutationQueue
{
public:
    MutationQueue()
        : mHead(0)
        , mCount(0)
    {
        mSlots.FillDefault();
    }

    // Returns false (state unchanged) when the queue already holds N entries.
    bool Enqueue(const TEntry& entry)
    {
        if (mCount >= N)
        {
            return false;
        }
        mSlots[(mHead + mCount) % N] = entry;
        ++mCount;
        return true;
    }

    uint32_t Count() const { return mCount; }

    // Pops the oldest entry; returns false when empty.
    bool Dequeue(TEntry& out)
    {
        if (mCount == 0)
        {
            return false;
        }
        out = mSlots[mHead];
        mHead = (mHead + 1) % N;
        --mCount;
        return true;
    }

    void Clear()
    {
        mHead = 0;
        mCount = 0;
    }

private:
    Dia::Core::Containers::DynamicArrayC<TEntry, N> mSlots;
    uint32_t mHead;
    uint32_t mCount;
};

}  // namespace WorldStep
}  // namespace CoW
