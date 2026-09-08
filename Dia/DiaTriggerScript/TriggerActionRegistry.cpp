#include "TriggerActionRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <unordered_map>

namespace Dia
{
    namespace TriggerScript
    {
        struct TriggerActionRegistry::Impl
        {
            std::unordered_map<unsigned int, ITriggerActionHandler*> table;
        };

        TriggerActionRegistry::TriggerActionRegistry()
            : mImpl(new Impl())
        {
        }

        TriggerActionRegistry::~TriggerActionRegistry()
        {
            delete mImpl;
        }

        void TriggerActionRegistry::Register(Dia::Core::StringCRC actionType, ITriggerActionHandler* handler)
        {
            mImpl->table[actionType.Value()] = handler;
        }

        bool TriggerActionRegistry::Has(Dia::Core::StringCRC actionType) const
        {
            return mImpl->table.count(actionType.Value()) != 0;
        }

        void TriggerActionRegistry::Dispatch(Dia::Core::StringCRC actionType, const ActionContext& ctx) const
        {
            const auto it = mImpl->table.find(actionType.Value());
            DIA_ASSERT(it != mImpl->table.end(), "TriggerActionRegistry: no handler registered for action type");
            if (it != mImpl->table.end())
            {
                it->second->Execute(ctx);
            }
        }

    } // namespace TriggerScript
} // namespace Dia
