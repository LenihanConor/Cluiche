// GENERATED — do not edit. Source: physics_messages.diagamemessages
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <DiaRigidBody2D/Events/CollisionEvent.h>
#include <cstdint>
#include <functional>

namespace Dia::RigidBody2D::Messages {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct PhysicsCollisionEvent {
        static inline const Dia::Core::StringCRC kTypeId{ "PhysicsCollisionEvent" };
        Dia::RigidBody2D::CollisionEventType type;  // Enter/Stay/Exit — mirrors CollisionEvent::type
        uint32_t bodyAUniqueId;  // Body2DBase::GetUniqueId() snapshot for bodyA — stable identity assigned by PhysicsWorld, not a pointer
        uint32_t bodyBUniqueId;  // Body2DBase::GetUniqueId() snapshot for bodyB — stable identity assigned by PhysicsWorld, not a pointer
        Dia::Core::StringCRC bodyAId;  // Body2DBase::GetId() snapshot for bodyA
        Dia::Core::StringCRC bodyBId;  // Body2DBase::GetId() snapshot for bodyB
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // PhysicsCollisionEvent — broadcast, primary, capacity 64 (default), assert (default)
        bus.RegisterType<PhysicsCollisionEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<PhysicsCollisionEvent>(Dia::Core::StringCRC{ "PhysicsBusAdapter" });

    }

}
