#include <DiaAICallout/CalloutHandle.h>
#include <DiaAICallout/Callout.h>

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
    // TODO: verify generation matches slot generation in mRegistry once CalloutRegistryData is defined
    return true;
}

bool CalloutHandle::IsClaimed() const
{
    if (!IsValid())
    {
        return false;
    }
    // TODO: check claimed flag via mRegistry once CalloutRegistryData is defined
    return false;
}

const Callout* CalloutHandle::Get() const
{
    if (!IsValid())
    {
        return nullptr;
    }
    // TODO: return pointer to Callout slot in mRegistry once CalloutRegistryData is defined
    return nullptr;
}

} // namespace Dia::AICallout
