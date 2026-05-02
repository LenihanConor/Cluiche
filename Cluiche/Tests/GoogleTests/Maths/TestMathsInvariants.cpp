#include <gtest/gtest.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Matrix/Matrix22.h>
#include <DiaMaths/Core/Angle.h>
#include <cmath>
#include <random>

using namespace Dia::Maths;

static const int kSamples = 500;

static std::mt19937 MakeRng() { return std::mt19937(0xDEADBEEFu); }

static Vector2D RandVec(std::mt19937& rng, float range = 100.0f)
{
    std::uniform_real_distribution<float> dist(-range, range);
    return Vector2D(dist(rng), dist(rng));
}

static Vector2D RandNonZeroVec(std::mt19937& rng, float range = 100.0f)
{
    std::uniform_real_distribution<float> dist(0.1f, range);
    float sign1 = (rng() & 1) ? 1.0f : -1.0f;
    float sign2 = (rng() & 1) ? 1.0f : -1.0f;
    return Vector2D(dist(rng) * sign1, dist(rng) * sign2);
}

// ---------------------------------------------------------------------------
// Vector2D invariants
// ---------------------------------------------------------------------------

TEST(DiaMaths_Invariants, Vector2D_DotProduct_Commutative)
{
    auto rng = MakeRng();
    for (int i = 0; i < kSamples; ++i)
    {
        Vector2D a = RandVec(rng);
        Vector2D b = RandVec(rng);
        EXPECT_NEAR(a.Dot(b), b.Dot(a), 1e-3f) << "Dot not commutative at sample " << i;
    }
}

TEST(DiaMaths_Invariants, Vector2D_NormalizeMagnitude_IsOne)
{
    auto rng = MakeRng();
    for (int i = 0; i < kSamples; ++i)
    {
        Vector2D v = RandNonZeroVec(rng);
        v.Normalize();
        float mag = v.Magnitude();
        EXPECT_NEAR(mag, 1.0f, 1e-5f) << "Magnitude != 1 at sample " << i;
    }
}

TEST(DiaMaths_Invariants, Vector2D_Addition_Associative)
{
    auto rng = MakeRng();
    for (int i = 0; i < kSamples; ++i)
    {
        Vector2D a = RandVec(rng);
        Vector2D b = RandVec(rng);
        Vector2D c = RandVec(rng);
        Vector2D lhs = (a + b) + c;
        Vector2D rhs = a + (b + c);
        EXPECT_NEAR(lhs.x, rhs.x, 1e-3f) << "Addition not associative (x) at sample " << i;
        EXPECT_NEAR(lhs.y, rhs.y, 1e-3f) << "Addition not associative (y) at sample " << i;
    }
}

TEST(DiaMaths_Invariants, Vector2D_ScalarMultiply_Distributive)
{
    auto rng = MakeRng();
    std::uniform_real_distribution<float> scalar(-10.0f, 10.0f);
    for (int i = 0; i < kSamples; ++i)
    {
        float  s = scalar(rng);
        Vector2D a = RandVec(rng, 10.0f);
        Vector2D b = RandVec(rng, 10.0f);
        Vector2D lhs = (a + b) * s;
        Vector2D rhs = a * s + b * s;
        EXPECT_NEAR(lhs.x, rhs.x, 1e-2f) << "Scalar distribution failed (x) at sample " << i;
        EXPECT_NEAR(lhs.y, rhs.y, 1e-2f) << "Scalar distribution failed (y) at sample " << i;
    }
}

// ---------------------------------------------------------------------------
// Matrix22 invariants
// ---------------------------------------------------------------------------

TEST(DiaMaths_Invariants, Matrix22_MultiplyByInverse_IsIdentity)
{
    // Identity matrix times itself should still be identity
    Matrix22 id;
    id.Set(1.0f, 0.0f, 0.0f, 1.0f);
    Matrix22 idInv = id.AsInverse();
    Matrix22 product = id * idInv;

    EXPECT_NEAR(product.Element(0), 1.0f, 1e-4f);
    EXPECT_NEAR(product.Element(1), 0.0f, 1e-4f);
    EXPECT_NEAR(product.Element(2), 0.0f, 1e-4f);
    EXPECT_NEAR(product.Element(3), 1.0f, 1e-4f);
}

TEST(DiaMaths_Invariants, Matrix22_RotationMatrix_MultiplyByInverse_IsIdentity)
{
    // Rotation matrix is orthogonal: M * M^T = I, and M^-1 = M^T
    for (int i = 0; i < 16; ++i)
    {
        float angle = static_cast<float>(i) * (3.14159265f / 8.0f);
        Matrix22 rot = Matrix22::FromAngleClockwise(Angle(angle));
        Matrix22 inv = rot.AsInverseOrthogonal();
        Matrix22 product = rot * inv;

        EXPECT_NEAR(product.Element(0), 1.0f, 1e-4f) << "Rotation " << i;
        EXPECT_NEAR(product.Element(1), 0.0f, 1e-4f) << "Rotation " << i;
        EXPECT_NEAR(product.Element(2), 0.0f, 1e-4f) << "Rotation " << i;
        EXPECT_NEAR(product.Element(3), 1.0f, 1e-4f) << "Rotation " << i;
    }
}

TEST(DiaMaths_Invariants, Matrix22_DeterminantOfProduct_IsProductOfDeterminants)
{
    // det(AB) == det(A) * det(B)
    Matrix22 a(2.0f, 1.0f, 0.0f, 3.0f);
    Matrix22 b(1.0f, 2.0f, 3.0f, 1.0f);
    Matrix22 ab = a * b;

    float detA  = a.Determinant();
    float detB  = b.Determinant();
    float detAB = ab.Determinant();

    EXPECT_NEAR(detAB, detA * detB, 1e-4f);
}

TEST(DiaMaths_Invariants, Matrix22_Transpose_InvolutoryProperty)
{
    // (M^T)^T == M
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 mTT = m.AsTranspose().AsTranspose();

    EXPECT_NEAR(mTT.Element(0), m.Element(0), 1e-5f);
    EXPECT_NEAR(mTT.Element(1), m.Element(1), 1e-5f);
    EXPECT_NEAR(mTT.Element(2), m.Element(2), 1e-5f);
    EXPECT_NEAR(mTT.Element(3), m.Element(3), 1e-5f);
}

// ---------------------------------------------------------------------------
// Angle invariants
// ---------------------------------------------------------------------------

TEST(DiaMaths_Invariants, Angle_Normalize_AlwaysInRange)
{
    // Angle::Normalize() maps to [-PI, PI]
    auto rng = MakeRng();
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
    const float pi = 3.14159265f;

    for (int i = 0; i < kSamples; ++i)
    {
        Angle a(dist(rng));
        a.Normalize();
        float rad = a.AsRadians();
        EXPECT_GE(rad, -pi - 1e-4f) << "Angle < -PI after Normalize at sample " << i;
        EXPECT_LE(rad,  pi + 1e-4f) << "Angle > PI after Normalize at sample " << i;
    }
}

TEST(DiaMaths_Invariants, Angle_AddSubtract_Identity)
{
    Angle base(1.0f);
    Angle delta(0.5f);
    Angle result = (base + delta) - delta;
    EXPECT_NEAR(result.AsRadians(), base.AsRadians(), 1e-5f);
}
