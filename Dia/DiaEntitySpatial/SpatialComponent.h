#pragma once
#ifndef DIA_ENTITYSPATIAL_SPATIALCOMPONENT_H
#define DIA_ENTITYSPATIAL_SPATIALCOMPONENT_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Core/Assert.h>

namespace Dia::EntitySpatial {

// SpatialComponent — opt-in data component for spatial indexing.
//
// Any entity with this component is tracked by EntitySpatialModule in the
// spatial structure (ISpatialStructure<Entity>). On first frame the entity is
// always inserted (mLayerMask starts with bit 31 set). On subsequent frames
// EntitySpatialModule only re-indexes entities whose dirty bit is set.
//
// Bit layout of mLayerMask:
//   bits 0–30 : game-visible layer mask (set via SetLayerMask)
//   bit  31   : engine-reserved dirty flag (set by MarkDirty / SetLayerMask;
//               cleared by EntitySpatialModule::Update after re-indexing)
//
// Memory footprint: 16 bytes (position 8 + radius 4 + mLayerMask 4).
class SpatialComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(SpatialComponent, "spatial-component", 1)
    DIA_READONLY

    FIELD(Dia::Maths::Vector2D, position, Dia::Maths::Vector2D(0.f, 0.f))
    FIELD(float, radius, 1.0f)

public:
    // mLayerMask is declared manually (not FIELD) because:
    //   1. It has a non-trivial default (0x80000000u — starts dirty).
    //   2. Bit 31 is engine-internal and must not be serialized raw.
    //   3. Serialization saves only bits 0–30 (see SpatialComponent.cpp).
    uint32_t mLayerMask = 0x80000000u;

    // -------------------------------------------------------------------------
    // Game-facing API
    // -------------------------------------------------------------------------

    // Set bits 0–30 of the layer mask. Bit 31 MUST NOT be set by callers —
    // that bit is reserved for the engine dirty flag. Automatically marks dirty
    // after any successful change so EntitySpatialModule re-indexes this entity.
    void SetLayerMask(uint32_t mask)
    {
        DIA_ASSERT((mask & 0x80000000u) == 0,
            "SpatialComponent::SetLayerMask: bit 31 is reserved for engine use");
        mLayerMask = (mLayerMask & 0x80000000u) | (mask & 0x7FFFFFFFu);
        mLayerMask |= 0x80000000u; // mark dirty after any mask change
    }

    // Returns bits 0–30 only (bit 31 masked out).
    uint32_t GetLayerMask() const { return mLayerMask & 0x7FFFFFFFu; }

    // Returns true if the dirty flag (bit 31) is set.
    bool IsDirty() const { return (mLayerMask & 0x80000000u) != 0; }

    // -------------------------------------------------------------------------
    // EntitySpatialModule-facing helpers
    // -------------------------------------------------------------------------

    // Set bit 31 — marks this entity for re-indexing on the next Update.
    void MarkDirty()  { mLayerMask |=  0x80000000u; }

    // Clear bit 31 — called by EntitySpatialModule after successful re-indexing.
    void ClearDirty() { mLayerMask &= ~0x80000000u; }

private:
};

} // namespace Dia::EntitySpatial

#endif // DIA_ENTITYSPATIAL_SPATIALCOMPONENT_H
