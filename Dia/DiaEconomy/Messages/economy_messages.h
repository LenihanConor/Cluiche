// GENERATED — do not edit. Source: economy_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <functional>

namespace Dia::Economy::Messages {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct PoolChangedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "PoolChangedEvent" };
        Dia::Core::StringCRC instanceName;  // EconomyInstance::GetInstanceName() snapshot
        Dia::Core::StringCRC resourceName;  // resource pool that changed
        float newValue;  // pool value after the change
        float delta;  // signed change amount
    };

    struct TransactionClampedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "TransactionClampedEvent" };
        Dia::Core::StringCRC instanceName;  // EconomyInstance::GetInstanceName() snapshot
        Dia::Core::StringCRC resourceName;  // resource pool that was clamped
        float requestedAmount;  // amount originally requested
        float actualAmount;  // amount actually applied after clamping
    };

    struct TransferCompletedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "TransferCompletedEvent" };
        Dia::Core::StringCRC fromInstanceName;  // source instance identity snapshot
        Dia::Core::StringCRC toInstanceName;  // destination instance identity snapshot
        Dia::Core::StringCRC resourceName;  // resource transferred
        float amount;  // amount transferred
    };

    struct PoolReachedMaximumEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "PoolReachedMaximumEvent" };
        Dia::Core::StringCRC instanceName;  // EconomyInstance::GetInstanceName() snapshot
        Dia::Core::StringCRC resourceName;  // resource pool that hit its maximum
    };

    struct PoolReachedMinimumEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "PoolReachedMinimumEvent" };
        Dia::Core::StringCRC instanceName;  // EconomyInstance::GetInstanceName() snapshot
        Dia::Core::StringCRC resourceName;  // resource pool that hit its minimum
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // PoolChangedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<PoolChangedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<PoolChangedEvent>(Dia::Core::StringCRC{ "EconomyBusAdapter" });

        // TransactionClampedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<TransactionClampedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<TransactionClampedEvent>(Dia::Core::StringCRC{ "EconomyBusAdapter" });

        // TransferCompletedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<TransferCompletedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<TransferCompletedEvent>(Dia::Core::StringCRC{ "EconomyBusAdapter" });

        // PoolReachedMaximumEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<PoolReachedMaximumEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<PoolReachedMaximumEvent>(Dia::Core::StringCRC{ "EconomyBusAdapter" });

        // PoolReachedMinimumEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<PoolReachedMinimumEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<PoolReachedMinimumEvent>(Dia::Core::StringCRC{ "EconomyBusAdapter" });

    }

}
