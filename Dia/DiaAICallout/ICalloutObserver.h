#pragma once

#include <DiaAICallout/CalloutHandle.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::AICallout {

    struct Callout;

    // Interface for objects that want to observe CalloutRegistry lifecycle events
    // (emit / claim / release). All methods default to no-op — implement only
    // the notifications you need.
    //
    // NOTE: TTL expiry (CalloutRegistry::Update) never fires any of these
    // notifications — expiry is a passive/no-op-driven state change, not an
    // observable action, per the DiaAICallout observer spec.
    class ICalloutObserver
    {
    public:
        virtual ~ICalloutObserver() = default;

        // Fired every time Emit() successfully posts a new callout.
        virtual void OnCalloutEmitted(const Callout& callout, CalloutHandle handle) {}

        // Fired only when Claim() succeeds (returns true).
        virtual void OnCalloutClaimed(CalloutHandle handle, Dia::Core::StringCRC claimerEntityId) {}

        // Fired only when Release() performs an actual release — never on the
        // documented no-op paths (expired handle, already-unclaimed, wrong claimer).
        virtual void OnCalloutReleased(CalloutHandle handle, Dia::Core::StringCRC claimerEntityId) {}
    };

} // namespace Dia::AICallout
