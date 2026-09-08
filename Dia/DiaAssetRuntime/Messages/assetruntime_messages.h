// GENERATED — do not edit. Source: assetruntime_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <DiaCore/Strings/String512.h>
#include <functional>

namespace Dia::AssetRuntime {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct AssetReadyEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "AssetReadyEvent" };
        Dia::Core::StringCRC assetId;  // asset that transitioned to Staged
        Dia::Core::Containers::String512 resolvedPath;  // resolved deploy path for the asset
    };

    struct AssetUnloadingEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "AssetUnloadingEvent" };
        Dia::Core::StringCRC assetId;  // asset that transitioned to Unloaded
    };

    struct AssetLoadFailedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "AssetLoadFailedEvent" };
        Dia::Core::StringCRC assetId;  // asset that transitioned to Failed
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // AssetReadyEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<AssetReadyEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<AssetReadyEvent>(Dia::Core::StringCRC{ "AssetRuntimeBusAdapter" });

        // AssetUnloadingEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<AssetUnloadingEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<AssetUnloadingEvent>(Dia::Core::StringCRC{ "AssetRuntimeBusAdapter" });

        // AssetLoadFailedEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<AssetLoadFailedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<AssetLoadFailedEvent>(Dia::Core::StringCRC{ "AssetRuntimeBusAdapter" });

    }

}
