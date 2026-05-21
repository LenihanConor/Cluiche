#include <DiaMailbox/Testing/MockRouter.h>

namespace Dia::Mailbox::Testing {

    MockRouter::MockRouter(Dia::Core::StringCRC routerId)
        : mRouterId(routerId), mResolveCallCount(0)
    {}

    void MockRouter::AddRule(uint64_t payload, const Dia::Mailbox::SubscriberSet& matched) {
        // Update existing rule
        for (uint32_t i = 0; i < mRulePayloads.Size(); ++i) {
            if (mRulePayloads[i] == payload) {
                mRuleResults[i] = matched;
                return;
            }
        }
        // Add new rule if not full
        if (!mRulePayloads.IsFull()) {
            mRulePayloads.Add(payload);
            mRuleResults.Add(matched);
        }
    }

    void MockRouter::ClearRules() {
        mRulePayloads.RemoveAll();
        mRuleResults.RemoveAll();
        mResolveCallCount = 0;
    }

    int MockRouter::ResolveCallCount() const {
        return mResolveCallCount;
    }

    Dia::Core::StringCRC MockRouter::GetRouterId() const {
        return mRouterId;
    }

    void MockRouter::Resolve(const Dia::Mailbox::Address& addr,
                             const Dia::Mailbox::SubscriberSet& liveSubscribers,
                             Dia::Mailbox::SubscriberSet& outMatched) {
        ++mResolveCallCount;

        for (uint32_t i = 0; i < mRulePayloads.Size(); ++i) {
            if (mRulePayloads[i] == addr.payload) {
                // Copy matched subscribers that are also in liveSubscribers
                for (uint32_t m = 0; m < mRuleResults[i].Size(); ++m) {
                    const Dia::Mailbox::SubscriberId& id = mRuleResults[i][m];
                    // Check if id is in liveSubscribers
                    for (uint32_t j = 0; j < liveSubscribers.Size(); ++j) {
                        if (liveSubscribers[j] == id) {
                            if (!outMatched.IsFull()) {
                                outMatched.Add(id);
                            }
                            break;
                        }
                    }
                }
                return;
            }
        }
        // No rule found: outMatched stays empty (already cleared by caller)
    }

} // namespace Dia::Mailbox::Testing
