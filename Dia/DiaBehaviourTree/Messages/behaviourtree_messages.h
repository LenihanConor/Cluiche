// GENERATED — do not edit. Source: behaviourtree_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <functional>

namespace Dia::BehaviourTree::Messages {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct NodeEnteredEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "NodeEnteredEvent" };
        Dia::Core::StringCRC nodeId;  // node entered on the owning entity's tree
    };

    struct NodeCompletedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "NodeCompletedEvent" };
        Dia::Core::StringCRC nodeId;  // node that finished evaluating
        NodeResult result;  // kSuccess or kFailure — never posted for kRunning
    };

    struct TreeCompletedEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "TreeCompletedEvent" };
        NodeResult result;  // root node's final result for this tick
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // NodeEnteredEvent — entity, primary, capacity 64 (default), assert (default)
        bus.RegisterType<NodeEnteredEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<NodeEnteredEvent>(Dia::Core::StringCRC{ "BehaviourTreeBusAdapter" });

        // NodeCompletedEvent — entity, primary, capacity 64 (default), assert (default)
        bus.RegisterType<NodeCompletedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<NodeCompletedEvent>(Dia::Core::StringCRC{ "BehaviourTreeBusAdapter" });

        // TreeCompletedEvent — entity, primary, capacity 64 (default), assert (default)
        bus.RegisterType<TreeCompletedEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<TreeCompletedEvent>(Dia::Core::StringCRC{ "BehaviourTreeBusAdapter" });

    }

}
