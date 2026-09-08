#pragma once
// EntityRef<TComponent>::Resolve() implementation.
// Included at the bottom of Domain.inl, after Domain is fully defined.
// DO NOT include this file directly — it is pulled in transitively via Domain.h.
#include <DiaEntity/EntityRef.h>

namespace Dia::Entity {

    template<ComponentType TComponent>
    bool EntityRef<TComponent>::Resolve(const Domain& domain) const {
        if (!entity.IsValid()) return false;
        return domain.HasComponent<TComponent>(entity);
    }

} // namespace Dia::Entity
