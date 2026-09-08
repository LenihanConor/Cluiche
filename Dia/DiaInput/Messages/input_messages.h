// GENERATED — do not edit. Source: input_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <functional>

namespace Dia::Input::Messages {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct KeyDownEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "KeyDownEvent" };
        int code;  // Event::KeyEvent::code snapshot — pass to EKey::CreateFromInt(code) to decode; source EType: kKeyPressed
        bool alt;  // Alt modifier held
        bool control;  // Control modifier held
        bool shift;  // Shift modifier held
        bool system;  // System/OS modifier held
    };

    struct KeyUpEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "KeyUpEvent" };
        int code;  // Event::KeyEvent::code snapshot — pass to EKey::CreateFromInt(code) to decode; source EType: kKeyReleased
        bool alt;  // Alt modifier held
        bool control;  // Control modifier held
        bool shift;  // Shift modifier held
        bool system;  // System/OS modifier held
    };

    struct MouseButtonEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "MouseButtonEvent" };
        int button;  // Event::MouseButtonEvent::button snapshot — pass to EMouseButton::CreateFromInt(button) to decode
        int x;  // cursor x, relative to the owner window
        int y;  // cursor y, relative to the owner window
        bool pressed;  // true for source EType kMouseButtonPressed, false for kMouseButtonReleased
    };

    struct MouseMovedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "MouseMovedEvent" };
        int x;  // Event::MouseMoveEvent::x snapshot
        int y;  // Event::MouseMoveEvent::y snapshot
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // KeyDownEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<KeyDownEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<KeyDownEvent>(Dia::Core::StringCRC{ "InputBusAdapter" });

        // KeyUpEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<KeyUpEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<KeyUpEvent>(Dia::Core::StringCRC{ "InputBusAdapter" });

        // MouseButtonEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<MouseButtonEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<MouseButtonEvent>(Dia::Core::StringCRC{ "InputBusAdapter" });

        // MouseMovedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<MouseMovedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<MouseMovedEvent>(Dia::Core::StringCRC{ "InputBusAdapter" });

    }

}
