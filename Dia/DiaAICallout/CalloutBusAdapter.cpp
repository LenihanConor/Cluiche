#include <DiaAICallout/CalloutBusAdapter.h>

namespace Dia::AICallout {

    void CalloutBusAdapter::OnCalloutEmitted(const Callout& callout, CalloutHandle handle)
    {
        mBus.Broadcast(CalloutEmittedEvent{ callout, handle });
    }

    void CalloutBusAdapter::OnCalloutClaimed(CalloutHandle handle, Dia::Core::StringCRC claimerEntityId)
    {
        mBus.Broadcast(CalloutClaimedEvent{ handle, claimerEntityId });
    }

    void CalloutBusAdapter::OnCalloutReleased(CalloutHandle handle, Dia::Core::StringCRC claimerEntityId)
    {
        mBus.Broadcast(CalloutReleasedEvent{ handle, claimerEntityId });
    }

} // namespace Dia::AICallout
