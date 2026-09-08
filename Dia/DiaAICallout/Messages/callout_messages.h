// GENERATED — do not edit. Source: callout_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <DiaAICallout/Callout.h>
#include <DiaAICallout/CalloutHandle.h>
#include <functional>

namespace Dia::AICallout {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct CalloutEmittedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "CalloutEmittedEvent" };
        Callout callout;  // full callout payload — kind/position/radius/faction/ttl/payload, self-describes filtering criteria
        CalloutHandle handle;  // owning handle for the newly-emitted callout
    };

    struct CalloutClaimedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "CalloutClaimedEvent" };
        CalloutHandle handle;  // handle of the claimed callout
        Dia::Core::StringCRC claimerEntityId;  // entity that performed the claim
    };

    struct CalloutReleasedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "CalloutReleasedEvent" };
        CalloutHandle handle;  // handle of the released callout
        Dia::Core::StringCRC claimerEntityId;  // entity that had claimed it
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // CalloutEmittedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<CalloutEmittedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<CalloutEmittedEvent>(Dia::Core::StringCRC{ "CalloutBusAdapter" });

        // CalloutClaimedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<CalloutClaimedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<CalloutClaimedEvent>(Dia::Core::StringCRC{ "CalloutBusAdapter" });

        // CalloutReleasedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<CalloutReleasedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<CalloutReleasedEvent>(Dia::Core::StringCRC{ "CalloutBusAdapter" });

    }

}
