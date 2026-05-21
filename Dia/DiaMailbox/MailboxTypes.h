#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Mailbox {

    struct Address {
        Dia::Core::StringCRC routerId;
        uint64_t             payload = 0;

        bool operator==(const Address& rhs) const;
        bool operator!=(const Address& rhs) const;
    };

    struct SubscriberId {
        uint64_t value = 0;
        bool operator==(const SubscriberId& rhs) const { return value == rhs.value; }
        bool operator!=(const SubscriberId& rhs) const { return value != rhs.value; }
    };

    enum class OverflowPolicy : unsigned char {
        DropOldest,
        Assert
    };

    // Forward declaration only — full definition in subscriptions feature
    class SubscriptionHandle;

    using SubscriberSet = Dia::Core::Containers::DynamicArrayC<SubscriberId, 64>;

} // namespace Dia::Mailbox
