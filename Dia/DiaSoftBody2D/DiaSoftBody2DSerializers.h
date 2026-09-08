#pragma once
// =============================================================================
// DiaSoftBody2DSerializers.h
// serialize() free functions for DiaSoftBody2D configuration value types.
//
// TYPES COVERED:
//   WorldDef, RopeDef, ClothDef
//
// SKIPPED FIELDS:
//   WorldDef::rigidBodyWorld — non-owning raw pointer to PhysicsWorld;
//   the pointer target is caller-managed and must not be serialized.
//   RopeDef::startAnchor, RopeDef::endAnchor — non-owning raw pointers to Body2DBase;
//   anchor targets are caller-managed and must not be serialized.
//
// SKIPPED TYPES:
//   Particle, DistanceConstraint — runtime simulation state, not configuration values.
//
// ADL NOTE:
//   All serialize() functions are placed in namespace Dia::SoftBody2D (same
//   namespace as the types), so Argument-Dependent Lookup finds them
//   automatically when an archive calls serialize(ar, value, version).
//
// USAGE:
//   #include "DiaCore/Reflect/Reflect.h"
//   #include "DiaCore/Reflect/JsonArchive.h"
//   #include "DiaSoftBody2D/DiaSoftBody2DSerializers.h"
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaMaths/DiaMathsSerializers.h"
#include "DiaSoftBody2D/SoftBodyWorld.h"
#include "DiaSoftBody2D/Rope.h"
#include "DiaSoftBody2D/Cloth.h"

namespace Dia::SoftBody2D {

// -----------------------------------------------------------------------------
// WorldDef — gravity (Vector2D), fixedTimestep, maxSubSteps, solverIterations.
//            rigidBodyWorld skipped — non-owning raw pointer.
// -----------------------------------------------------------------------------
DIA_SERIALIZE(WorldDef, 1)
    DIA_FIELD(gravity)
    DIA_FIELD(fixedTimestep)
    DIA_FIELD(maxSubSteps)
    DIA_FIELD(solverIterations)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// RopeDef — id, startPoint, endPoint, particleCount, mass, stiffness,
//           particleRadius, maxStretch.
//           startAnchor, endAnchor skipped — non-owning raw pointers.
// -----------------------------------------------------------------------------
DIA_SERIALIZE(RopeDef, 1)
    DIA_FIELD(id)
    DIA_FIELD(startPoint)
    DIA_FIELD(endPoint)
    DIA_FIELD(particleCount)
    DIA_FIELD(mass)
    DIA_FIELD(stiffness)
    DIA_FIELD(particleRadius)
    DIA_FIELD(maxStretch)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// ClothDef — id, origin, width, height, resX, resY, mass, structuralStiffness,
//            shearStiffness, bendStiffness, particleRadius, maxStretch, pinTopRow.
// -----------------------------------------------------------------------------
DIA_SERIALIZE(ClothDef, 1)
    DIA_FIELD(id)
    DIA_FIELD(origin)
    DIA_FIELD(width)
    DIA_FIELD(height)
    DIA_FIELD(resX)
    DIA_FIELD(resY)
    DIA_FIELD(mass)
    DIA_FIELD(structuralStiffness)
    DIA_FIELD(shearStiffness)
    DIA_FIELD(bendStiffness)
    DIA_FIELD(particleRadius)
    DIA_FIELD(maxStretch)
    DIA_FIELD(pinTopRow)
DIA_SERIALIZE_END

}  // namespace Dia::SoftBody2D
