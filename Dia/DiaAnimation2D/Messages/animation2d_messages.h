// GENERATED — do not edit. Source: animation2d_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <functional>

namespace Dia::Animation2D::Messages {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct ClipFinishedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "ClipFinishedEvent" };
        Dia::Core::StringCRC clipId;  // AnimClip::GetId() snapshot — stable clip identity, not a pointer
    };

    struct ClipLoopedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "ClipLoopedEvent" };
        Dia::Core::StringCRC clipId;  // AnimClip::GetId() snapshot — stable clip identity, not a pointer
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // ClipFinishedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<ClipFinishedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<ClipFinishedEvent>(Dia::Core::StringCRC{ "AnimClipBusAdapter" });

        // ClipLoopedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<ClipLoopedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<ClipLoopedEvent>(Dia::Core::StringCRC{ "AnimClipBusAdapter" });

    }

}
