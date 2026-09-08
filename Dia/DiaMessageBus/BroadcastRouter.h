#pragma once
#include <DiaMailbox/IMailboxRouter.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::MessageBus {

    // Fans a message out to every live subscriber of its type, regardless of
    // addr.payload. Registered by Bus::Initialize() under router id "broadcast".
    class BroadcastRouter : public Dia::Mailbox::IMailboxRouter {
    public:
        Dia::Core::StringCRC GetRouterId() const override;

        void Resolve(const Dia::Mailbox::Address& addr,
                     const Dia::Mailbox::SubscriberSet& liveSubscribers,
                     Dia::Mailbox::SubscriberSet& outMatched) override;
    };

} // namespace Dia::MessageBus
