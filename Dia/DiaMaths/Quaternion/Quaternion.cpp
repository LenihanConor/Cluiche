#include "DiaMaths/Quaternion/Quaternion.h"
#include "DiaMaths/Vector/Vector3D.h"
#include "DiaMaths/Matrix/Matrix33.h"
#include "DiaMaths/Matrix/Matrix44.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaMaths/Core/Trigonometry.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"
#include <math.h>

namespace Dia { namespace Maths {

    DIA_TYPE_DEFINITION(Quaternion)
        DIA_TYPE_ADD_VARIABLE("x", x)
        DIA_TYPE_ADD_VARIABLE("y", y)
        DIA_TYPE_ADD_VARIABLE("z", z)
        DIA_TYPE_ADD_VARIABLE("w", w)
    DIA_TYPE_DEFINITION_END()

    //-----------------------------------------------------------------------------
    float Quaternion::Magnitude() const
    {
        return Dia::Maths::SquareRoot(SquareMagnitude());
    }

    //-----------------------------------------------------------------------------
    Quaternion& Quaternion::Normalize()
    {
        float mag = Magnitude();
        if (mag > 1e-6f)
        {
            float inv = 1.0f / mag;
            x *= inv;
            y *= inv;
            z *= inv;
            w *= inv;
        }
        return *this;
    }

    //-----------------------------------------------------------------------------
    bool Quaternion::IsValid() const
    {
        if (_isnan(x) || _isnan(y) || _isnan(z) || _isnan(w)) return false;
        if (!_finite(x) || !_finite(y) || !_finite(z) || !_finite(w)) return false;
        return SquareMagnitude() > 1e-10f;
    }

    //-----------------------------------------------------------------------------
    Quaternion Quaternion::Inverse() const
    {
        float sq = SquareMagnitude();
        if (sq < 1e-10f) return Identity();
        float invSq = 1.0f / sq;
        return Quaternion(-x * invSq, -y * invSq, -z * invSq, w * invSq);
    }

    //-----------------------------------------------------------------------------
    Quaternion Quaternion::operator*(const Quaternion& rhs) const
    {
        return Quaternion(
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
        );
    }

    //-----------------------------------------------------------------------------
    Quaternion& Quaternion::operator*=(const Quaternion& rhs)
    {
        *this = *this * rhs;
        return *this;
    }

    //-----------------------------------------------------------------------------
    // Optimised rotation: t = 2*cross(q.xyz, v); result = v + q.w*t + cross(q.xyz, t)
    Vector3D Quaternion::Rotate(const Vector3D& v) const
    {
        Vector3D qv(x, y, z);
        Vector3D t = qv.Cross(v) * 2.0f;
        return v + (t * w) + qv.Cross(t);
    }

    //-----------------------------------------------------------------------------
    Quaternion Quaternion::FromAxisAngle(const Vector3D& axis, const Angle& angle)
    {
        float halfRad = angle.AsRadians() * 0.5f;
        float s = Dia::Maths::Sin(halfRad);
        Vector3D n = axis.AsNormalSafe();
        return Quaternion(n.x * s, n.y * s, n.z * s, Dia::Maths::Cos(halfRad));
    }

    //-----------------------------------------------------------------------------
    // YXZ intrinsic: yaw around Y, pitch around X, roll around Z
    // q = qY * qX * qZ
    Quaternion Quaternion::FromEuler(const Angle& yaw, const Angle& pitch, const Angle& roll)
    {
        float hy = yaw.AsRadians()   * 0.5f;
        float hp = pitch.AsRadians() * 0.5f;
        float hr = roll.AsRadians()  * 0.5f;

        float cy = Dia::Maths::Cos(hy), sy = Dia::Maths::Sin(hy);
        float cp = Dia::Maths::Cos(hp), sp = Dia::Maths::Sin(hp);
        float cr = Dia::Maths::Cos(hr), sr = Dia::Maths::Sin(hr);

        return Quaternion(
             cy * sp * cr + sy * cp * sr,
             sy * cp * cr - cy * sp * sr,
             cy * cp * sr - sy * sp * cr,
             cy * cp * cr + sy * sp * sr
        );
    }

    //-----------------------------------------------------------------------------
    Matrix33 Quaternion::ToMatrix33() const
    {
        float x2 = x * x, y2 = y * y, z2 = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;

        return Matrix33(
            1.0f - 2.0f * (y2 + z2),  2.0f * (xy - wz),           2.0f * (xz + wy),
            2.0f * (xy + wz),          1.0f - 2.0f * (x2 + z2),    2.0f * (yz - wx),
            2.0f * (xz - wy),          2.0f * (yz + wx),            1.0f - 2.0f * (x2 + y2)
        );
    }

    //-----------------------------------------------------------------------------
    // ToMatrix44: pure rotation; translation is zero; last row is (0,0,0,1)
    Matrix44 Quaternion::ToMatrix44() const
    {
        float x2 = x * x, y2 = y * y, z2 = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;

        return Matrix44(
            1.0f - 2.0f * (y2 + z2),  2.0f * (xy - wz),           2.0f * (xz + wy),           0.0f,
            2.0f * (xy + wz),          1.0f - 2.0f * (x2 + z2),    2.0f * (yz - wx),           0.0f,
            2.0f * (xz - wy),          2.0f * (yz + wx),            1.0f - 2.0f * (x2 + y2),   0.0f,
            0.0f,                      0.0f,                        0.0f,                       1.0f
        );
    }

    //-----------------------------------------------------------------------------
    // Shepperd's method
    Quaternion Quaternion::FromMatrix33(const Matrix33& m)
    {
        float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];

        if (trace > 0.0f)
        {
            float s = 0.5f / Dia::Maths::SquareRoot(trace + 1.0f);
            return Quaternion(
                (m.m[2][1] - m.m[1][2]) * s,
                (m.m[0][2] - m.m[2][0]) * s,
                (m.m[1][0] - m.m[0][1]) * s,
                0.25f / s
            );
        }
        else if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2])
        {
            float s = 2.0f * Dia::Maths::SquareRoot(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]);
            return Quaternion(
                0.25f * s,
                (m.m[0][1] + m.m[1][0]) / s,
                (m.m[0][2] + m.m[2][0]) / s,
                (m.m[2][1] - m.m[1][2]) / s
            );
        }
        else if (m.m[1][1] > m.m[2][2])
        {
            float s = 2.0f * Dia::Maths::SquareRoot(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]);
            return Quaternion(
                (m.m[0][1] + m.m[1][0]) / s,
                0.25f * s,
                (m.m[1][2] + m.m[2][1]) / s,
                (m.m[0][2] - m.m[2][0]) / s
            );
        }
        else
        {
            float s = 2.0f * Dia::Maths::SquareRoot(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]);
            return Quaternion(
                (m.m[0][2] + m.m[2][0]) / s,
                (m.m[1][2] + m.m[2][1]) / s,
                0.25f * s,
                (m.m[1][0] - m.m[0][1]) / s
            );
        }
    }

    //-----------------------------------------------------------------------------
    // FromMatrix44: extract upper-left 3×3 and delegate to FromMatrix33
    Quaternion Quaternion::FromMatrix44(const Matrix44& m)
    {
        // Extract upper-left 3×3 rotation part
        Matrix33 rotation(
            m.m[0][0], m.m[0][1], m.m[0][2],
            m.m[1][0], m.m[1][1], m.m[1][2],
            m.m[2][0], m.m[2][1], m.m[2][2]
        );

        return FromMatrix33(rotation);
    }

    //-----------------------------------------------------------------------------
    void Quaternion::ToAxisAngle(Vector3D& outAxis, Angle& outAngle) const
    {
        Quaternion q = AsNormal();
        float clampedW = Dia::Maths::Clamp(q.w, -1.0f, 1.0f);
        float halfAngle = Dia::Maths::ACos(clampedW);
        outAngle = Angle(halfAngle * 2.0f);

        float s = Dia::Maths::Sin(halfAngle);
        if (s > 1e-6f)
        {
            outAxis = Vector3D(q.x / s, q.y / s, q.z / s);
        }
        else
        {
            outAxis = Vector3D::XAxis();
        }
    }

    //-----------------------------------------------------------------------------
    // YXZ intrinsic: extract yaw(Y), pitch(X), roll(Z)
    void Quaternion::ToEuler(Angle& outYaw, Angle& outPitch, Angle& outRoll) const
    {
        float sinPitch = 2.0f * (w * x - y * z);
        sinPitch = Dia::Maths::Clamp(sinPitch, -1.0f, 1.0f);
        outPitch = Angle(Dia::Maths::ASin(sinPitch));

        float cosP = Dia::Maths::Cos(outPitch.AsRadians());
        if (cosP > 1e-6f)
        {
            outYaw  = Angle(Dia::Maths::ATan2(2.0f * (w * y + x * z),  1.0f - 2.0f * (x * x + y * y)));
            outRoll = Angle(Dia::Maths::ATan2(2.0f * (w * z + x * y),  1.0f - 2.0f * (x * x + z * z)));
        }
        else
        {
            outYaw  = Angle(Dia::Maths::ATan2(-2.0f * (y * z - w * x), 1.0f - 2.0f * (x * x + z * z)));
            outRoll = Angle(0.0f);
        }
    }

    //-----------------------------------------------------------------------------
    // LookRotation: Y-up right-handed, -Z forward convention
    Quaternion Quaternion::LookRotation(const Vector3D& forward, const Vector3D& up)
    {
        Vector3D f = forward.AsNormalSafe();
        Vector3D r = f.Cross(up).AsNormalSafe();   // right = forward × up (right-handed, -Z forward)

        DIA_ASSERT(r.SquareMagnitude() > 1e-6f, "LookRotation: forward and up are parallel");
        if (r.SquareMagnitude() < 1e-6f) return Identity();

        Vector3D u = r.Cross(f);   // corrected up = right × forward

        // Build rotation matrix with r/u/-f as COLUMNS (local axes in world space).
        // Row-major storage — columns become the transposed arrangement.
        Matrix33 mat(
            r.x, u.x, -f.x,
            r.y, u.y, -f.y,
            r.z, u.z, -f.z
        );

        return FromMatrix33(mat);
    }

    //-----------------------------------------------------------------------------
    // Slerp with shortest-path guarantee
    Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t)
    {
        Quaternion bAdj = b;
        float d = a.Dot(b);
        if (d < 0.0f)
        {
            bAdj = Quaternion(-b.x, -b.y, -b.z, -b.w);
            d = -d;
        }

        // Near-identical quaternions: fall back to nlerp to avoid division by ~0
        if (d > 0.9995f) return Nlerp(a, bAdj, t);

        float theta0 = Dia::Maths::ACos(d);
        float theta  = theta0 * t;
        float sinTheta  = Dia::Maths::Sin(theta);
        float sinTheta0 = Dia::Maths::Sin(theta0);

        float s0 = Dia::Maths::Cos(theta) - d * sinTheta / sinTheta0;
        float s1 = sinTheta / sinTheta0;

        return Quaternion(
            s0 * a.x + s1 * bAdj.x,
            s0 * a.y + s1 * bAdj.y,
            s0 * a.z + s1 * bAdj.z,
            s0 * a.w + s1 * bAdj.w
        ).AsNormal();
    }

    //-----------------------------------------------------------------------------
    Quaternion Quaternion::Nlerp(const Quaternion& a, const Quaternion& b, float t)
    {
        Quaternion bAdj = b;
        if (a.Dot(b) < 0.0f)
        {
            bAdj = Quaternion(-b.x, -b.y, -b.z, -b.w);
        }
        float s = 1.0f - t;
        return Quaternion(
            s * a.x + t * bAdj.x,
            s * a.y + t * bAdj.y,
            s * a.z + t * bAdj.z,
            s * a.w + t * bAdj.w
        ).AsNormal();
    }

} }
