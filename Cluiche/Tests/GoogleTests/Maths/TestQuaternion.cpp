// TestQuaternion.cpp - Google Test unit tests for Quaternion
//
// Tests the Dia::Maths::Quaternion class for 3D rotation

#include <gtest/gtest.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Matrix/Matrix33.h>
#include <DiaMaths/Core/Angle.h>
#include <cmath>

using namespace Dia::Maths;

// ==============================================================================
// Test Helpers
// ==============================================================================

// Quaternions q and -q represent the same rotation, so accept either form.
static void ExpectQuatNear(const Quaternion& a, const Quaternion& b, float eps = 1e-4f)
{
    bool direct  = std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps
                && std::abs(a.z - b.z) < eps && std::abs(a.w - b.w) < eps;
    bool negated = std::abs(a.x + b.x) < eps && std::abs(a.y + b.y) < eps
                && std::abs(a.z + b.z) < eps && std::abs(a.w + b.w) < eps;
    EXPECT_TRUE(direct || negated)
        << "Quaternions differ beyond eps=" << eps << "\n"
        << "  a=(" << a.x << "," << a.y << "," << a.z << "," << a.w << ")\n"
        << "  b=(" << b.x << "," << b.y << "," << b.z << "," << b.w << ")";
}

static void ExpectVec3Near(const Vector3D& a, const Vector3D& b, float eps = 1e-4f)
{
    EXPECT_NEAR(a.x, b.x, eps);
    EXPECT_NEAR(a.y, b.y, eps);
    EXPECT_NEAR(a.z, b.z, eps);
}

// ==============================================================================
// Storage Tests
// ==============================================================================

TEST(QuaternionStorageTest, DefaultConstruction_IsIdentity)
{
    Quaternion q;
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
}

TEST(QuaternionStorageTest, ComponentConstruction_StoresXYZW)
{
    Quaternion q(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(q.x, 1.0f);
    EXPECT_FLOAT_EQ(q.y, 2.0f);
    EXPECT_FLOAT_EQ(q.z, 3.0f);
    EXPECT_FLOAT_EQ(q.w, 4.0f);
}

TEST(QuaternionStorageTest, Identity_ReturnsIdentityQuaternion)
{
    Quaternion q = Quaternion::Identity();
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
}

// ==============================================================================
// Axis-Angle Tests
// ==============================================================================

TEST(QuaternionAxisAngleTest, FromAxisAngle_UnitMagnitude)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    EXPECT_NEAR(q.Magnitude(), 1.0f, 1e-4f);
}

TEST(QuaternionAxisAngleTest, FromAxisAngle_RoundTrip)
{
    // 90 degrees around Y — back to axis and angle
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    Vector3D outAxis;
    Angle outAngle;
    q.ToAxisAngle(outAxis, outAngle);

    EXPECT_NEAR(outAngle.AsRadians(), Angle::Deg90.AsRadians(), 1e-4f);
    ExpectVec3Near(outAxis, Vector3D::YAxis());
}

TEST(QuaternionAxisAngleTest, FromAxisAngle_AxisUnchangedByRotation)
{
    // A rotation about Y leaves the Y axis itself unchanged.
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    Vector3D rotated = q.Rotate(Vector3D::YAxis());
    ExpectVec3Near(rotated, Vector3D::YAxis());
}

// ==============================================================================
// Euler Angle Tests
// ==============================================================================

TEST(QuaternionEulerTest, FromEuler_Identity)
{
    Quaternion q = Quaternion::FromEuler(Angle::Deg0, Angle::Deg0, Angle::Deg0);
    ExpectQuatNear(q, Quaternion::Identity());
}

TEST(QuaternionEulerTest, FromEuler_YawEquivalentToAxisAngleAroundY)
{
    Quaternion fromEuler   = Quaternion::FromEuler(Angle::Deg90, Angle::Deg0, Angle::Deg0);
    Quaternion fromAxisAng = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    ExpectQuatNear(fromEuler, fromAxisAng);
}

TEST(QuaternionEulerTest, FromEuler_PitchEquivalentToAxisAngleAroundX)
{
    Quaternion fromEuler   = Quaternion::FromEuler(Angle::Deg0, Angle::Deg90, Angle::Deg0);
    Quaternion fromAxisAng = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg90);
    ExpectQuatNear(fromEuler, fromAxisAng);
}

TEST(QuaternionEulerTest, ToEuler_RoundTrip)
{
    Angle yaw   = Angle::Deg45;
    Angle pitch = Angle::Deg30;
    Angle roll  = Angle::Deg15;

    Quaternion q = Quaternion::FromEuler(yaw, pitch, roll);

    Angle outYaw, outPitch, outRoll;
    q.ToEuler(outYaw, outPitch, outRoll);

    EXPECT_NEAR(outYaw.AsRadians(),   yaw.AsRadians(),   1e-4f);
    EXPECT_NEAR(outPitch.AsRadians(), pitch.AsRadians(), 1e-4f);
    EXPECT_NEAR(outRoll.AsRadians(),  roll.AsRadians(),  1e-4f);
}

// ==============================================================================
// Matrix Conversion Tests
// ==============================================================================

TEST(QuaternionMatrixTest, Identity_ToMatrix33_IsIdentity)
{
    Matrix33 m = Quaternion::Identity().ToMatrix33();
    Matrix33 identity = Matrix33::Identity();

    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            EXPECT_NEAR(m.m[r][c], identity.m[r][c], 1e-4f);
}

TEST(QuaternionMatrixTest, FromMatrix33_IdentityIsIdentity)
{
    Quaternion q = Quaternion::FromMatrix33(Matrix33::Identity());
    ExpectQuatNear(q, Quaternion::Identity());
}

TEST(QuaternionMatrixTest, ToMatrix33_TransformDirectionMatchesRotate)
{
    // 90 degrees around Z: X maps to Y.
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::ZAxis(), Angle::Deg90);
    Matrix33 m = q.ToMatrix33();

    Vector3D xAxis(1.0f, 0.0f, 0.0f);
    Vector3D byQuat   = q.Rotate(xAxis);
    // Matrix33::TransformDirection operates in 2D homogeneous, so test via raw multiply.
    // Use the 3x3 matrix directly with the 3D vector.
    Vector3D byMatrix(
        m.m[0][0] * xAxis.x + m.m[0][1] * xAxis.y + m.m[0][2] * xAxis.z,
        m.m[1][0] * xAxis.x + m.m[1][1] * xAxis.y + m.m[1][2] * xAxis.z,
        m.m[2][0] * xAxis.x + m.m[2][1] * xAxis.y + m.m[2][2] * xAxis.z
    );

    ExpectVec3Near(byQuat, byMatrix);
}

// ==============================================================================
// Rotate Tests
// ==============================================================================

TEST(QuaternionRotateTest, YAxis90_RotatesXAxisToNegativeZ)
{
    // Right-hand rule: 90 deg about Y rotates +X toward -Z.
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    Vector3D result = q.Rotate(Vector3D::XAxis());
    ExpectVec3Near(result, Vector3D(0.0f, 0.0f, -1.0f));
}

TEST(QuaternionRotateTest, ZAxis180_RotatesXAxisToNegativeX)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::ZAxis(), Angle::Deg180);
    Vector3D result = q.Rotate(Vector3D::XAxis());
    ExpectVec3Near(result, Vector3D(-1.0f, 0.0f, 0.0f));
}

TEST(QuaternionRotateTest, Identity_LeavesVectorUnchanged)
{
    Vector3D v(1.0f, 2.0f, 3.0f);
    Vector3D result = Quaternion::Identity().Rotate(v);
    ExpectVec3Near(result, v);
}

// ==============================================================================
// Composition Tests
// ==============================================================================

TEST(QuaternionCompositionTest, Hamilton_ABRotateV_EqualsARotateBRotateV)
{
    Quaternion a = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    Quaternion b = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg45);
    Vector3D v(1.0f, 0.0f, 0.0f);

    Vector3D combined  = (a * b).Rotate(v);
    Vector3D sequential = a.Rotate(b.Rotate(v));

    ExpectVec3Near(combined, sequential);
}

TEST(QuaternionCompositionTest, IdentityTimesQ_EqualsQ)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::ZAxis(), Angle::Deg45);
    ExpectQuatNear(Quaternion::Identity() * q, q);
    ExpectQuatNear(q * Quaternion::Identity(), q);
}

TEST(QuaternionCompositionTest, QTimesInverse_IsIdentity)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg60).AsNormal();
    Quaternion result = q * q.Inverse();
    ExpectQuatNear(result, Quaternion::Identity());
}

// ==============================================================================
// Conjugate / Inverse Tests
// ==============================================================================

TEST(QuaternionConjugateInverseTest, UnitQuat_ConjugateApproxEqualsInverse)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    Quaternion conj = q.Conjugate();
    Quaternion inv  = q.Inverse();
    ExpectQuatNear(conj, inv);
}

TEST(QuaternionConjugateInverseTest, Conjugate_NegatatesXYZ_KeepsW)
{
    Quaternion q(0.5f, 0.5f, 0.5f, 0.5f);
    Quaternion c = q.Conjugate();
    EXPECT_FLOAT_EQ(c.x, -0.5f);
    EXPECT_FLOAT_EQ(c.y, -0.5f);
    EXPECT_FLOAT_EQ(c.z, -0.5f);
    EXPECT_FLOAT_EQ(c.w,  0.5f);
}

TEST(QuaternionConjugateInverseTest, Inverse_NonUnit)
{
    // For non-unit q, Inverse != Conjugate.
    Quaternion q(1.0f, 0.0f, 0.0f, 2.0f);
    Quaternion inv = q.Inverse();
    // q * q.Inverse() should be identity
    Quaternion product = q * inv;
    ExpectQuatNear(product, Quaternion::Identity(), 1e-4f);
}

// ==============================================================================
// Normalize Tests
// ==============================================================================

TEST(QuaternionNormalizeTest, Normalize_MagnitudeIsOne)
{
    Quaternion q(1.0f, 2.0f, 3.0f, 4.0f);
    q.Normalize();
    EXPECT_NEAR(q.Magnitude(), 1.0f, 1e-4f);
}

TEST(QuaternionNormalizeTest, AsNormal_ReturnsCopyWithMagnitudeOne)
{
    Quaternion q(1.0f, 2.0f, 3.0f, 4.0f);
    Quaternion n = q.AsNormal();
    EXPECT_NEAR(n.Magnitude(), 1.0f, 1e-4f);
    // Original not mutated
    EXPECT_FLOAT_EQ(q.x, 1.0f);
    EXPECT_FLOAT_EQ(q.w, 4.0f);
}

TEST(QuaternionNormalizeTest, AlreadyNormalized_StaysNormalized)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg45);
    EXPECT_NEAR(q.Magnitude(), 1.0f, 1e-4f);
    q.Normalize();
    EXPECT_NEAR(q.Magnitude(), 1.0f, 1e-4f);
}

// ==============================================================================
// IsValid Tests
// ==============================================================================

TEST(QuaternionIsValidTest, Identity_IsValid)
{
    EXPECT_TRUE(Quaternion::Identity().IsValid());
}

TEST(QuaternionIsValidTest, NaN_IsNotValid)
{
    Quaternion q;
    q.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(q.IsValid());
}

TEST(QuaternionIsValidTest, Infinity_IsNotValid)
{
    Quaternion q;
    q.w = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(q.IsValid());
}

TEST(QuaternionIsValidTest, ZeroQuaternion_IsNotValid)
{
    Quaternion q(0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(q.IsValid());
}

// ==============================================================================
// Slerp Tests
// ==============================================================================

TEST(QuaternionSlerpTest, SlerpAtT0_ReturnsA)
{
    Quaternion a = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg0);
    Quaternion b = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    ExpectQuatNear(Quaternion::Slerp(a, b, 0.0f), a);
}

TEST(QuaternionSlerpTest, SlerpAtT1_ReturnsB)
{
    Quaternion a = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg0);
    Quaternion b = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
    ExpectQuatNear(Quaternion::Slerp(a, b, 1.0f), b);
}

TEST(QuaternionSlerpTest, SlerpShortestPath_NegativeDot)
{
    // b is the negation of a — same rotation but opposite hemisphere.
    // Slerp should handle the shortest-path flip.
    Quaternion a = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg45);
    Quaternion b(-a.x, -a.y, -a.z, -a.w);  // negated

    // Slerp at 0.5 between a and -a should still reach a (they represent the same rotation).
    // The key check: result is a valid unit quaternion.
    Quaternion mid = Quaternion::Slerp(a, b, 0.5f);
    EXPECT_NEAR(mid.Magnitude(), 1.0f, 1e-4f);
}

TEST(QuaternionSlerpTest, SlerpResult_IsUnitQuaternion)
{
    Quaternion a = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg30);
    Quaternion b = Quaternion::FromAxisAngle(Vector3D::ZAxis(), Angle::Deg120);
    Quaternion mid = Quaternion::Slerp(a, b, 0.5f);
    EXPECT_NEAR(mid.Magnitude(), 1.0f, 1e-4f);
}

// ==============================================================================
// Nlerp Tests
// ==============================================================================

TEST(QuaternionNlerpTest, NlerpAtT0_ReturnsA)
{
    Quaternion a = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg0);
    Quaternion b = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg90);
    ExpectQuatNear(Quaternion::Nlerp(a, b, 0.0f), a);
}

TEST(QuaternionNlerpTest, NlerpAtT1_ReturnsB)
{
    Quaternion a = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg0);
    Quaternion b = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg90);
    ExpectQuatNear(Quaternion::Nlerp(a, b, 1.0f), b);
}

TEST(QuaternionNlerpTest, NlerpResult_IsUnitQuaternion)
{
    Quaternion a = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg30);
    Quaternion b = Quaternion::FromAxisAngle(Vector3D::ZAxis(), Angle::Deg60);
    Quaternion mid = Quaternion::Nlerp(a, b, 0.5f);
    EXPECT_NEAR(mid.Magnitude(), 1.0f, 1e-4f);
}

// ==============================================================================
// LookRotation Tests
// ==============================================================================

TEST(QuaternionLookRotationTest, LookAlongNegativeZ_WithYUp_IsIdentity)
{
    // Default camera convention: looking along -Z with +Y up is identity.
    Quaternion q = Quaternion::LookRotation(Vector3D(0.0f, 0.0f, -1.0f), Vector3D::YAxis());
    ExpectQuatNear(q, Quaternion::Identity());
}

TEST(QuaternionLookRotationTest, LookAlongNegativeX_RotatesForward)
{
    // Looking along -X should rotate the default forward (-Z) to point along -X.
    Quaternion q = Quaternion::LookRotation(Vector3D(-1.0f, 0.0f, 0.0f), Vector3D::YAxis());

    // The quaternion must be unit magnitude.
    EXPECT_NEAR(q.Magnitude(), 1.0f, 1e-4f);

    // Rotating the default forward (-Z) by this quaternion should point along -X.
    Vector3D forward(0.0f, 0.0f, -1.0f);
    Vector3D result = q.Rotate(forward);
    ExpectVec3Near(result, Vector3D(-1.0f, 0.0f, 0.0f));
}
