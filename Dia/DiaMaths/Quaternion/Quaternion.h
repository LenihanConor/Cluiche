#pragma once
#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia { namespace Maths {

class Vector3D;
class Matrix33;
class Matrix44;
class Angle;

//==============================================================================
// CLASS Quaternion
//==============================================================================
// Unit quaternion for 3D rotation in the Dia maths library.
//
// Hamilton convention: q = xi + yj + zk + w
// Identity: (0, 0, 0, 1)
//
// COMPOSITION: operator* applies rotations left-to-right (Hamilton product).
//   (a * b).Rotate(v) == a.Rotate(b.Rotate(v))
//
// EULER CONVENTION: YXZ intrinsic (yaw around Y, pitch around X, roll around Z)
//==============================================================================
class Quaternion
{
public:
    DIA_TYPE_DECLARATION;

    Quaternion();                                        // identity (0,0,0,1)
    Quaternion(float x, float y, float z, float w);
    Quaternion(const Quaternion& other);

    static Quaternion Identity();
    static Quaternion FromAxisAngle(const Vector3D& axis, const Angle& angle);
    static Quaternion FromEuler(const Angle& yaw, const Angle& pitch, const Angle& roll);  // YXZ intrinsic
    static Quaternion FromMatrix33(const Matrix33& rotation);
    static Quaternion FromMatrix44(const Matrix44& transform);
    static Quaternion LookRotation(const Vector3D& forward, const Vector3D& up);

    Quaternion& operator=(const Quaternion& other);
    Quaternion  operator*(const Quaternion& rhs) const;  // Hamilton product
    Quaternion& operator*=(const Quaternion& rhs);
    bool operator==(const Quaternion& rhs) const;
    bool operator!=(const Quaternion& rhs) const;

    Quaternion  Conjugate() const;        // (-x,-y,-z, w)
    Quaternion  Inverse()   const;        // Conjugate / SquareMagnitude
    Quaternion& Normalize();              // mutates
    Quaternion  AsNormal()  const;        // returns copy
    float       Dot(const Quaternion& rhs) const;
    float       Magnitude()       const;
    float       SquareMagnitude() const;
    bool        IsValid() const;          // not NaN/Inf, magnitude > epsilon

    // Rotate a vector by this quaternion.
    // Optimised form: t = 2*cross(q.xyz, v); result = v + q.w*t + cross(q.xyz, t)
    Vector3D Rotate(const Vector3D& v) const;

    Matrix33   ToMatrix33() const;
    Matrix44   ToMatrix44() const;
    void       ToAxisAngle(Vector3D& outAxis, Angle& outAngle) const;
    void       ToEuler(Angle& outYaw, Angle& outPitch, Angle& outRoll) const;  // YXZ intrinsic

    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);  // shortest-path
    static Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t);

    float x, y, z, w;
};

} }

#include "DiaMaths/Quaternion/Quaternion.inl"
