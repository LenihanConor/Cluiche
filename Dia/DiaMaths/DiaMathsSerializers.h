#pragma once
// =============================================================================
// DiaMathsSerializers.h
// serialize() free functions for DiaMaths types.
//
// USAGE:
//   #include "DiaMaths/DiaMathsSerializers.h"
//
// ADL NOTE:
//   All serialize() functions are placed in namespace Dia::Maths (same
//   namespace as the types), so Argument-Dependent Lookup finds them
//   automatically when an archive calls serialize(ar, value, version).
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Vector/Vector3D.h"
#include "DiaMaths/Vector/Vector4D.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Matrix/Matrix22.h"
#include "DiaMaths/Matrix/Matrix33.h"
#include "DiaMaths/Matrix/Matrix34.h"
#include "DiaMaths/Matrix/Matrix44.h"
#include "DiaMaths/Quaternion/Quaternion.h"
#include "DiaMaths/Transform/Transform2D.h"
#include "DiaMaths/Transform/Transform3D.h"

namespace Dia::Maths {

// -----------------------------------------------------------------------------
// Vector2D  — public float x, y
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Vector2D, 1)
    DIA_FIELD(x)
    DIA_FIELD(y)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Vector3D  — public float x, y, z
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Vector3D, 1)
    DIA_FIELD(x)
    DIA_FIELD(y)
    DIA_FIELD(z)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Vector4D  — public float x, y, z, w
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Vector4D, 1)
    DIA_FIELD(x)
    DIA_FIELD(y)
    DIA_FIELD(z)
    DIA_FIELD(w)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Angle  — private float mRadian; friend declared in Angle.h
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Angle, 1)
    DIA_FIELD(mRadian)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Matrix22  — private float mElement[4], exposed via public operator[](int)
//   Layout: [0]=e00, [1]=e01, [2]=e10, [3]=e11
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Matrix22, 1)
    _ar_ & Dia::Reflect::named("e00", obj[0]);
    _ar_ & Dia::Reflect::named("e01", obj[1]);
    _ar_ & Dia::Reflect::named("e10", obj[2]);
    _ar_ & Dia::Reflect::named("e11", obj[3]);
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Matrix33  — public float m[3][3] (row-major)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Matrix33, 1)
    DIA_FIELD_NAMED("m00", m[0][0])
    DIA_FIELD_NAMED("m01", m[0][1])
    DIA_FIELD_NAMED("m02", m[0][2])
    DIA_FIELD_NAMED("m10", m[1][0])
    DIA_FIELD_NAMED("m11", m[1][1])
    DIA_FIELD_NAMED("m12", m[1][2])
    DIA_FIELD_NAMED("m20", m[2][0])
    DIA_FIELD_NAMED("m21", m[2][1])
    DIA_FIELD_NAMED("m22", m[2][2])
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Matrix34  — public float m[3][4] (row-major)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Matrix34, 1)
    DIA_FIELD_NAMED("m00", m[0][0])
    DIA_FIELD_NAMED("m01", m[0][1])
    DIA_FIELD_NAMED("m02", m[0][2])
    DIA_FIELD_NAMED("m03", m[0][3])
    DIA_FIELD_NAMED("m10", m[1][0])
    DIA_FIELD_NAMED("m11", m[1][1])
    DIA_FIELD_NAMED("m12", m[1][2])
    DIA_FIELD_NAMED("m13", m[1][3])
    DIA_FIELD_NAMED("m20", m[2][0])
    DIA_FIELD_NAMED("m21", m[2][1])
    DIA_FIELD_NAMED("m22", m[2][2])
    DIA_FIELD_NAMED("m23", m[2][3])
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Matrix44  — public float m[4][4] (row-major)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Matrix44, 1)
    DIA_FIELD_NAMED("m00", m[0][0])
    DIA_FIELD_NAMED("m01", m[0][1])
    DIA_FIELD_NAMED("m02", m[0][2])
    DIA_FIELD_NAMED("m03", m[0][3])
    DIA_FIELD_NAMED("m10", m[1][0])
    DIA_FIELD_NAMED("m11", m[1][1])
    DIA_FIELD_NAMED("m12", m[1][2])
    DIA_FIELD_NAMED("m13", m[1][3])
    DIA_FIELD_NAMED("m20", m[2][0])
    DIA_FIELD_NAMED("m21", m[2][1])
    DIA_FIELD_NAMED("m22", m[2][2])
    DIA_FIELD_NAMED("m23", m[2][3])
    DIA_FIELD_NAMED("m30", m[3][0])
    DIA_FIELD_NAMED("m31", m[3][1])
    DIA_FIELD_NAMED("m32", m[3][2])
    DIA_FIELD_NAMED("m33", m[3][3])
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Quaternion  — public float x, y, z, w
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Quaternion, 1)
    DIA_FIELD(x)
    DIA_FIELD(y)
    DIA_FIELD(z)
    DIA_FIELD(w)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Transform2D  — private members; friend declared in Transform2D.h
//   mParent skipped — hierarchy reconstructed at scene level.
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Transform2D, 1)
    DIA_FIELD(mLocalPosition)
    DIA_FIELD(mLocalRotation)
    DIA_FIELD(mLocalScale)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Transform3D  — private members; friend declared in Transform3D.h
//   mParent skipped — hierarchy reconstructed at scene level.
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Transform3D, 1)
    DIA_FIELD(mLocalPosition)
    DIA_FIELD(mLocalRotation)
    DIA_FIELD(mLocalScale)
DIA_SERIALIZE_END

}  // namespace Dia::Maths
