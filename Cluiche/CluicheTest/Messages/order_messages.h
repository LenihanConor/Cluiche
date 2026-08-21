// GENERATED — do not edit. Source: order_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <DiaMaths/Vector/Vector2D.h>
#include <functional>

namespace CluicheTest::Messages {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct OrderStartedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "OrderStartedEvent" };
        Dia::Core::StringCRC orderId;  // IOrder<GuardOrderContext>::GetOrderId() snapshot
        Dia::Maths::Vector2D target;  // GuardMoveOrder::target, if the started order is a GuardMoveOrder; zero-initialized otherwise
        float speed;  // GuardMoveOrder::speed, if the started order is a GuardMoveOrder; zero otherwise
    };

    struct OrderFinishedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "OrderFinishedEvent" };
        Dia::Core::StringCRC orderId;  // IOrder<GuardOrderContext>::GetOrderId() snapshot
    };

    struct OrderCancelledEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "OrderCancelledEvent" };
        Dia::Core::StringCRC orderId;  // IOrder<GuardOrderContext>::GetOrderId() snapshot
    };

    struct OrderQueueEmptyEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "OrderQueueEmptyEvent" };
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // OrderStartedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<OrderStartedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<OrderStartedEvent>(Dia::Core::StringCRC{ "GuardOrderBusAdapter" });

        // OrderFinishedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<OrderFinishedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<OrderFinishedEvent>(Dia::Core::StringCRC{ "GuardOrderBusAdapter" });

        // OrderCancelledEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<OrderCancelledEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<OrderCancelledEvent>(Dia::Core::StringCRC{ "GuardOrderBusAdapter" });

        // OrderQueueEmptyEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<OrderQueueEmptyEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<OrderQueueEmptyEvent>(Dia::Core::StringCRC{ "GuardOrderBusAdapter" });

    }

}
