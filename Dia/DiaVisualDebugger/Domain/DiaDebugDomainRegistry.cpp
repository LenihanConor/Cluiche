////////////////////////////////////////////////////////////////////////////////
// Filename: DiaDebugDomainRegistry.cpp
// Description: Implementation of DiaDebugDomainRegistry.
////////////////////////////////////////////////////////////////////////////////
#include "DiaDebugDomainRegistry.h"

#ifdef DIA_DEBUG

#include <DiaCore/Core/Assert.h>

#include <algorithm>

namespace Dia
{
    namespace VisualDebugger
    {
        void DiaDebugDomainRegistry::Register(IDebugDomain& domain)
        {
            const Dia::Core::StringCRC domainId = domain.GetDomainId();

            // Duplicate detection: same domainId is a no-op, but assert to surface
            // the programming error in debug builds.
            if (mById.count(domainId) != 0)
            {
                DIA_ASSERT(false, "DiaDebugDomainRegistry::Register — domainId already registered (no-op)");
                return;
            }

            mById[domainId] = &domain;

            const Dia::Core::StringCRC groupId = domain.GetGroup();
            mByGroup[groupId].push_back(&domain);
        }

        void DiaDebugDomainRegistry::Unregister(IDebugDomain& domain)
        {
            const Dia::Core::StringCRC domainId = domain.GetDomainId();

            auto byIdIt = mById.find(domainId);
            if (byIdIt == mById.end())
            {
                return;  // not registered — no-op
            }

            mById.erase(byIdIt);

            // Remove from the group bucket
            const Dia::Core::StringCRC groupId = domain.GetGroup();
            auto byGroupIt = mByGroup.find(groupId);
            if (byGroupIt != mByGroup.end())
            {
                std::vector<IDebugDomain*>& bucket = byGroupIt->second;
                bucket.erase(
                    std::remove(bucket.begin(), bucket.end(), &domain),
                    bucket.end()
                );

                if (bucket.empty())
                {
                    mByGroup.erase(byGroupIt);
                }
            }
        }

        IDebugDomain* DiaDebugDomainRegistry::FindDomain(Dia::Core::StringCRC domainId) const
        {
            auto it = mById.find(domainId);
            if (it == mById.end())
            {
                return nullptr;
            }
            return it->second;
        }

        int DiaDebugDomainRegistry::GetDomainCount() const
        {
            return static_cast<int>(mById.size());
        }

        void DiaDebugDomainRegistry::Clear()
        {
            mById.clear();
            mByGroup.clear();
        }

    } // namespace VisualDebugger
} // namespace Dia

#endif // DIA_DEBUG
