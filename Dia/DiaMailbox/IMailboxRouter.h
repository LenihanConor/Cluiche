#pragma once
#include <DiaMailbox/MailboxTypes.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Mailbox {

    class IMailboxRouter {
    public:
        virtual ~IMailboxRouter() = default;

        virtual Dia::Core::StringCRC GetRouterId() const = 0;

        // Fill outMatched with subscriber IDs from liveSubscribers that should receive
        // the message addressed to addr. outMatched is pre-cleared by the caller.
        // Precondition: addr.routerId == GetRouterId().
        // Lifetime contract: this pointer must remain valid while registered with Mailbox.
        virtual void Resolve(const Address& addr,
                             const SubscriberSet& liveSubscribers,
                             SubscriberSet& outMatched) = 0;
    };

} // namespace Dia::Mailbox
