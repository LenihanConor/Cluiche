#pragma once
#include <cstdint>
#include <DiaCore/Containers/Handle.h>

namespace Dia::Entity {

    class EntityTag {};
    using Entity = Dia::Core::Handle<EntityTag>;

    inline constexpr uint32_t kMaxEntitiesPerDomain       = 1024;
    inline constexpr uint32_t kMaxMutationsPerFrame       = 256;
    inline constexpr uint32_t kMaxComponentTypesPerDomain = 64;
    inline constexpr uint32_t kMaxDebugNameLength         = 64;

} // namespace Dia::Entity
