#include <DiaEntity/EntityAddress.h>
#include <DiaCore/CRC/CRC.h>

namespace Dia::Entity {

    const Dia::Core::StringCRC kEntityRouterId("dia.entity.router");

    // =========================================================================
    // Helper: construct a StringCRC from a raw uint32_t CRC value.
    // StringCRC has no uint32_t constructor, so we assign through the CRC base.
    // =========================================================================
    static Dia::Core::StringCRC StringCRCFromValue(uint32_t value) {
        Dia::Core::StringCRC result;
        static_cast<Dia::Core::CRC&>(result) = value;
        return result;
    }

    // =========================================================================
    // Address factory helpers
    // =========================================================================

    Dia::Mailbox::Address MakeEntityAddress(Entity target) {
        Dia::Mailbox::Address addr;
        addr.routerId = kEntityRouterId;
        const uint64_t kind  = static_cast<uint64_t>(AddressKind::Entity) << 56;
        const uint64_t index = static_cast<uint64_t>(target.GetIndex()) << 24;
        const uint64_t gen   = static_cast<uint64_t>(target.GetGeneration()) & 0xFFFFFFu;
        addr.payload = kind | index | gen;
        return addr;
    }

    Dia::Mailbox::Address MakeAllAddress() {
        Dia::Mailbox::Address addr;
        addr.routerId = kEntityRouterId;
        const uint64_t kind = static_cast<uint64_t>(AddressKind::All) << 56;
        addr.payload = kind;
        return addr;
    }

    Dia::Mailbox::Address MakeComponentTypeAddress(Dia::Core::StringCRC componentTypeId) {
        Dia::Mailbox::Address addr;
        addr.routerId = kEntityRouterId;
        const uint64_t kind = static_cast<uint64_t>(AddressKind::ComponentType) << 56;
        const uint64_t crc  = static_cast<uint64_t>(componentTypeId.Value()) & 0xFFFFFFFFu;
        addr.payload = kind | crc;
        return addr;
    }

    Dia::Mailbox::Address MakeSelfAddress(Entity sender) {
        Dia::Mailbox::Address addr;
        addr.routerId = kEntityRouterId;
        const uint64_t kind  = static_cast<uint64_t>(AddressKind::Self) << 56;
        const uint64_t index = static_cast<uint64_t>(sender.GetIndex()) << 24;
        const uint64_t gen   = static_cast<uint64_t>(sender.GetGeneration()) & 0xFFFFFFu;
        addr.payload = kind | index | gen;
        return addr;
    }

    // =========================================================================
    // Address decode helpers
    // =========================================================================

    AddressKind GetAddressKind(const Dia::Mailbox::Address& addr) {
        return static_cast<AddressKind>(addr.payload >> 56);
    }

    Entity GetAddressEntity(const Dia::Mailbox::Address& addr) {
        const uint32_t index = static_cast<uint32_t>((addr.payload >> 24) & 0xFFFFFFFFu);
        const uint32_t gen   = static_cast<uint32_t>(addr.payload & 0xFFFFFFu);
        return Entity(index, gen);
    }

    Dia::Core::StringCRC GetAddressComponentType(const Dia::Mailbox::Address& addr) {
        const uint32_t crc = static_cast<uint32_t>(addr.payload & 0xFFFFFFFFu);
        return StringCRCFromValue(crc);
    }

    // =========================================================================
    // SubscriberId helpers
    // =========================================================================

    Dia::Mailbox::SubscriberId MakeEntitySubscriberId(Entity entity) {
        Dia::Mailbox::SubscriberId id;
        const uint64_t index = static_cast<uint64_t>(entity.GetIndex()) << 24;
        const uint64_t gen   = static_cast<uint64_t>(entity.GetGeneration()) & 0xFFFFFFu;
        id.value = index | gen;
        return id;
    }

    Entity GetEntityFromSubscriberId(Dia::Mailbox::SubscriberId id) {
        const uint32_t index = static_cast<uint32_t>((id.value >> 24) & 0xFFFFFFFFu);
        const uint32_t gen   = static_cast<uint32_t>(id.value & 0xFFFFFFu);
        return Entity(index, gen);
    }

} // namespace Dia::Entity
