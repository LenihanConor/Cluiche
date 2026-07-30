#include <DiaUtilityAI/GroupConsiderationContext.h>
#include <unordered_map>
#include <climits>

namespace Dia
{
    namespace UtilityAI
    {
        struct GroupConsiderationContext::Impl
        {
            std::unordered_map<unsigned int, int> counts;
        };

        GroupConsiderationContext::GroupConsiderationContext()
            : mImpl(new Impl())
        {
        }

        GroupConsiderationContext::~GroupConsiderationContext()
        {
            delete mImpl;
        }

        void GroupConsiderationContext::Increment(Dia::Core::StringCRC actionId)
        {
            auto& count = mImpl->counts[actionId.Value()];
            if (count < INT_MAX) ++count;
        }

        void GroupConsiderationContext::Decrement(Dia::Core::StringCRC actionId)
        {
            auto it = mImpl->counts.find(actionId.Value());
            if (it != mImpl->counts.end() && it->second > 0)
                --it->second;
        }

        int GroupConsiderationContext::GetCount(Dia::Core::StringCRC actionId) const
        {
            auto it = mImpl->counts.find(actionId.Value());
            if (it != mImpl->counts.end())
                return it->second;
            return 0;
        }

        void GroupConsiderationContext::Reset()
        {
            mImpl->counts.clear();
        }

    } // namespace UtilityAI
} // namespace Dia
