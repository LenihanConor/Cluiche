#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaCore/Reflect/ReflectMacros.h>

// Serialize free function — must be defined before DIA_COMPONENT_REGISTER.
//
// position and radius are FIELD-declared members, serialized by name.
// mLayerMask is serialized as "layerMask" using bits 0–30 only; bit 31 is a
// runtime-only dirty flag and is never written to or read from persistent data.
//
// The local-copy idiom works for both read and write archives:
//   - Write: layerMaskBits is set from obj, ar writes it out.
//   - Read:  ar fills layerMaskBits from JSON, then we restore it into obj
//            while preserving the current bit-31 state.
DIA_SERIALIZE(Dia::EntitySpatial::SpatialComponent, Dia::EntitySpatial::SpatialComponent::kVersion)
    DIA_FIELD(position)
    DIA_FIELD(radius)
    {
        uint32_t layerMaskBits = obj.mLayerMask & 0x7FFFFFFFu;
        _ar_ & Dia::Reflect::named("layerMask", layerMaskBits);
        obj.mLayerMask = (obj.mLayerMask & 0x80000000u) | (layerMaskBits & 0x7FFFFFFFu);
    }
DIA_SERIALIZE_END

namespace Dia::EntitySpatial {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

// Field metadata — one entry per member exposed to the editor / reflection system.
// position: Vector2D (Nested), radius: float (Primitive), mLayerMask: uint32_t (Primitive).
static Dia::Entity::FieldDesc s_SpatialComponent_fields[] = {
    DIA_FIELD_ENTRY(Dia::Maths::Vector2D, position,   SpatialComponent)
    DIA_FIELD_ENTRY(float,               radius,     SpatialComponent)
    DIA_FIELD_ENTRY(uint32_t,            mLayerMask, SpatialComponent)
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// Registration — defines kTypeId, GetDesc(), and triggers ComponentRegistry entry.
// IsUpdatable = false, IsReadOnly = true (pure data component).
DIA_COMPONENT_REGISTER(SpatialComponent, "spatial-component", false, true,
    s_SpatialComponent_fields, DIA_ARRAY_COUNT(s_SpatialComponent_fields),
    nullptr, 0,
    nullptr, 0)

} // namespace Dia::EntitySpatial
