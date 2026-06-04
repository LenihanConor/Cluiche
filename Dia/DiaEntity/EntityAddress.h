#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMailbox/MailboxTypes.h>
#include <DiaEntity/Entity.h>

namespace Dia::Entity {

    // =========================================================================
    // AddressKind — tag byte packed into bits 63-56 of an Address payload
    // =========================================================================
    enum class AddressKind : uint8_t {
        Unknown       = 0,
        Entity        = 1,
        All           = 2,
        ComponentType = 3,
        Self          = 4,
    };

    // =========================================================================
    // Router ID
    // =========================================================================
    extern const Dia::Core::StringCRC kEntityRouterId;

    // =========================================================================
    // Address factory helpers
    // =========================================================================

    // Encode an entity handle into an Address (kind=Entity).
    // bits 63-56 = 1 (Entity), bits 55-24 = entity index, bits 23-0 = generation (24-bit)
    Dia::Mailbox::Address MakeEntityAddress(Entity target);

    // Encode a broadcast address (kind=All).
    Dia::Mailbox::Address MakeAllAddress();

    // Encode a component-type broadcast address (kind=ComponentType).
    // bits 63-56 = 3, bits 31-0 = component type CRC
    Dia::Mailbox::Address MakeComponentTypeAddress(Dia::Core::StringCRC componentTypeId);

    // Encode a self-send address using the sender entity (kind=Self, same layout as Entity).
    Dia::Mailbox::Address MakeSelfAddress(Entity sender);

    // =========================================================================
    // Address decode helpers
    // =========================================================================

    AddressKind          GetAddressKind(const Dia::Mailbox::Address& addr);
    Entity               GetAddressEntity(const Dia::Mailbox::Address& addr);
    Dia::Core::StringCRC GetAddressComponentType(const Dia::Mailbox::Address& addr);

    // =========================================================================
    // SubscriberId helpers — pack/unpack an Entity into a SubscriberId
    // =========================================================================

    // Pack entity index + generation into a SubscriberId value.
    // Layout: bits 55-24 = entity index, bits 23-0 = generation (24-bit).
    Dia::Mailbox::SubscriberId MakeEntitySubscriberId(Entity entity);

    // Unpack entity index + generation from a SubscriberId.
    Entity GetEntityFromSubscriberId(Dia::Mailbox::SubscriberId id);

} // namespace Dia::Entity
