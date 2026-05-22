#pragma once
// =============================================================================
// DiaRigidBody2DSerializers.h
// serialize() free functions for DiaRigidBody2D configuration value types.
//
// TYPES COVERED:
//   ResponseConfig, ConstraintSolverConfig, WorldDef
//   All structs are fully public — no friend declarations required.
//
// SKIPPED FIELDS:
//   WorldDef::broadPhase — non-owning raw pointer to ISpatialStructure;
//   the pointer target is caller-managed and must not be serialized.
//
// ADL NOTE:
//   All serialize() functions are placed in namespace Dia::RigidBody2D (same
//   namespace as the types), so Argument-Dependent Lookup finds them
//   automatically when an archive calls serialize(ar, value, version).
//
// USAGE:
//   #include "DiaCore/Reflect/Reflect.h"
//   #include "DiaCore/Reflect/JsonArchive.h"
//   #include "DiaRigidBody2D/DiaRigidBody2DSerializers.h"
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaMaths/DiaMathsSerializers.h"
#include "DiaRigidBody2D/Response/ResolveCollisions.h"
#include "DiaRigidBody2D/Constraints/ConstraintSolver.h"
#include "DiaRigidBody2D/World/WorldDef.h"

namespace Dia::RigidBody2D {

// -----------------------------------------------------------------------------
// ResponseConfig — baumgarteSlop, baumgarteFactor, restitutionVelocitySlop
// -----------------------------------------------------------------------------
DIA_SERIALIZE(ResponseConfig, 1)
    DIA_FIELD(baumgarteSlop)
    DIA_FIELD(baumgarteFactor)
    DIA_FIELD(restitutionVelocitySlop)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// ConstraintSolverConfig — iterations (int)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(ConstraintSolverConfig, 1)
    DIA_FIELD(iterations)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// WorldDef — gravity (Vector2D), fixedTimestep (float), maxSubSteps (int),
//            responseConfig, constraintConfig, sleep thresholds.
//            broadPhase skipped — non-owning raw pointer.
// -----------------------------------------------------------------------------
DIA_SERIALIZE(WorldDef, 1)
    DIA_FIELD(gravity)
    DIA_FIELD(fixedTimestep)
    DIA_FIELD(maxSubSteps)
    DIA_FIELD(responseConfig)
    DIA_FIELD(constraintConfig)
    DIA_FIELD(sleepLinearThreshold)
    DIA_FIELD(sleepAngularThreshold)
    DIA_FIELD(sleepTimeThreshold)
DIA_SERIALIZE_END

}  // namespace Dia::RigidBody2D
