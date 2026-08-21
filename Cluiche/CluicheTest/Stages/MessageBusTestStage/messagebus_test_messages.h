// GENERATED — do not edit. Source: messagebus_test_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <DiaEntity/EntityId.h>
#include <functional>

namespace CluicheTest::Messages {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct NetworkPulseEvent {
        static constexpr Dia::Core::StringCRC kTypeId{ "NetworkPulseEvent" };
        uint32_t sequenceId;  // monotonically increasing per emitter
        EntityId emitterId;  // which emitter fired
    };

    struct DirectPingEvent {
        static constexpr Dia::Core::StringCRC kTypeId{ "DirectPingEvent" };
        EntityId senderId;  // entity that sent the ping
        uint32_t pingId;  // per-sender sequence number
    };

    struct PongEvent {
        static constexpr Dia::Core::StringCRC kTypeId{ "PongEvent" };
        EntityId responderId;  // receiver that replied
    };

    struct BurstEvent {
        static constexpr Dia::Core::StringCRC kTypeId{ "BurstEvent" };
        uint32_t count;  // number of pulses in this burst
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
        std::function<void(const NetworkPulseEvent&)> onNetworkPulseEvent;  // consumer: ReceiverSystem
        std::function<void(const DirectPingEvent&)> onDirectPingEvent;  // consumer: ReceiverSystem
        std::function<void(const PongEvent&)> onPongEvent;  // consumer: EmitterSystem
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // NetworkPulseEvent — broadcast, primary, capacity 128, drop_oldest
        bus.RegisterType<NetworkPulseEvent, 128>(Dia::Mailbox::OverflowPolicy::DropOldest);
        bus.RegisterProducer<NetworkPulseEvent>(Dia::Core::StringCRC{ "EmitterSystem" });
        DIA_ASSERT(handlers.onNetworkPulseEvent, "Handlers::onNetworkPulseEvent is unbound");
        bus.Subscribe<NetworkPulseEvent>(Dia::Core::StringCRC{ "ReceiverSystem" }, handlers.onNetworkPulseEvent, Dia::MessageBus::Pass::Primary);

        // DirectPingEvent — entity, primary, capacity 64 (default), assert (default)
        bus.RegisterType<DirectPingEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<DirectPingEvent>(Dia::Core::StringCRC{ "EmitterSystem" });
        DIA_ASSERT(handlers.onDirectPingEvent, "Handlers::onDirectPingEvent is unbound");
        bus.Subscribe<DirectPingEvent>(Dia::Core::StringCRC{ "ReceiverSystem" }, handlers.onDirectPingEvent, Dia::MessageBus::Pass::Primary);

        // PongEvent — broadcast, reaction, capacity 64 (default), assert (default)
        bus.RegisterType<PongEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<PongEvent>(Dia::Core::StringCRC{ "ReceiverSystem" });
        DIA_ASSERT(handlers.onPongEvent, "Handlers::onPongEvent is unbound");
        bus.Subscribe<PongEvent>(Dia::Core::StringCRC{ "EmitterSystem" }, handlers.onPongEvent, Dia::MessageBus::Pass::Reaction);

        // BurstEvent — broadcast, primary, capacity 32, assert (default)
        bus.RegisterType<BurstEvent, 32>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<BurstEvent>(Dia::Core::StringCRC{ "EmitterSystem" });

    }

}
