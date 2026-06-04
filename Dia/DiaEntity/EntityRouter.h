#pragma once
#include <DiaMailbox/IMailboxRouter.h>
#include <diaentitytemplate/EntityAddress.h>

namespace Dia::Entity {

    // Forward declaration to break circular include: EntityRouter.h → Domain.h → EntityRouter.h
    class Domain;

    // =========================================================================
    // EntityRouter — IMailboxRouter that decodes 64-bit entity address payloads
    // and resolves them to the correct subscriber set in a Domain.
    //
    // Auto-registered with the Domain's Mailbox in the Domain constructor.
    // Single-threaded (SD-ENT-018): no locking.
    //
    // AddressKind dispatch:
    //   Entity        → resolve the specific entity's subscriber (if alive + matching gen)
    //   All           → copy all liveSubscribers to outMatched
    //   ComponentType → subscribers whose entity has the named component type
    //   Self          → same as Entity kind, decoding sender from payload
    //   Unknown       → DIA_LOG_WARNING + return
    // =========================================================================
    class EntityRouter final : public Dia::Mailbox::IMailboxRouter {
    public:
        explicit EntityRouter(Domain& domain);

        Dia::Core::StringCRC GetRouterId() const override { return kEntityRouterId; }

        void Resolve(const Dia::Mailbox::Address& addr,
                     const Dia::Mailbox::SubscriberSet& liveSubscribers,
                     Dia::Mailbox::SubscriberSet& outMatched) override;

    private:
        Domain& mDomain;

        void ResolveEntity(Entity target,
                           const Dia::Mailbox::SubscriberSet& liveSubscribers,
                           Dia::Mailbox::SubscriberSet& outMatched);
    };

} // namespace Dia::Entity
