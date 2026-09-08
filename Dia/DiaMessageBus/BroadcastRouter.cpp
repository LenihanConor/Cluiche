#include <DiaMessageBus/BroadcastRouter.h>

namespace Dia::MessageBus {

    Dia::Core::StringCRC BroadcastRouter::GetRouterId() const {
        return Dia::Core::StringCRC("broadcast");
    }

    void BroadcastRouter::Resolve(const Dia::Mailbox::Address& /*addr*/,
                                  const Dia::Mailbox::SubscriberSet& liveSubscribers,
                                  Dia::Mailbox::SubscriberSet& outMatched) {
        // Full fan-out: every live subscriber of the type receives the message.
        for (uint32_t i = 0; i < liveSubscribers.Size(); ++i) {
            if (outMatched.IsFull()) {
                break;
            }
            outMatched.Add(liveSubscribers[i]);
        }
    }

} // namespace Dia::MessageBus
