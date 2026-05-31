////////////////////////////////////////////////////////////////////////////////
// Filename: PickRouter.h
// Description: IMailboxRouter that routes PickEvents to subscribers by trigger.
//              Consumers call SubscribeToTrigger to receive only the triggers
//              they care about. PickingModule sends one PickEvent per trigger.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaPicking/PickTrigger.h>
#include <DiaMailbox/IMailboxRouter.h>
#include <DiaMailbox/MailboxTypes.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Picking {

class PickRouter : public Dia::Mailbox::IMailboxRouter
{
public:
    static constexpr unsigned int kMaxSubscribersPerTrigger = 16;
    static constexpr unsigned int kTriggerCount = static_cast<unsigned int>(PickTrigger::kCount);

    Dia::Core::StringCRC GetRouterId() const override;

    // Resolve: fills outMatched with subscribers that registered for the trigger
    // encoded in addr.payload. liveSubscribers is ignored — PickRouter maintains
    // its own per-trigger subscriber lists.
    void Resolve(const Dia::Mailbox::Address& addr,
                 const Dia::Mailbox::SubscriberSet& liveSubscribers,
                 Dia::Mailbox::SubscriberSet& outMatched) override;

    void SubscribeToTrigger(PickTrigger trigger, Dia::Mailbox::SubscriberId subscriber);
    void UnsubscribeFromTrigger(PickTrigger trigger, Dia::Mailbox::SubscriberId subscriber);

private:
    using TriggerSubscriberList = Dia::Core::Containers::DynamicArrayC<
        Dia::Mailbox::SubscriberId, kMaxSubscribersPerTrigger>;

    TriggerSubscriberList mSubscribers[kTriggerCount];
};

} // namespace Dia::Picking
