#pragma once
// =============================================================================
// DiaIK2DSerializers.h
// serialize() free functions for DiaIK2D value types.
//
// TYPES COVERED:
//   JointLimitDef, PoleVector, IKChainDef
//   All structs are fully public — no friend declarations required.
//
// ADL NOTE:
//   All serialize() functions are placed in namespace Dia::IK2D (same
//   namespace as the types), so Argument-Dependent Lookup finds them
//   automatically when an archive calls serialize(ar, value, version).
//
// USAGE:
//   #include "DiaCore/Reflect/Reflect.h"
//   #include "DiaCore/Reflect/JsonArchive.h"
//   #include "DiaIK2D/DiaIK2DSerializers.h"
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/Reflect/DiaCoreSerializers.h"
#include "DiaIK2D/IKChainDef.h"

namespace Dia
{
    namespace IK2D
    {

// -----------------------------------------------------------------------------
// JointLimitDef — minAngle, maxAngle (float radians), enabled (bool)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(JointLimitDef, 1)
    DIA_FIELD(minAngle)
    DIA_FIELD(maxAngle)
    DIA_FIELD(enabled)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// PoleVector — direction (Vector2D), weight (float)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(PoleVector, 1)
    DIA_FIELD(direction)
    DIA_FIELD(weight)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// IKChainDef — id, startBoneId, endBoneId (StringCRC); reachWeight, tolerance
//              (float); maxIterations (int); jointLimits (DynamicArrayC)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(IKChainDef, 1)
    DIA_FIELD(id)
    DIA_FIELD(startBoneId)
    DIA_FIELD(endBoneId)
    DIA_FIELD(reachWeight)
    DIA_FIELD(maxIterations)
    DIA_FIELD(tolerance)
    DIA_FIELD(jointLimits)
DIA_SERIALIZE_END

    }  // namespace IK2D
}  // namespace Dia
