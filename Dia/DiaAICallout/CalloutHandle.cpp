#include <DiaAICallout/CalloutHandle.h>
#include <DiaAICallout/Callout.h>
#include <DiaAICallout/CalloutRegistry.h>

namespace Dia::AICallout {

CalloutHandle::CalloutHandle()
    : mIndex(kInvalidIndex)
    , mGeneration(kInvalidGeneration)
    , mRegistry(nullptr)
{
}

CalloutHandle::CalloutHandle(uint32_t index, uint32_t generation, const CalloutRegistryData* registry)
    : mIndex(index)
    , mGeneration(generation)
    , mRegistry(registry)
{
}

bool CalloutHandle::IsValid() const
{
    if (mIndex == kInvalidIndex || mGeneration == kInvalidGeneration || mRegistry == nullptr)
    {
        return false;
    }
    if (mIndex >= CalloutRegistryData::kMaxCallouts)
    {
        return false;
    }
    const CalloutSlot& slot = mRegistry->mSlots[mIndex];
    return slot.live && slot.generation == mGeneration;
}

bool CalloutHandle::IsClaimed() const
{
    if (!IsValid())
    {
        return false;
    }
    return mRegistry->mSlots[mIndex].claimed;
}

const Callout* CalloutHandle::Get() const
{
    if (!IsValid())
    {
        return nullptr;
    }
    return &mRegistry->mSlots[mIndex].callout;
}

uint32_t CalloutHandle::GetIndex() const
{
    return mIndex;
}

uint32_t CalloutHandle::GetGeneration() const
{
    return mGeneration;
}

} // namespace Dia::AICallout
