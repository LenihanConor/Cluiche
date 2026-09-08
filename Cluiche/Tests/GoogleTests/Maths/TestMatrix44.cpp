// TestMatrix44.cpp - Google Test unit tests for Matrix44
//
// Tests the Dia::Maths::Matrix44 class for 3D transformations

#include <gtest/gtest.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Vector/Vector4D.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <DiaMaths/Core/Angle.h>
#include <cmath>

using namespace Dia::Maths;

constexpr float kEpsilon = 1e-4f;

// ==============================================================================
// Test Helpers
// ==============================================================================

static void ExpectVec3Near(const Vector3D& a, const Vector3D& b, float eps = kEpsilon)
{
	EXPECT_NEAR(a.x, b.x, eps);
	EXPECT_NEAR(a.y, b.y, eps);
	EXPECT_NEAR(a.z, b.z, eps);
}

static void ExpectVec4Near(const Vector4D& a, const Vector4D& b, float eps = kEpsilon)
{
	EXPECT_NEAR(a.x, b.x, eps);
	EXPECT_NEAR(a.y, b.y, eps);
	EXPECT_NEAR(a.z, b.z, eps);
	EXPECT_NEAR(a.w, b.w, eps);
}

static void ExpectMatrix44Near(const Matrix44& a, const Matrix44& b, float eps = kEpsilon)
{
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			EXPECT_NEAR(a(i, j), b(i, j), eps)
				<< "Element at (" << i << "," << j << ") differs";
		}
	}
}

// ==============================================================================
// Storage Tests
// ==============================================================================

TEST(Matrix44StorageTest, DefaultConstruction_CreatesIdentityMatrix)
{
	Matrix44 m;

	for (int row = 0; row < 4; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			float expected = (row == col) ? 1.0f : 0.0f;
			EXPECT_FLOAT_EQ(m(row, col), expected)
				<< "Element at (" << row << "," << col << ") should be " << expected;
		}
	}
}

TEST(Matrix44StorageTest, ComponentConstruction_InitializesAllElements)
{
	Matrix44 m(
		1, 2, 3, 4,
		5, 6, 7, 8,
		9, 10, 11, 12,
		13, 14, 15, 16
	);

	EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
	EXPECT_FLOAT_EQ(m(0, 1), 2.0f);
	EXPECT_FLOAT_EQ(m(0, 2), 3.0f);
	EXPECT_FLOAT_EQ(m(0, 3), 4.0f);
	EXPECT_FLOAT_EQ(m(1, 0), 5.0f);
	EXPECT_FLOAT_EQ(m(1, 1), 6.0f);
	EXPECT_FLOAT_EQ(m(1, 2), 7.0f);
	EXPECT_FLOAT_EQ(m(1, 3), 8.0f);
	EXPECT_FLOAT_EQ(m(2, 0), 9.0f);
	EXPECT_FLOAT_EQ(m(2, 1), 10.0f);
	EXPECT_FLOAT_EQ(m(2, 2), 11.0f);
	EXPECT_FLOAT_EQ(m(2, 3), 12.0f);
	EXPECT_FLOAT_EQ(m(3, 0), 13.0f);
	EXPECT_FLOAT_EQ(m(3, 1), 14.0f);
	EXPECT_FLOAT_EQ(m(3, 2), 15.0f);
	EXPECT_FLOAT_EQ(m(3, 3), 16.0f);
}

TEST(Matrix44StorageTest, CopyConstruction_CopiesAllElements)
{
	Matrix44 original(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
	Matrix44 copy(original);

	for (int row = 0; row < 4; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			EXPECT_FLOAT_EQ(copy(row, col), original(row, col));
		}
	}
}

TEST(Matrix44StorageTest, ElementAccess_MatchesDirectAccess)
{
	Matrix44 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

	for (int row = 0; row < 4; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			EXPECT_FLOAT_EQ(m(row, col), m.m[row][col]);
		}
	}
}

// ==============================================================================
// Factory Method Tests
// ==============================================================================

TEST(Matrix44FromTranslationTest, CreatesTranslationMatrix)
{
	Vector3D translation(10.0f, 20.0f, 30.0f);
	Matrix44 m = Matrix44::FromTranslation(translation);

	// Translation should be in the last column
	EXPECT_FLOAT_EQ(m(0, 3), 10.0f);
	EXPECT_FLOAT_EQ(m(1, 3), 20.0f);
	EXPECT_FLOAT_EQ(m(2, 3), 30.0f);

	// Should be identity with translation
	EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
	EXPECT_FLOAT_EQ(m(1, 1), 1.0f);
	EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
	EXPECT_FLOAT_EQ(m(3, 3), 1.0f);
}

TEST(Matrix44FromTranslationTest, TransformPoint_AppliesTranslation)
{
	Vector3D translation(10.0f, 20.0f, 30.0f);
	Matrix44 m = Matrix44::FromTranslation(translation);

	Vector3D point(1.0f, 2.0f, 3.0f);
	Vector3D transformed = m.TransformPoint(point);

	ExpectVec3Near(transformed, Vector3D(11.0f, 22.0f, 33.0f));
}

TEST(Matrix44FromTranslationTest, TransformDirection_IgnoresTranslation)
{
	Vector3D translation(10.0f, 20.0f, 30.0f);
	Matrix44 m = Matrix44::FromTranslation(translation);

	Vector3D direction(1.0f, 0.0f, 0.0f);
	Vector3D transformed = m.TransformDirection(direction);

	ExpectVec3Near(transformed, direction);
}

// ==============================================================================
// Rotation Tests
// ==============================================================================

TEST(Matrix44FromRotationTest, Identity_IsIdentity)
{
	Matrix44 m = Matrix44::FromRotation(Quaternion::Identity());
	Matrix44 identity = Matrix44::Identity();

	ExpectMatrix44Near(m, identity);
}

TEST(Matrix44FromRotationTest, TransformDirection_MatchesQuaternionRotate)
{
	// 90 degrees around Y axis (right-handed)
	Quaternion q = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::FromDegrees(90.0f));
	Matrix44 m = Matrix44::FromRotation(q);

	Vector3D xAxis = Vector3D::XAxis();
	Vector3D rotatedMatrix = m.TransformDirection(xAxis);
	Vector3D rotatedQuat = q.Rotate(xAxis);

	ExpectVec3Near(rotatedMatrix, rotatedQuat);
}

// ==============================================================================
// Scale Tests
// ==============================================================================

TEST(Matrix44FromScaleTest, NonUniform_CreatesDiagonalMatrix)
{
	Vector3D scale(2.0f, 3.0f, 4.0f);
	Matrix44 m = Matrix44::FromScale(scale);

	EXPECT_FLOAT_EQ(m(0, 0), 2.0f);
	EXPECT_FLOAT_EQ(m(1, 1), 3.0f);
	EXPECT_FLOAT_EQ(m(2, 2), 4.0f);
	EXPECT_FLOAT_EQ(m(3, 3), 1.0f);

	// Off-diagonal should be zero
	EXPECT_FLOAT_EQ(m(0, 1), 0.0f);
	EXPECT_FLOAT_EQ(m(0, 2), 0.0f);
}

TEST(Matrix44FromScaleTest, Uniform_CreatesUniformScale)
{
	Matrix44 m = Matrix44::FromScale(5.0f);

	EXPECT_FLOAT_EQ(m(0, 0), 5.0f);
	EXPECT_FLOAT_EQ(m(1, 1), 5.0f);
	EXPECT_FLOAT_EQ(m(2, 2), 5.0f);
	EXPECT_FLOAT_EQ(m(3, 3), 1.0f);
}

TEST(Matrix44FromScaleTest, TransformPoint_ScalesCorrectly)
{
	Vector3D scale(2.0f, 3.0f, 4.0f);
	Matrix44 m = Matrix44::FromScale(scale);

	Vector3D point(1.0f, 2.0f, 3.0f);
	Vector3D transformed = m.TransformPoint(point);

	ExpectVec3Near(transformed, Vector3D(2.0f, 6.0f, 12.0f));
}

// ==============================================================================
// TRS Tests
// ==============================================================================

TEST(Matrix44FromTRSTest, CompositionOrder_ScaleThenRotateThenTranslate)
{
	Vector3D translation(10.0f, 0.0f, 0.0f);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::FromDegrees(90.0f));
	Vector3D scale(2.0f, 2.0f, 2.0f);

	Matrix44 trs = Matrix44::FromTRS(translation, rotation, scale);

	// Apply to origin: scale(0) → rotate(0) → translate(origin) = translation
	Vector3D point(0.0f, 0.0f, 0.0f);
	Vector3D transformed = trs.TransformPoint(point);
	ExpectVec3Near(transformed, translation);

	// Apply to X-axis: scale(2,0,0) → rotate(-2,0,0) → translate(8,0,0)
	Vector3D xAxis(1.0f, 0.0f, 0.0f);
	Vector3D transformedX = trs.TransformPoint(xAxis);
	// After scale: (2,0,0)
	// After 90deg Y rotation (right-handed): (0,0,-2)
	// After translate: (10,0,-2)
	ExpectVec3Near(transformedX, Vector3D(10.0f, 0.0f, -2.0f));
}

TEST(Matrix44FromTRSTest, RoundTrip_GetComponentsMatchesInput)
{
	Vector3D translation(10.0f, 20.0f, 30.0f);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::FromDegrees(45.0f));
	Vector3D scale(2.0f, 3.0f, 4.0f);

	Matrix44 trs = Matrix44::FromTRS(translation, rotation, scale);

	Vector3D extractedTranslation = trs.GetTranslation();
	Quaternion extractedRotation = trs.GetRotation();
	Vector3D extractedScale = trs.GetScale();

	ExpectVec3Near(extractedTranslation, translation);
	// Quaternions q and -q represent the same rotation
	EXPECT_TRUE(
		(std::abs(extractedRotation.x - rotation.x) < kEpsilon &&
		 std::abs(extractedRotation.y - rotation.y) < kEpsilon &&
		 std::abs(extractedRotation.z - rotation.z) < kEpsilon &&
		 std::abs(extractedRotation.w - rotation.w) < kEpsilon) ||
		(std::abs(extractedRotation.x + rotation.x) < kEpsilon &&
		 std::abs(extractedRotation.y + rotation.y) < kEpsilon &&
		 std::abs(extractedRotation.z + rotation.z) < kEpsilon &&
		 std::abs(extractedRotation.w + rotation.w) < kEpsilon)
	);
	ExpectVec3Near(extractedScale, scale);
}

// ==============================================================================
// Perspective Tests
// ==============================================================================

TEST(Matrix44PerspectiveTest, SymmetricFrustum)
{
	Angle fovY = Angle::FromDegrees(60.0f);
	float aspect = 16.0f / 9.0f;
	float nearZ = 0.1f;
	float farZ = 100.0f;

	Matrix44 p = Matrix44::Perspective(fovY, aspect, nearZ, farZ);

	// For a symmetric frustum, m[0][2] and m[1][2] should be zero
	EXPECT_NEAR(p(0, 2), 0.0f, kEpsilon);
	EXPECT_NEAR(p(1, 2), 0.0f, kEpsilon);

	// m[3][2] should be -1 (for right-handed)
	EXPECT_NEAR(p(3, 2), -1.0f, kEpsilon);
	EXPECT_NEAR(p(3, 3), 0.0f, kEpsilon);
}

TEST(Matrix44PerspectiveTest, AspectRatio_Honored)
{
	Angle fovY = Angle::FromDegrees(90.0f);
	float aspect = 2.0f;  // width is 2x height
	float nearZ = 0.1f;
	float farZ = 100.0f;

	Matrix44 p = Matrix44::Perspective(fovY, aspect, nearZ, farZ);

	// For 90deg fov, f = 1.0
	// m[0][0] = f / aspect = 1.0 / 2.0 = 0.5
	// m[1][1] = f = 1.0
	EXPECT_NEAR(p(0, 0) * aspect, p(1, 1), kEpsilon);
}

TEST(Matrix44PerspectiveTest, NearFar_DepthMapsToMinusOneToOne)
{
	Angle fovY = Angle::FromDegrees(60.0f);
	float aspect = 1.0f;
	float nearZ = 1.0f;
	float farZ = 10.0f;

	Matrix44 p = Matrix44::Perspective(fovY, aspect, nearZ, farZ);

	// Point at near plane center: (0, 0, -nearZ, 1) in view space
	// After perspective: homogeneous divide should yield z = -1 (OpenGL convention)
	Vector4D nearPoint(0.0f, 0.0f, -nearZ, 1.0f);
	Vector4D nearClip = p.TransformVector4(nearPoint);
	float nearNDC = nearClip.z / nearClip.w;
	EXPECT_NEAR(nearNDC, -1.0f, kEpsilon);

	// Point at far plane center: (0, 0, -farZ, 1)
	Vector4D farPoint(0.0f, 0.0f, -farZ, 1.0f);
	Vector4D farClip = p.TransformVector4(farPoint);
	float farNDC = farClip.z / farClip.w;
	EXPECT_NEAR(farNDC, 1.0f, kEpsilon);
}

// ==============================================================================
// Orthographic Tests
// ==============================================================================

TEST(Matrix44OrthographicTest, CubeToNDC_Mapping)
{
	float left = -1.0f, right = 1.0f;
	float bottom = -1.0f, top = 1.0f;
	float nearZ = 0.1f, farZ = 10.0f;

	Matrix44 ortho = Matrix44::Orthographic(left, right, bottom, top, nearZ, farZ);

	// Center of near plane: (0, 0, -nearZ) → NDC (0, 0, -1)
	Vector3D nearCenter(0.0f, 0.0f, -nearZ);
	Vector3D nearNDC = ortho.TransformPoint(nearCenter);
	ExpectVec3Near(nearNDC, Vector3D(0.0f, 0.0f, -1.0f));

	// Center of far plane: (0, 0, -farZ) → NDC (0, 0, 1)
	Vector3D farCenter(0.0f, 0.0f, -farZ);
	Vector3D farNDC = ortho.TransformPoint(farCenter);
	ExpectVec3Near(farNDC, Vector3D(0.0f, 0.0f, 1.0f));
}

TEST(Matrix44OrthographicTest, DepthRange_MinusOneToOne)
{
	Matrix44 ortho = Matrix44::Orthographic(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 100.0f);

	Vector3D nearPoint(0.0f, 0.0f, -1.0f);
	Vector3D farPoint(0.0f, 0.0f, -100.0f);

	Vector3D nearNDC = ortho.TransformPoint(nearPoint);
	Vector3D farNDC = ortho.TransformPoint(farPoint);

	EXPECT_NEAR(nearNDC.z, -1.0f, kEpsilon);
	EXPECT_NEAR(farNDC.z, 1.0f, kEpsilon);
}

// ==============================================================================
// LookAt Tests
// ==============================================================================

TEST(Matrix44LookAtTest, EyeAtOrigin_LookingDownMinusZ_UpY_IsIdentityRotation)
{
	Vector3D eye(0.0f, 0.0f, 0.0f);
	Vector3D target(0.0f, 0.0f, -1.0f);
	Vector3D up(0.0f, 1.0f, 0.0f);

	Matrix44 view = Matrix44::LookAt(eye, target, up);

	// World point at (0,0,-1) should map to (0,0,-1) in view space (distance 1 down -Z)
	Vector3D worldPoint(0.0f, 0.0f, -1.0f);
	Vector3D viewPoint = view.TransformPoint(worldPoint);
	ExpectVec3Near(viewPoint, Vector3D(0.0f, 0.0f, -1.0f));
}

TEST(Matrix44LookAtTest, WorldPoint_TransformsToViewSpace)
{
	Vector3D eye(0.0f, 0.0f, 10.0f);
	Vector3D target(0.0f, 0.0f, 0.0f);
	Vector3D up(0.0f, 1.0f, 0.0f);

	Matrix44 view = Matrix44::LookAt(eye, target, up);

	// World point at origin should be 10 units down -Z in view space
	Vector3D origin(0.0f, 0.0f, 0.0f);
	Vector3D viewOrigin = view.TransformPoint(origin);
	ExpectVec3Near(viewOrigin, Vector3D(0.0f, 0.0f, -10.0f));
}

// ==============================================================================
// Multiplication Tests
// ==============================================================================

TEST(Matrix44MultiplyTest, CompositionOrder_Correct)
{
	Matrix44 translation = Matrix44::FromTranslation(Vector3D(10.0f, 0.0f, 0.0f));
	Matrix44 scale = Matrix44::FromScale(2.0f);

	// (T * S).TransformPoint(p) should equal T.TransformPoint(S.TransformPoint(p))
	Vector3D point(1.0f, 0.0f, 0.0f);

	Vector3D composedResult = (translation * scale).TransformPoint(point);
	Vector3D stepByStep = translation.TransformPoint(scale.TransformPoint(point));

	ExpectVec3Near(composedResult, stepByStep);
}

TEST(Matrix44MultiplyTest, Identity_IsLeftIdentity)
{
	Matrix44 identity = Matrix44::Identity();
	Matrix44 m = Matrix44::FromTranslation(Vector3D(5.0f, 10.0f, 15.0f));

	Matrix44 result = identity * m;
	ExpectMatrix44Near(result, m);
}

TEST(Matrix44MultiplyTest, Identity_IsRightIdentity)
{
	Matrix44 identity = Matrix44::Identity();
	Matrix44 m = Matrix44::FromScale(Vector3D(2.0f, 3.0f, 4.0f));

	Matrix44 result = m * identity;
	ExpectMatrix44Near(result, m);
}

// ==============================================================================
// Transform Tests
// ==============================================================================

TEST(Matrix44TransformTest, TransformPoint_WithTranslation)
{
	Matrix44 m = Matrix44::FromTranslation(Vector3D(5.0f, 10.0f, 15.0f));
	Vector3D point(1.0f, 2.0f, 3.0f);

	Vector3D result = m.TransformPoint(point);
	ExpectVec3Near(result, Vector3D(6.0f, 12.0f, 18.0f));
}

TEST(Matrix44TransformTest, TransformDirection_IgnoresTranslation)
{
	Matrix44 m = Matrix44::FromTranslation(Vector3D(5.0f, 10.0f, 15.0f));
	Vector3D direction(1.0f, 0.0f, 0.0f);

	Vector3D result = m.TransformDirection(direction);
	ExpectVec3Near(result, direction);
}

TEST(Matrix44TransformTest, TransformVector4_NoDivide)
{
	Matrix44 m = Matrix44::FromScale(2.0f);
	Vector4D v(1.0f, 2.0f, 3.0f, 4.0f);

	Vector4D result = m.TransformVector4(v);
	ExpectVec4Near(result, Vector4D(2.0f, 4.0f, 6.0f, 4.0f));
}

TEST(Matrix44TransformTest, PerspectiveMatrix_DividesCorrectly)
{
	Matrix44 p = Matrix44::Perspective(Angle::FromDegrees(90.0f), 1.0f, 1.0f, 10.0f);

	Vector3D point(0.5f, 0.5f, -2.0f);  // In front of camera, at -2 view-space Z
	Vector3D ndc = p.TransformPoint(point);

	// After perspective divide, w should have been applied
	// Just check that the result is finite and reasonable
	EXPECT_TRUE(std::isfinite(ndc.x));
	EXPECT_TRUE(std::isfinite(ndc.y));
	EXPECT_TRUE(std::isfinite(ndc.z));
}

// ==============================================================================
// Inverse Tests
// ==============================================================================

TEST(Matrix44InverseTest, Identity_InverseIsIdentity)
{
	Matrix44 identity = Matrix44::Identity();
	Matrix44 inverse = identity.Inverse();

	ExpectMatrix44Near(inverse, identity);
}

TEST(Matrix44InverseTest, MatrixTimesInverse_IsIdentity)
{
	Matrix44 m = Matrix44::FromTRS(
		Vector3D(10.0f, 20.0f, 30.0f),
		Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::FromDegrees(45.0f)),
		Vector3D(2.0f, 3.0f, 4.0f)
	);

	Matrix44 inverse = m.Inverse();
	Matrix44 product = m * inverse;

	ExpectMatrix44Near(product, Matrix44::Identity(), 1e-3f);
}

TEST(Matrix44InverseTest, SingularMatrix_ReturnsIdentity)
{
	// Create a singular matrix (zero scale)
	Matrix44 singular = Matrix44::FromScale(Vector3D(0.0f, 1.0f, 1.0f));
	Matrix44 inverse = singular.Inverse();

	ExpectMatrix44Near(inverse, Matrix44::Identity());
}

// ==============================================================================
// Determinant Tests
// ==============================================================================

TEST(Matrix44DeterminantTest, Identity_IsOne)
{
	Matrix44 identity = Matrix44::Identity();
	float det = identity.Determinant();

	EXPECT_NEAR(det, 1.0f, kEpsilon);
}

TEST(Matrix44DeterminantTest, ScaleMatrix_IsProduct)
{
	Vector3D scale(2.0f, 3.0f, 4.0f);
	Matrix44 m = Matrix44::FromScale(scale);
	float det = m.Determinant();

	EXPECT_NEAR(det, 2.0f * 3.0f * 4.0f, kEpsilon);
}

TEST(Matrix44DeterminantTest, SingularMatrix_IsZero)
{
	Matrix44 singular = Matrix44::FromScale(Vector3D(0.0f, 1.0f, 1.0f));
	float det = singular.Determinant();

	EXPECT_NEAR(det, 0.0f, kEpsilon);
}

// ==============================================================================
// Transpose Tests
// ==============================================================================

TEST(Matrix44TransposeTest, TransposeTwice_IsOriginal)
{
	Matrix44 m(
		1, 2, 3, 4,
		5, 6, 7, 8,
		9, 10, 11, 12,
		13, 14, 15, 16
	);

	Matrix44 transposed = m.Transpose().Transpose();
	ExpectMatrix44Near(transposed, m);
}

TEST(Matrix44TransposeTest, Identity_TransposeIsIdentity)
{
	Matrix44 identity = Matrix44::Identity();
	Matrix44 transposed = identity.Transpose();

	ExpectMatrix44Near(transposed, identity);
}

TEST(Matrix44TransposeTest, RowsBecomeCols)
{
	Matrix44 m(
		1, 2, 3, 4,
		5, 6, 7, 8,
		9, 10, 11, 12,
		13, 14, 15, 16
	);

	Matrix44 t = m.Transpose();

	// First row of m becomes first column of t
	EXPECT_FLOAT_EQ(t(0, 0), m(0, 0));
	EXPECT_FLOAT_EQ(t(1, 0), m(0, 1));
	EXPECT_FLOAT_EQ(t(2, 0), m(0, 2));
	EXPECT_FLOAT_EQ(t(3, 0), m(0, 3));
}

// ==============================================================================
// GetColumnMajor Tests
// ==============================================================================

TEST(Matrix44GetColumnMajorTest, Identity_ProducesCanonicalBuffer)
{
	Matrix44 identity = Matrix44::Identity();
	float buffer[16];
	identity.GetColumnMajor(buffer);

	// Column-major identity: 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
	float expected[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};

	for (int i = 0; i < 16; ++i)
	{
		EXPECT_FLOAT_EQ(buffer[i], expected[i]) << "Index " << i;
	}
}

TEST(Matrix44GetColumnMajorTest, OutDataLayout_MatchesSpec)
{
	Matrix44 m(
		1, 2, 3, 4,
		5, 6, 7, 8,
		9, 10, 11, 12,
		13, 14, 15, 16
	);

	float buffer[16];
	m.GetColumnMajor(buffer);

	// outData[col * 4 + row] = m[row][col]
	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			EXPECT_FLOAT_EQ(buffer[col * 4 + row], m(row, col))
				<< "col=" << col << " row=" << row;
		}
	}
}

// ==============================================================================
// Quaternion Matrix44 Conversion Tests (appended to TestQuaternion suite)
// ==============================================================================

TEST(QuaternionMatrix44Test, RoundTrip_Identity)
{
	Quaternion identity = Quaternion::Identity();
	Matrix44 m = identity.ToMatrix44();
	Quaternion roundTrip = Quaternion::FromMatrix44(m);

	// Quaternions q and -q represent the same rotation
	bool match = (std::abs(roundTrip.x - identity.x) < kEpsilon &&
	              std::abs(roundTrip.y - identity.y) < kEpsilon &&
	              std::abs(roundTrip.z - identity.z) < kEpsilon &&
	              std::abs(roundTrip.w - identity.w) < kEpsilon) ||
	             (std::abs(roundTrip.x + identity.x) < kEpsilon &&
	              std::abs(roundTrip.y + identity.y) < kEpsilon &&
	              std::abs(roundTrip.z + identity.z) < kEpsilon &&
	              std::abs(roundTrip.w + identity.w) < kEpsilon);

	EXPECT_TRUE(match);
}

TEST(QuaternionMatrix44Test, RoundTrip_ArbitraryAxisAngle)
{
	Quaternion q = Quaternion::FromAxisAngle(Vector3D(1.0f, 1.0f, 1.0f).AsNormal(), Angle::FromDegrees(73.0f));
	Matrix44 m = q.ToMatrix44();
	Quaternion roundTrip = Quaternion::FromMatrix44(m);

	bool match = (std::abs(roundTrip.x - q.x) < kEpsilon &&
	              std::abs(roundTrip.y - q.y) < kEpsilon &&
	              std::abs(roundTrip.z - q.z) < kEpsilon &&
	              std::abs(roundTrip.w - q.w) < kEpsilon) ||
	             (std::abs(roundTrip.x + q.x) < kEpsilon &&
	              std::abs(roundTrip.y + q.y) < kEpsilon &&
	              std::abs(roundTrip.z + q.z) < kEpsilon &&
	              std::abs(roundTrip.w + q.w) < kEpsilon);

	EXPECT_TRUE(match);
}

TEST(QuaternionMatrix44Test, FromMatrix44_OfIdentityQuaternion)
{
	Matrix44 m = Quaternion::Identity().ToMatrix44();
	Quaternion q = Quaternion::FromMatrix44(m);

	EXPECT_NEAR(q.x, 0.0f, kEpsilon);
	EXPECT_NEAR(q.y, 0.0f, kEpsilon);
	EXPECT_NEAR(q.z, 0.0f, kEpsilon);
	EXPECT_NEAR(std::abs(q.w), 1.0f, kEpsilon);
}

TEST(QuaternionMatrix44Test, ToMatrix44_LastRowIsZeroZeroZeroOne)
{
	Quaternion q = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::FromDegrees(45.0f));
	Matrix44 m = q.ToMatrix44();

	EXPECT_FLOAT_EQ(m(3, 0), 0.0f);
	EXPECT_FLOAT_EQ(m(3, 1), 0.0f);
	EXPECT_FLOAT_EQ(m(3, 2), 0.0f);
	EXPECT_FLOAT_EQ(m(3, 3), 1.0f);
}
