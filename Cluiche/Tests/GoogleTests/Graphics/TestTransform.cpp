// TestTransform.cpp - Google Test unit tests for Transform
//
// Tests Transform 2D transformation matrix from DiaGraphics

#include <gtest/gtest.h>
#include <DiaGraphics/Misc/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <cmath>

using namespace Dia::Graphics;
using namespace Dia::Maths;

// Helper function for float comparison
const float EPSILON = 0.0001f;
bool NearlyEqual(float a, float b)
{
    return std::abs(a - b) < EPSILON;
}

// ==============================================================================
// Transform Construction Tests
// ==============================================================================

TEST(Transform, DefaultConstructor_CreatesIdentity)
{
    Transform t;
    Vector2D point(10.0f, 20.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 10.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 20.0f));
}

TEST(Transform, Identity_IsIdentityTransform)
{
    Transform t = Transform::Identity;
    Vector2D point(5.0f, 15.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 5.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 15.0f));
}

TEST(Transform, MatrixConstructor_CreatesCorrectTransform)
{
    // Create a translation matrix: move by (10, 20)
    Transform t(1, 0, 10,
                0, 1, 20,
                0, 0, 1);

    Vector2D point(0.0f, 0.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 10.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 20.0f));
}

// ==============================================================================
// Transform Translation Tests
// ==============================================================================

TEST(Transform, Translate_MovesPoint)
{
    Transform t;
    t.Translate(10.0f, 20.0f);

    Vector2D point(5.0f, 5.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 15.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 25.0f));
}

TEST(Transform, TranslateVector_MovesPoint)
{
    Transform t;
    t.Translate(Vector2D(10.0f, 20.0f));

    Vector2D point(5.0f, 5.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 15.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 25.0f));
}

TEST(Transform, MultipleTranslations_Accumulate)
{
    Transform t;
    t.Translate(10.0f, 10.0f);
    t.Translate(5.0f, 5.0f);

    Vector2D point(0.0f, 0.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 15.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 15.0f));
}

// ==============================================================================
// Transform Rotation Tests
// ==============================================================================

TEST(Transform, Rotate90Degrees_RotatesCorrectly)
{
    Transform t;
    t.Rotate(90.0f);

    Vector2D point(1.0f, 0.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 0.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 1.0f));
}

TEST(Transform, Rotate180Degrees_RotatesCorrectly)
{
    Transform t;
    t.Rotate(180.0f);

    Vector2D point(1.0f, 0.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, -1.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 0.0f));
}

TEST(Transform, RotateAroundCenter_RotatesCorrectly)
{
    Transform t;
    t.Rotate(90.0f, 10.0f, 10.0f);

    Vector2D point(11.0f, 10.0f); // 1 unit right of center
    Vector2D result = t.TransformPoint(point);

    // After 90° rotation around (10,10), point should be at (10, 11)
    EXPECT_TRUE(NearlyEqual(result.x, 10.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 11.0f));
}

TEST(Transform, RotateAroundCenterVector_RotatesCorrectly)
{
    Transform t;
    t.Rotate(90.0f, Vector2D(10.0f, 10.0f));

    Vector2D point(11.0f, 10.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 10.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 11.0f));
}

// ==============================================================================
// Transform Scale Tests
// ==============================================================================

TEST(Transform, Scale_ScalesPoint)
{
    Transform t;
    t.Scale(2.0f, 3.0f);

    Vector2D point(10.0f, 10.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 20.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 30.0f));
}

TEST(Transform, ScaleVector_ScalesPoint)
{
    Transform t;
    t.Scale(Vector2D(2.0f, 3.0f));

    Vector2D point(10.0f, 10.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 20.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 30.0f));
}

TEST(Transform, ScaleAroundCenter_ScalesCorrectly)
{
    Transform t;
    t.Scale(2.0f, 2.0f, 10.0f, 10.0f);

    Vector2D point(15.0f, 15.0f); // 5 units from center
    Vector2D result = t.TransformPoint(point);

    // After 2x scale around (10,10), point should be at (20, 20)
    EXPECT_TRUE(NearlyEqual(result.x, 20.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 20.0f));
}

TEST(Transform, ScaleAroundCenterVector_ScalesCorrectly)
{
    Transform t;
    t.Scale(Vector2D(2.0f, 2.0f), Vector2D(10.0f, 10.0f));

    Vector2D point(15.0f, 15.0f);
    Vector2D result = t.TransformPoint(point);

    EXPECT_TRUE(NearlyEqual(result.x, 20.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 20.0f));
}

// ==============================================================================
// Transform Combination Tests
// ==============================================================================

TEST(Transform, TranslateAndRotate_AppliesInOrder)
{
    Transform t;
    t.Translate(10.0f, 0.0f);
    t.Rotate(90.0f);

    Vector2D point(0.0f, 0.0f);
    Vector2D result = t.TransformPoint(point);

    // In graphics transformations, operations apply in reverse order:
    // Rotate first: (0,0) rotates 90° → (0,0) (no change at origin)
    // Then translate: (0,0) → (10,0)
    EXPECT_TRUE(NearlyEqual(result.x, 10.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 0.0f));
}

TEST(Transform, Combine_CombinesTwoTransforms)
{
    Transform t1;
    t1.Translate(10.0f, 0.0f);

    Transform t2;
    t2.Rotate(90.0f);

    t1.Combine(t2);

    Vector2D point(0.0f, 0.0f);
    Vector2D result = t1.TransformPoint(point);

    // Combine behaves like chained operations: transformations apply in reverse order
    // Rotate first: (0,0) → (0,0), then translate: (0,0) → (10,0)
    EXPECT_TRUE(NearlyEqual(result.x, 10.0f));
    EXPECT_TRUE(NearlyEqual(result.y, 0.0f));
}

TEST(Transform, MultiplicationOperator_CombinesTransforms)
{
    Transform t1;
    t1.Translate(10.0f, 0.0f);

    Transform t2;
    t2.Rotate(90.0f);

    Transform result = t1 * t2;

    Vector2D point(0.0f, 0.0f);
    Vector2D transformed = result.TransformPoint(point);

    // t1 * t2 behaves like chained operations: transformations apply in reverse order
    // Rotate first: (0,0) → (0,0), then translate: (0,0) → (10,0)
    EXPECT_TRUE(NearlyEqual(transformed.x, 10.0f));
    EXPECT_TRUE(NearlyEqual(transformed.y, 0.0f));
}

// ==============================================================================
// Transform Inverse Tests
// ==============================================================================

TEST(Transform, GetInverse_InvertsTranslation)
{
    Transform t;
    t.Translate(10.0f, 20.0f);

    Transform inv = t.GetInverse();

    Vector2D point(0.0f, 0.0f);
    Vector2D forward = t.TransformPoint(point);
    Vector2D back = inv.TransformPoint(forward);

    EXPECT_TRUE(NearlyEqual(back.x, 0.0f));
    EXPECT_TRUE(NearlyEqual(back.y, 0.0f));
}

TEST(Transform, GetInverse_InvertsRotation)
{
    Transform t;
    t.Rotate(45.0f);

    Transform inv = t.GetInverse();

    Vector2D point(10.0f, 0.0f);
    Vector2D forward = t.TransformPoint(point);
    Vector2D back = inv.TransformPoint(forward);

    EXPECT_TRUE(NearlyEqual(back.x, 10.0f));
    EXPECT_TRUE(NearlyEqual(back.y, 0.0f));
}

// ==============================================================================
// Transform Matrix Access Tests
// ==============================================================================

TEST(Transform, GetMatrix_ReturnsValidPointer)
{
    Transform t;
    const float* matrix = t.GetMatrix();

    EXPECT_NE(matrix, nullptr);
}

TEST(Transform, GetMatrix_IdentityHasCorrectValues)
{
    Transform t = Transform::Identity;
    const float* matrix = t.GetMatrix();

    // OpenGL column-major 4x4 matrix for identity
    EXPECT_TRUE(NearlyEqual(matrix[0], 1.0f));  // m00
    EXPECT_TRUE(NearlyEqual(matrix[5], 1.0f));  // m11
    EXPECT_TRUE(NearlyEqual(matrix[10], 1.0f)); // m22
    EXPECT_TRUE(NearlyEqual(matrix[15], 1.0f)); // m33
}
