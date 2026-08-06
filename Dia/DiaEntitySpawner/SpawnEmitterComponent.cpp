#include <DiaEntitySpawner/SpawnEmitterComponent.h>
#include <DiaCore/Reflect/ReflectMacros.h>

// Serialize free function — must be defined before DIA_COMPONENT_REGISTER.
DIA_SERIALIZE(Dia::EntitySpawner::SpawnEmitterComponent, Dia::EntitySpawner::SpawnEmitterComponent::kVersion)
    DIA_FIELD(blueprintId)
    DIA_FIELD(rate)
    DIA_FIELD(burstCount)
    DIA_FIELD(cap)
    DIA_FIELD(lifetime)
    DIA_FIELD(despawnRadius)
    DIA_FIELD(active)
DIA_SERIALIZE_END

namespace Dia::EntitySpawner {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

// Field metadata — one entry per member exposed to the editor / reflection system.
static Dia::Entity::FieldDesc s_SpawnEmitterComponent_fields[] = {
    DIA_FIELD_ENTRY(Dia::Core::StringCRC, blueprintId,   SpawnEmitterComponent)
    DIA_FIELD_ENTRY(float,               rate,           SpawnEmitterComponent)
    DIA_FIELD_ENTRY(int,                 burstCount,     SpawnEmitterComponent)
    DIA_FIELD_ENTRY(int,                 cap,            SpawnEmitterComponent)
    DIA_FIELD_ENTRY(float,               lifetime,       SpawnEmitterComponent)
    DIA_FIELD_ENTRY(float,               despawnRadius,  SpawnEmitterComponent)
    DIA_FIELD_ENTRY(bool,                active,         SpawnEmitterComponent)
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// Registration — defines kTypeId, GetDesc(), and triggers ComponentRegistry entry.
// IsUpdatable = false, IsReadOnly = true (pure data component).
DIA_COMPONENT_REGISTER(SpawnEmitterComponent, "spawn-emitter-component", false, true,
    s_SpawnEmitterComponent_fields, DIA_ARRAY_COUNT(s_SpawnEmitterComponent_fields),
    nullptr, 0,
    nullptr, 0)

} // namespace Dia::EntitySpawner
