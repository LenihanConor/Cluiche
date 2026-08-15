#include <DiaAICallout/CalloutRegistry.h>
#include <DiaCore/Core/Assert.h>

namespace Dia::AICallout {

CalloutRegistry::CalloutRegistry()
{
    for (uint32_t i = 0u; i < kMaxCallouts; ++i)
    {
        mSlots[i].generation      = CalloutHandle::kInvalidGeneration;
        mSlots[i].live            = false;
        mSlots[i].claimed         = false;
    }
}

CalloutHandle CalloutRegistry::Emit(const Callout& callout)
{
    for (uint32_t i = 0u; i < kMaxCallouts; ++i)
    {
        if (!mSlots[i].live)
        {
            // Advance generation, skipping the reserved invalid value (0).
            uint32_t newGen = mSlots[i].generation + 1u;
            if (newGen == CalloutHandle::kInvalidGeneration)
            {
                newGen = 1u;
            }

            mSlots[i].callout         = callout;
            mSlots[i].generation      = newGen;
            mSlots[i].live            = true;
            mSlots[i].claimed         = false;
            mSlots[i].claimerEntityId = Dia::Core::StringCRC();

            return CalloutHandle(i, newGen, this);
        }
    }

    DIA_ASSERT(false, "CalloutRegistry pool is full (capacity=%u)", kMaxCallouts);
    return CalloutHandle();
}

int CalloutRegistry::GetLiveCount() const
{
    int count = 0;
    for (uint32_t i = 0u; i < kMaxCallouts; ++i)
    {
        if (mSlots[i].live)
        {
            ++count;
        }
    }
    return count;
}

bool CalloutRegistry::Claim(const CalloutHandle& handle, Dia::Core::StringCRC claimerEntityId)
{
    if (!handle.IsValid())
    {
        return false;
    }

    CalloutSlot& slot = mSlots[handle.GetIndex()];

    if (slot.claimed)
    {
        return false; // SD-001: already claimed
    }

    slot.claimed         = true;
    slot.claimerEntityId = claimerEntityId;
    return true;
}

void CalloutRegistry::Release(const CalloutHandle& handle, Dia::Core::StringCRC claimerEntityId)
{
    if (!handle.IsValid())
    {
        return; // stale/expired — no-op per spec
    }

    CalloutSlot& slot = mSlots[handle.GetIndex()];

    if (!slot.claimed)
    {
        return; // already unclaimed — no-op
    }

    if (slot.claimerEntityId != claimerEntityId)
    {
        return; // wrong claimer — silent no-op per spec
    }

    slot.claimed         = false;
    slot.claimerEntityId = Dia::Core::StringCRC();
}

void CalloutRegistry::Update(float dt)
{
    for (uint32_t i = 0u; i < kMaxCallouts; ++i)
    {
        CalloutSlot& slot = mSlots[i];

        if (!slot.live)
        {
            continue;
        }

        slot.callout.ttl -= dt;

        if (slot.callout.ttl <= 0.0f)
        {
            slot.live            = false;
            slot.claimed         = false;
            slot.claimerEntityId = Dia::Core::StringCRC();
        }
    }
}

} // namespace Dia::AICallout
