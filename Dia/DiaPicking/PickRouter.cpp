////////////////////////////////////////////////////////////////////////////////
// Filename: PickRouter.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaPicking/PickRouter.h"
#include "DiaPicking/PickAddress.h"

namespace Dia::Picking {

Dia::Core::StringCRC PickRouter::GetRouterId() const
{
    return PickAddress::kRouterId;
}

void PickRouter::Resolve(const Dia::Mailbox::Address& addr,
                         const Dia::Mailbox::SubscriberSet& /*liveSubscribers*/,
                         Dia::Mailbox::SubscriberSet& outMatched)
{
    const PickTrigger trigger = PickAddress::TriggerFromAddress(addr);
    const unsigned int idx = static_cast<unsigned int>(trigger);
    if (idx >= kTriggerCount) return;

    outMatched.RemoveAll();
    const TriggerSubscriberList& list = mSubscribers[idx];
    for (unsigned int i = 0; i < list.Size(); ++i)
        outMatched.Add(list[i]);
}

void PickRouter::SubscribeToTrigger(PickTrigger trigger, Dia::Mailbox::SubscriberId subscriber)
{
    const unsigned int idx = static_cast<unsigned int>(trigger);
    if (idx >= kTriggerCount) return;
    if (mSubscribers[idx].FindIndex(subscriber) >= 0) return;
    if (!mSubscribers[idx].IsFull())
        mSubscribers[idx].Add(subscriber);
}

void PickRouter::UnsubscribeFromTrigger(PickTrigger trigger, Dia::Mailbox::SubscriberId subscriber)
{
    const unsigned int idx = static_cast<unsigned int>(trigger);
    if (idx >= kTriggerCount) return;
    mSubscribers[idx].RemoveFirst(subscriber);
}

} // namespace Dia::Picking
