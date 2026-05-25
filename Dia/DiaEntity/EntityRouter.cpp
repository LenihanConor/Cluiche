#include <DiaEntity/EntityRouter.h>
#include <DiaEntity/Domain.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::Entity {

    EntityRouter::EntityRouter(Domain& domain)
        : mDomain(domain)
    {
    }

    // =========================================================================
    // ResolveEntity — shared logic for Entity and Self kinds.
    // Decodes the entity from the payload, validates generation, and if a matching
    // subscriber exists in liveSubscribers, adds it to outMatched.
    // =========================================================================
    void EntityRouter::ResolveEntity(Entity target,
                                     const Dia::Mailbox::SubscriberSet& liveSubscribers,
                                     Dia::Mailbox::SubscriberSet& outMatched) {
        if (!mDomain.IsAlive(target)) {
#ifdef DEBUG
            DIA_LOG_WARNING("DiaEntity", "EntityRouter::Resolve — entity is not alive (stale address), no-op");
#endif
            return;
        }

        const Dia::Mailbox::SubscriberId targetSub = MakeEntitySubscriberId(target);
        for (uint32_t i = 0; i < liveSubscribers.Size(); ++i) {
            if (liveSubscribers[i] == targetSub) {
                if (!outMatched.IsFull()) {
                    outMatched.Add(liveSubscribers[i]);
                }
                break;
            }
        }
    }

    // =========================================================================
    // Resolve — main dispatch
    // =========================================================================
    void EntityRouter::Resolve(const Dia::Mailbox::Address& addr,
                               const Dia::Mailbox::SubscriberSet& liveSubscribers,
                               Dia::Mailbox::SubscriberSet& outMatched) {
        const AddressKind kind = GetAddressKind(addr);

        switch (kind) {
            case AddressKind::Entity: {
                const Entity target = GetAddressEntity(addr);
                ResolveEntity(target, liveSubscribers, outMatched);
                break;
            }

            case AddressKind::All: {
                for (uint32_t i = 0; i < liveSubscribers.Size(); ++i) {
                    if (!outMatched.IsFull()) {
                        outMatched.Add(liveSubscribers[i]);
                    }
                }
                break;
            }

            case AddressKind::ComponentType: {
                const Dia::Core::StringCRC typeId = GetAddressComponentType(addr);
                for (uint32_t i = 0; i < liveSubscribers.Size(); ++i) {
                    const Entity e = GetEntityFromSubscriberId(liveSubscribers[i]);
                    if (mDomain.HasComponentByTypeId(e, typeId)) {
                        if (!outMatched.IsFull()) {
                            outMatched.Add(liveSubscribers[i]);
                        }
                    }
                }
                break;
            }

            case AddressKind::Self: {
                // Self uses the same layout as Entity — decode sender and resolve as entity.
                const Entity sender = GetAddressEntity(addr);
                ResolveEntity(sender, liveSubscribers, outMatched);
                break;
            }

            default: {
                DIA_LOG_WARNING("DiaEntity", "EntityRouter::Resolve — unknown AddressKind, no-op");
                break;
            }
        }
    }

} // namespace Dia::Entity
