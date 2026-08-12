////////////////////////////////////////////////////////////////////////////////
// Filename: DiaDebugDomainRegistry.h
// Description: Grouped registry of IDebugDomain instances. Replaces the flat
//              64-entry DebugLayerManager array as the container for all debug
//              domain modules. Supports fast lookup by domain ID and
//              group-based iteration without exposing STL in the public API.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

#include <unordered_map>
#include <vector>

namespace Dia
{
    namespace VisualDebugger
    {
        ////////////////////////////////////////////////////////////////////////////////
        // DiaDebugDomainRegistry
        //
        // Value type — not a singleton. Owned by whichever Module manages the
        // DiaDebugPanel (SD-DBG-DOMAIN).
        //
        // Duplicate registration policy:
        //   Registering a domain whose domainId is already present is a no-op.
        //   DIA_ASSERT fires in debug builds to surface the programming error.
        ////////////////////////////////////////////////////////////////////////////////
        class DiaDebugDomainRegistry
        {
        public:
            // Register a domain. Second call with the same domainId is a no-op
            // (DIA_ASSERT fires in debug builds).
            void Register(IDebugDomain& domain);

            // Unregister a domain. No-op if the domain is not registered.
            void Unregister(IDebugDomain& domain);

            // Find a domain by its stable ID. Returns nullptr if not registered.
            IDebugDomain* FindDomain(Dia::Core::StringCRC domainId) const;

            // Visit all domains in a given group.
            // groupId should be a canonical DebugGroupAccents key
            // (e.g. StringCRC("Physics"), StringCRC("AI")).
            // fn signature: void(IDebugDomain&)
            template<typename Fn>
            void VisitGroup(Dia::Core::StringCRC groupId, Fn&& fn) const;

            // Visit every registered domain.
            // fn signature: void(IDebugDomain&)
            template<typename Fn>
            void VisitAll(Fn&& fn) const;

            // Total number of registered domains.
            int GetDomainCount() const;

        private:
            // Fast lookup by domainId.Value()
            std::unordered_map<Dia::Core::StringCRC, IDebugDomain*> mById;

            // Group-based lookup: groupId → list of domains in that group.
            std::unordered_map<Dia::Core::StringCRC, std::vector<IDebugDomain*>> mByGroup;
        };

        // ----------------------------------------------------------------
        // Template definitions (must be in header)
        // ----------------------------------------------------------------

        template<typename Fn>
        void DiaDebugDomainRegistry::VisitGroup(Dia::Core::StringCRC groupId, Fn&& fn) const
        {
            auto it = mByGroup.find(groupId);
            if (it == mByGroup.end())
            {
                return;
            }

            for (IDebugDomain* domain : it->second)
            {
                fn(*domain);
            }
        }

        template<typename Fn>
        void DiaDebugDomainRegistry::VisitAll(Fn&& fn) const
        {
            for (const auto& [key, domain] : mById)
            {
                fn(*domain);
            }
        }

    } // namespace VisualDebugger
} // namespace Dia

#endif // DIA_DEBUG
