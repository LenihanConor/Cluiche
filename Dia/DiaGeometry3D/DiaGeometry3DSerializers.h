#pragma once
// =============================================================================
// DiaGeometry3DSerializers.h
// serialize() free functions for DiaGeometry3D shape types.
//
// USAGE:
//   #include "DiaGeometry3D/DiaGeometry3DSerializers.h"
//
// ADL NOTE:
//   All serialize() functions are in namespace Dia::Geometry3D, matching
//   the types so ADL finds them automatically.
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaMaths/DiaMathsSerializers.h"
#include "DiaGeometry3D/Shapes/Sphere.h"
#include "DiaGeometry3D/Shapes/AABB.h"
#include "DiaGeometry3D/Shapes/OOBB.h"
#include "DiaGeometry3D/Shapes/Capsule.h"

namespace Dia::Geometry3D {

// -----------------------------------------------------------------------------
// Sphere  — private Vector3D mCenter, float mRadius
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Sphere, 1)
    DIA_FIELD(mCenter)
    DIA_FIELD(mRadius)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// AABB  — private Vector3D mMin, mMax
// -----------------------------------------------------------------------------
DIA_SERIALIZE(AABB, 1)
    DIA_FIELD(mMin)
    DIA_FIELD(mMax)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// OOBB  — private Vector3D mCenter, mHalfExtents; Quaternion mOrientation
// -----------------------------------------------------------------------------
DIA_SERIALIZE(OOBB, 1)
    DIA_FIELD(mCenter)
    DIA_FIELD(mHalfExtents)
    DIA_FIELD(mOrientation)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Capsule  — private Vector3D mStartA, mEndB; float mRadius
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Capsule, 1)
    DIA_FIELD(mStartA)
    DIA_FIELD(mEndB)
    DIA_FIELD(mRadius)
DIA_SERIALIZE_END

}  // namespace Dia::Geometry3D
