#pragma once
// =============================================================================
// DiaMathsSerializers.h
// serialize() free functions for DiaMaths types (Vector2D, Matrix22, Matrix34,
// Matrix44, Quaternion).
//
// MODULE BOUNDARY NOTE:
//   DiaMaths must NOT depend on DiaReflect (DiaCore/Reflect/). This header
//   bridges both sides: it includes the DiaMaths type headers and the Reflect
//   macros in one place, without touching either module's own headers.
//
// USAGE:
//   Do NOT include this from Reflect.h — that would create a DiaMaths
//   dependency on DiaCore/Reflect. Instead, include it explicitly in any
//   translation unit that needs to serialize DiaMaths types:
//
//       #include "DiaCore/Reflect/DiaMathsSerializers.h"
//
//   Any project that includes this header must already have DiaMaths as a
//   project dependency (GoogleTests does; DiaCore itself does not).
//
// ADL NOTE:
//   All serialize() functions are placed in namespace Dia::Maths (same
//   namespace as the types), so Argument-Dependent Lookup finds them
//   automatically when an archive calls serialize(ar, value, version).
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Matrix/Matrix22.h"
#include "DiaMaths/Matrix/Matrix34.h"
#include "DiaMaths/Matrix/Matrix44.h"
#include "DiaMaths/Quaternion/Quaternion.h"

namespace Dia::Maths {

// -----------------------------------------------------------------------------
// Vector2D  — public float x, y
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Vector2D, 1)
    DIA_FIELD(x)
    DIA_FIELD(y)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Matrix22  — private float mElement[4], exposed via public operator[](int)
//   Layout (from Matrix22.cpp constructor):
//     [0] = e00,  [1] = e01,  [2] = e10,  [3] = e11
//   operator[](int) returns float& for non-const Matrix22, so named() binds
//   to the element by reference without needing private-member access.
//   DIA_FIELD_NAMED cannot be used here (it prepends "obj." to the member
//   expression), so we write the named() calls directly.
//   Field names match the DIA_TYPE_ADD_VARIABLE strings in Matrix22.cpp.
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Matrix22, 1)
    _ar_ & Dia::Reflect::named("e00", obj[0]);
    _ar_ & Dia::Reflect::named("e01", obj[1]);
    _ar_ & Dia::Reflect::named("e10", obj[2]);
    _ar_ & Dia::Reflect::named("e11", obj[3]);
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Matrix34  — public float m[3][4]  (row-major, row 3 implicit as (0,0,0,1))
//   Field names match the DIA_TYPE_ADD_VARIABLE strings in Matrix34.cpp.
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
// Matrix44  — public float m[4][4]  (row-major)
//   Field names match the DIA_TYPE_ADD_VARIABLE strings in Matrix44.cpp.
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
//   Hamilton convention: identity = (0, 0, 0, 1)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Quaternion, 1)
    DIA_FIELD(x)
    DIA_FIELD(y)
    DIA_FIELD(z)
    DIA_FIELD(w)
DIA_SERIALIZE_END

}  // namespace Dia::Maths
