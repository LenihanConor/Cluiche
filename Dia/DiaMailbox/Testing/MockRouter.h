#pragma once
#include <DiaMailbox/IMailboxRouter.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Mailbox::Testing {

    class MockRouter : public Dia::Mailbox::IMailboxRouter {
    public:
        explicit MockRouter(Dia::Core::StringCRC routerId);

        // Add or replace rule: when addr.payload == payload, outMatched = matchedSubscribers.
        void AddRule(uint64_t payload, const Dia::Mailbox::SubscriberSet& matchedSubscribers);

        // Remove all rules and reset call count.
        void ClearRules();

        int ResolveCallCount() const;

        // IMailboxRouter
        Dia::Core::StringCRC GetRouterId() const override;
        void Resolve(const Dia::Mailbox::Address& addr,
                     const Dia::Mailbox::SubscriberSet& liveSubscribers,
                     Dia::Mailbox::SubscriberSet& outMatched) override;

    private:
        Dia::Core::StringCRC mRouterId;
        int mResolveCallCount = 0;

        static constexpr uint32_t kMaxRules = 16;
        Dia::Core::Containers::DynamicArrayC<uint64_t,                    kMaxRules> mRulePayloads;
        Dia::Core::Containers::DynamicArrayC<Dia::Mailbox::SubscriberSet, kMaxRules> mRuleResults;
    };

} // namespace Dia::Mailbox::Testing
