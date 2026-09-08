#include <DiaMailbox/MailboxTypes.h>

namespace Dia::Mailbox {

    bool Address::operator==(const Address& rhs) const {
        return routerId == rhs.routerId && payload == rhs.payload;
    }

    bool Address::operator!=(const Address& rhs) const {
        return !(*this == rhs);
    }

} // namespace Dia::Mailbox
