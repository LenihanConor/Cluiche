#pragma once

#include <DiaAICallout/ICalloutObserver.h>
#include <DiaAICallout/Messages/callout_messages.h> // CalloutEmittedEvent/CalloutClaimedEvent/CalloutReleasedEvent + Bus
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::AICallout {

    // -----------------------------------------------------------------------
    // CalloutBusAdapter
    //
    // ICalloutObserver implementation that forwards every CalloutRegistry
    // notification onto a Dia::MessageBus::Bus via Bus::Broadcast<T>(). Zero
    // logic beyond the forward — CalloutRegistry and ICalloutObserver stay
    // bus-free; only this adapter (and whoever owns it) depends on
    // DiaMessageBus.
    //
    // Ownership: explicitly constructed by whoever owns the CalloutRegistry
    // being observed, then registered via registry.Subscribe(&adapter). No
    // singleton, no central module — same explicit-construction model as
    // CalloutRegistry itself.
    // -----------------------------------------------------------------------
    class CalloutBusAdapter : public ICalloutObserver
    {
    public:
        explicit CalloutBusAdapter(Dia::MessageBus::Bus& bus) : mBus(bus) {}

        void OnCalloutEmitted(const Callout& callout, CalloutHandle handle) override;
        void OnCalloutClaimed(CalloutHandle handle, Dia::Core::StringCRC claimerEntityId) override;
        void OnCalloutReleased(CalloutHandle handle, Dia::Core::StringCRC claimerEntityId) override;

    private:
        Dia::MessageBus::Bus& mBus;
    };

} // namespace Dia::AICallout
