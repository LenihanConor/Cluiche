#pragma once
#include <DiaMailbox/MailboxTypes.h>

namespace Dia::Mailbox {

    struct Subscription {
        SubscriberId subscriberId;
        uint32_t     typeKey = 0;
    };

    constexpr uint32_t kMaxSubs = 256;

} // namespace Dia::Mailbox
