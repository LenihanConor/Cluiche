////////////////////////////////////////////////////////////////////////////////
// Filename: PickAddress.h
// Description: Helper to construct DiaMailbox Address values for picking.
//              routerId is always "picking"; payload encodes the PickTrigger.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaPicking/PickTrigger.h>
#include <DiaMailbox/MailboxTypes.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Picking {

struct PickAddress
{
    static const Dia::Core::StringCRC kRouterId;

    static Dia::Mailbox::Address ForTrigger(PickTrigger trigger)
    {
        Dia::Mailbox::Address addr;
        addr.routerId = kRouterId;
        addr.payload  = static_cast<uint64_t>(trigger);
        return addr;
    }

    static PickTrigger TriggerFromAddress(const Dia::Mailbox::Address& addr)
    {
        return static_cast<PickTrigger>(addr.payload);
    }
};

} // namespace Dia::Picking
