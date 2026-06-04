#pragma once
#include <concepts>
#include <type_traits>
#include <cstdint>
#include <DiaEntity/Entity.h>
#include <DiaEntity/IComponent.h>
#include <DiaCore/Reflect/Archive.h>

namespace Dia::Entity {

    // Forward declaration — Resolve() uses Domain, but EntityRef.h must not include
    // Domain.h (that would be circular). Callers must include Domain.h before
    // calling Resolve(). The full definition is in EntityRef.inl, included at the
    // bottom of Domain.inl after Domain is fully defined.
    class Domain;

    // C++20 concept: TComponent must derive from IComponent.
    template<class T>
    concept ComponentType = std::is_base_of_v<IComponent, T>;

    // Typed cross-entity reference slot. Default-constructed holds Entity::Invalid().
    // Resolve() checks that the held entity has TComponent attached in the given domain.
    template<ComponentType TComponent>
    struct EntityRef {
        Entity entity = Entity::Invalid();

        // True if the stored handle is non-invalid (does NOT imply the entity is alive).
        bool IsValid() const { return entity.IsValid(); }

        // Returns true if the entity is alive in domain and has TComponent attached.
        // Returns false (not assert) if the entity is Invalid or the component is absent.
        // Defined in EntityRef.inl, included at the bottom of Domain.inl.
        bool Resolve(const Domain& domain) const;
    };

} // namespace Dia::Entity

// =============================================================================
// Free serialize function for EntityRef<TComponent>.
//
// Serialized as a single "entityIndex" uint32 field.
//   Write: stores entity index, or 0xFFFFFFFF for Entity::Invalid().
//   Read:  reconstructs a placeholder Entity(index, 1u); generation=1 by
//          convention (Blueprint Pass 2 would fix up the full handle).
//
// This template lives outside the Dia::Entity namespace so it participates
// in ADL via the template argument, matching how DIA_SERIALIZE works.
// =============================================================================
template<class Archive, class TComponent>
void serialize(Archive& ar, Dia::Entity::EntityRef<TComponent>& ref, unsigned /*version*/ = 0u) {
    static_assert(Dia::Reflect::Archive<Archive>,
        "EntityRef serialize: Archive type does not satisfy Dia::Reflect::Archive concept");

    uint32_t idx = ref.entity.IsValid()
        ? ref.entity.GetIndex()
        : 0xFFFFFFFFu;

    ar & Dia::Reflect::named("entityIndex", idx);

    if (ar.IsReading()) {
        if (idx == 0xFFFFFFFFu) {
            ref.entity = Dia::Entity::Entity::Invalid();
        } else {
            // Placeholder: index preserved, generation=1. Blueprint Pass 2 resolves fully.
            ref.entity = Dia::Entity::Entity(idx, 1u);
        }
    }
}
