#include "OperatorRegistry.h"

#include <DiaObservation/Log/DiaLog.h>

#include <unordered_map>

namespace Dia
{
    namespace HTN
    {
        struct OperatorRegistry::Impl
        {
            std::unordered_map<unsigned int, OperatorBinding> table;
        };

        OperatorRegistry::OperatorRegistry()
            : mImpl(new Impl())
        {
        }

        OperatorRegistry::~OperatorRegistry()
        {
            delete mImpl;
        }

        void OperatorRegistry::Register(Dia::Core::StringCRC operatorId, OperatorFn fn)
        {
            mImpl->table[operatorId.Value()] = OperatorBinding(fn);
            DIA_LOG_DEBUG("HTN", "htn.registry.register: op=%u", operatorId.Value());
        }

        void OperatorRegistry::Register(Dia::Core::StringCRC operatorId, OperatorBinding binding)
        {
            mImpl->table[operatorId.Value()] = binding;
            DIA_LOG_DEBUG("HTN", "htn.registry.register_binding: op=%u", operatorId.Value());
        }

        OperatorBinding OperatorRegistry::Find(Dia::Core::StringCRC operatorId) const
        {
            const auto it = mImpl->table.find(operatorId.Value());
            if (it == mImpl->table.end())
                return OperatorBinding{};
            return it->second;
        }

        bool OperatorRegistry::Has(Dia::Core::StringCRC operatorId) const
        {
            return mImpl->table.count(operatorId.Value()) != 0;
        }

    } // namespace HTN
} // namespace Dia
