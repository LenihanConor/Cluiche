#pragma once
#include <cstdint>
#include <tuple>
#include <algorithm>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <diaentitytemplate/Entity.h>

namespace Dia::Entity {

    class Domain;

    // Compute the order-independent query signature from a list of component types.
    // XOR-folds the sorted type CRCs.  kTypeId is static const (not constexpr), so
    // this is a runtime computation.  std::sort is allowed internally.
    // Returns Dia::Core::CRC (not StringCRC) because the value is computed, not from a string.
    template<class... TComponents>
    inline Dia::Core::CRC ComputeQuerySignature() {
        uint32_t crcs[] = { TComponents::kTypeId.Value()... };
        const uint32_t count = static_cast<uint32_t>(sizeof...(TComponents));
        std::sort(crcs, crcs + count);
        uint32_t combined = 0u;
        for (uint32_t i = 0; i < count; ++i) {
            combined ^= crcs[i];
        }
        return Dia::Core::CRC(combined);
    }

    // A lightweight view over a cached query result.
    // Populated by Domain::Query<TComponents...>() — do not construct directly.
    // Pointers inside entries are stable until the next Domain::EndOfFrame call.
    template<class... TComponents>
    class QueryView {
    public:
        // Entry yielded by the iterator.  std::tuple is allowed internally per spec SD-ENT (PD-004).
        struct Entry {
            Entity                        entity;
            std::tuple<TComponents*...>   components;
        };

        // Forward iterator over QueryView entries.
        class Iterator {
        public:
            Iterator(const QueryView* view, uint32_t idx);

            Entry       operator*()  const;
            Iterator&   operator++();
            bool        operator!=(const Iterator& other) const;

        private:
            const QueryView* mView;
            uint32_t         mIdx;
        };

        Iterator begin() const;
        Iterator end()   const;

        // Number of entities matching the query signature.
        uint32_t Count() const;

        // Internal: called by Domain::Query to wire the view to a cache entry.
        void SetSource(
            const Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain>* entities,
            Domain* domain);

    private:
        const Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain>* mEntities = nullptr;
        Domain* mDomain = nullptr;
    };

} // namespace Dia::Entity
// QueryView.inl is included at the bottom of Domain.inl (after Domain is fully defined).
