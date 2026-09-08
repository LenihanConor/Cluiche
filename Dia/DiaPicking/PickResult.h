////////////////////////////////////////////////////////////////////////////////
// Filename: PickResult.h
// Description: Sorted collection of pick hits returned by PickingService.
//              Sorted by priority descending — Best() is highest priority.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>

namespace Dia::Picking {

template<typename THit, unsigned int MaxHits = 16>
class PickResult
{
public:
    void Add(const THit& hit)
    {
        if (mHits.IsFull()) return;

        // Find insertion point to maintain priority descending order
        for (unsigned int i = 0; i < mHits.Size(); ++i)
        {
            if (hit.priority > mHits[i].priority)
            {
                mHits.AddAt(hit, i);
                return;
            }
        }
        mHits.Add(hit);
    }

    bool         HasHit() const { return mHits.Size() > 0; }
    unsigned int Count()  const { return mHits.Size(); }

    const THit& Best() const
    {
        DIA_ASSERT(HasHit(), "PickResult::Best() called on empty result");
        return mHits[0];
    }

    const THit& operator[](unsigned int index) const
    {
        return mHits[index];
    }

    void Clear() { mHits.RemoveAll(); }

private:
    Dia::Core::Containers::DynamicArrayC<THit, MaxHits> mHits;
};

} // namespace Dia::Picking
