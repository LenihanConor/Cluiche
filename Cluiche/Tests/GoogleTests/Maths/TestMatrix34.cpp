// TestMatrix34.cpp - Google Test unit tests for Matrix34
//
// Tests the Dia::Maths::Matrix34 class for 3x4 affine transformations

#include <gtest/gtest.h>
#include <DiaMaths/Matrix/Matrix34.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Vector/Vector3D.h>
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

static void ExpectMatrix34Near(const Matrix34& a, const Matrix34& b, float eps = kEpsilon)
{
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			EXPECT_NEAR(a(i, j), b(i, j), eps)
				<< "Element at (" << i << "," << j << ") differs";
		}
	}
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
// Matrix34StorageTest
// ==============================================================================

TEST(Matrix34StorageTest, DefaultConstruction_CreatesIdentityMatrix)
{
	Matrix34 m;

	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			float expected = (row == col) ? 1.0f : 0.0f;
			EXPECT_FLOAT_EQ(m(row, col), expected)
				<< "Element at (" << row << "," << col << ") should be " << expected;
		}
	}
}

TEST(Matrix34StorageTest, ComponentConstruction_InitializesAllElements)
{
	Matrix34 m(
		1, 2, 3, 4,
		5, 6, 7, 8,
		9, 10, 11, 12
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
}

TEST(Matrix34StorageTest, CopyConstruction_CopiesAllElements)
{
	Matrix34 original(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
	Matrix34 copy(original);

	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			EXPECT_FLOAT_EQ(copy(row, col), original(row, col));
		}
	}
}

TEST(Matrix34StorageTest, ElementAccess_MatchesDirectAccess)
{
	Matrix34 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);

	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			EXPECT_FLOAT_EQ(m(row, col), m.m[row][col]);
		}
	}
}

TEST(Matrix34StorageTest, Assignment_CopiesAllElements)
{
	Matrix34 original(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
	Matrix34 assigned;
	assigned = original;

	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			EXPECT_FLOAT_EQ(assigned(row, col), original(row, col));
		}
	}
}

// ==============================================================================
// Matrix34FactoryTest
// ==============================================================================

TEST(Matrix34FactoryTest, Identity_ProducesIdentityMatrix)
{
	Matrix34 m = Matrix34::Identity();

	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			float expected = (row == col) ? 1.0f : 0.0f;
			EXPECT_FLOAT_EQ(m(row, col), expected);
		}
	}
}

TEST(Matrix34FactoryTest, FromTranslation_CreatesTranslationMatrix)
{
	Vector3D translation(10.0f, 20.0f, 30.0f);
	Matrix34 m = Matrix34::FromTranslation(translation);

	// Translation should be in the last column
	EXPECT_FLOAT_EQ(m(0, 3), 10.0f);
	EXPECT_FLOAT_EQ(m(1, 3), 20.0f);
	EXPECT_FLOAT_EQ(m(2, 3), 30.0f);

	// Upper-left 3x3 should be identity
	EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
	EXPECT_FLOAT_EQ(m(1, 1), 1.0f);
	EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
}

TEST(Matrix34FactoryTest, FromRotation_CreatesRotationMatrix)
{
	// Rotate 90 degrees around Y axis
	Quaternion rot = Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg90);
	Matrix34 m = Matrix34::FromRotation(rot);

	// Translation column should be zero
	EXPECT_NEAR(m(0, 3), 0.0f, kEpsilon);
	EXPECT_NEAR(m(1, 3), 0.0f, kEpsilon);
	EXPECT_NEAR(m(2, 3), 0.0f, kEpsilon);

	// Rotate a point to verify rotation
	Vector3D point(1, 0, 0);
	Vector3D rotated = m.TransformPoint(point);
	ExpectVec3Near(rotated, Vector3D(0, 0, -1));
}

TEST(Matrix34FactoryTest, FromScale_CreatesScaleMatrix)
{
	Vector3D scale(2.0f, 3.0f, 4.0f);
	Matrix34 m = Matrix34::FromScale(scale);

	// Diagonal should contain scale values
	EXPECT_FLOAT_EQ(m(0, 0), 2.0f);
	EXPECT_FLOAT_EQ(m(1, 1), 3.0f);
	EXPECT_FLOAT_EQ(m(2, 2), 4.0f);

	// Translation should be zero
	EXPECT_FLOAT_EQ(m(0, 3), 0.0f);
	EXPECT_FLOAT_EQ(m(1, 3), 0.0f);
	EXPECT_FLOAT_EQ(m(2, 3), 0.0f);
}

TEST(Matrix34FactoryTest, FromScaleUniform_CreatesUniformScaleMatrix)
{
	Matrix34 m = Matrix34::FromScale(5.0f);

	EXPECT_FLOAT_EQ(m(0, 0), 5.0f);
	EXPECT_FLOAT_EQ(m(1, 1), 5.0f);
	EXPECT_FLOAT_EQ(m(2, 2), 5.0f);
}

TEST(Matrix34FactoryTest, FromTRS_ComposesCorrectly)
{
	Vector3D translation(10, 20, 30);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg90);
	Vector3D scale(2, 2, 2);

	Matrix34 m = Matrix34::FromTRS(translation, rotation, scale);

	// Test that TRS composition works correctly by transforming a point
	Vector3D point(1, 0, 0);
	Vector3D result = m.TransformPoint(point);

	// Point should be: scaled (2,0,0), rotated (0,0,-2), translated (10,20,28)
	ExpectVec3Near(result, Vector3D(10, 20, 28));
}

// ==============================================================================
// Matrix34Matrix44ConvTest
// ==============================================================================

TEST(Matrix34Matrix44ConvTest, ToMatrix44_AppendsIdentityRow)
{
	Matrix34 m34(
		1, 2, 3, 4,
		5, 6, 7, 8,
		9, 10, 11, 12
	);

	Matrix44 m44 = m34.ToMatrix44();

	// Check top 3 rows match
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			EXPECT_FLOAT_EQ(m44(i, j), m34(i, j));
		}
	}

	// Check bottom row is (0, 0, 0, 1)
	EXPECT_FLOAT_EQ(m44(3, 0), 0.0f);
	EXPECT_FLOAT_EQ(m44(3, 1), 0.0f);
	EXPECT_FLOAT_EQ(m44(3, 2), 0.0f);
	EXPECT_FLOAT_EQ(m44(3, 3), 1.0f);
}

TEST(Matrix34Matrix44ConvTest, FromMatrix44_CopiesTopThreeRows)
{
	// Create an affine Matrix44
	Vector3D translation(10, 20, 30);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg45);
	Vector3D scale(2, 3, 4);
	Matrix44 m44 = Matrix44::FromTRS(translation, rotation, scale);

	Matrix34 m34 = Matrix34::FromMatrix44(m44);

	// Check that top 3 rows match
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			EXPECT_FLOAT_EQ(m34(i, j), m44(i, j));
		}
	}
}

TEST(Matrix34Matrix44ConvTest, RoundTrip_PreservesAffineMatrix)
{
	// Create a Matrix34
	Vector3D translation(5, 10, 15);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(1, 0, 0), Angle::Deg30);
	Vector3D scale(1.5f, 2.0f, 2.5f);
	Matrix34 original = Matrix34::FromTRS(translation, rotation, scale);

	// Round-trip: Matrix34 -> Matrix44 -> Matrix34
	Matrix44 m44 = original.ToMatrix44();
	Matrix34 roundTrip = Matrix34::FromMatrix44(m44);

	ExpectMatrix34Near(original, roundTrip);
}

// NOTE: We do not include a death-test for FromMatrix44 with non-affine input
// as the test infrastructure does not support death tests. The debug assert
// exists in the implementation and would trigger for non-affine inputs.

// ==============================================================================
// Matrix34MultiplyTest
// ==============================================================================

TEST(Matrix34MultiplyTest, Identity_IsLeftIdentity)
{
	Matrix34 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
	Matrix34 result = Matrix34::Identity() * m;

	ExpectMatrix34Near(result, m);
}

TEST(Matrix34MultiplyTest, Identity_IsRightIdentity)
{
	Matrix34 m(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
	Matrix34 result = m * Matrix34::Identity();

	ExpectMatrix34Near(result, m);
}

TEST(Matrix34MultiplyTest, TransformPoint_AssociativeWithMultiplication)
{
	Matrix34 a = Matrix34::FromTranslation(Vector3D(10, 0, 0));
	Matrix34 b = Matrix34::FromTranslation(Vector3D(0, 20, 0));
	Vector3D point(1, 2, 3);

	// (A * B).TransformPoint(v) should equal A.TransformPoint(B.TransformPoint(v))
	Vector3D result1 = (a * b).TransformPoint(point);
	Vector3D result2 = a.TransformPoint(b.TransformPoint(point));

	ExpectVec3Near(result1, result2);
}

TEST(Matrix34MultiplyTest, TwoTranslations_Add)
{
	Matrix34 a = Matrix34::FromTranslation(Vector3D(10, 20, 30));
	Matrix34 b = Matrix34::FromTranslation(Vector3D(5, 10, 15));
	Matrix34 result = a * b;

	Vector3D translation = result.GetTranslation();
	ExpectVec3Near(translation, Vector3D(15, 30, 45));
}

TEST(Matrix34MultiplyTest, RotationAndTranslation_ComposesCorrectly)
{
	Matrix34 rot = Matrix34::FromRotation(Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg90));
	Matrix34 trans = Matrix34::FromTranslation(Vector3D(10, 0, 0));

	// Rotate then translate
	Matrix34 m = trans * rot;
	Vector3D point(1, 0, 0);
	Vector3D result = m.TransformPoint(point);

	// (1,0,0) rotated 90deg around Y = (0,0,-1), then translated by (10,0,0) = (10,0,-1)
	ExpectVec3Near(result, Vector3D(10, 0, -1));
}

// ==============================================================================
// Matrix34TransformTest
// ==============================================================================

TEST(Matrix34TransformTest, TransformPoint_AppliesTranslation)
{
	Matrix34 m = Matrix34::FromTranslation(Vector3D(10, 20, 30));
	Vector3D point(1, 2, 3);
	Vector3D result = m.TransformPoint(point);

	ExpectVec3Near(result, Vector3D(11, 22, 33));
}

TEST(Matrix34TransformTest, TransformDirection_IgnoresTranslation)
{
	Matrix34 m = Matrix34::FromTranslation(Vector3D(10, 20, 30));
	Vector3D dir(1, 0, 0);
	Vector3D result = m.TransformDirection(dir);

	ExpectVec3Near(result, Vector3D(1, 0, 0));
}

TEST(Matrix34TransformTest, TransformDirection_AppliesRotation)
{
	Matrix34 m = Matrix34::FromRotation(Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg90));
	Vector3D dir(1, 0, 0);
	Vector3D result = m.TransformDirection(dir);

	ExpectVec3Near(result, Vector3D(0, 0, -1));
}

TEST(Matrix34TransformTest, TransformDirection_AppliesScale)
{
	Matrix34 m = Matrix34::FromScale(Vector3D(2, 3, 4));
	Vector3D dir(1, 1, 1);
	Vector3D result = m.TransformDirection(dir);

	ExpectVec3Near(result, Vector3D(2, 3, 4));
}

// ==============================================================================
// Matrix34InverseTest
// ==============================================================================

TEST(Matrix34InverseTest, Identity_InverseIsIdentity)
{
	Matrix34 m = Matrix34::Identity();
	Matrix34 inv = m.Inverse();

	ExpectMatrix34Near(inv, Matrix34::Identity());
}

TEST(Matrix34InverseTest, Translation_InverseNegatesTranslation)
{
	Matrix34 m = Matrix34::FromTranslation(Vector3D(10, 20, 30));
	Matrix34 inv = m.Inverse();

	Matrix34 expected = Matrix34::FromTranslation(Vector3D(-10, -20, -30));
	ExpectMatrix34Near(inv, expected);
}

TEST(Matrix34InverseTest, Product_WithInverse_GivesIdentity)
{
	Vector3D translation(5, 10, 15);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg45);
	Vector3D scale(2, 3, 4);
	Matrix34 m = Matrix34::FromTRS(translation, rotation, scale);

	Matrix34 inv = m.Inverse();
	Matrix34 product = m * inv;

	ExpectMatrix34Near(product, Matrix34::Identity());
}

TEST(Matrix34InverseTest, InverseOfInverse_GivesOriginal)
{
	Vector3D translation(1, 2, 3);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(1, 1, 1).AsNormal(), Angle::Deg60);
	Vector3D scale(2, 2, 2);
	Matrix34 m = Matrix34::FromTRS(translation, rotation, scale);

	Matrix34 inv = m.Inverse();
	Matrix34 invInv = inv.Inverse();

	ExpectMatrix34Near(invInv, m);
}

TEST(Matrix34InverseTest, TransformPoint_InverseUndoesTransform)
{
	Vector3D translation(10, 20, 30);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg90);
	Vector3D scale(2, 2, 2);
	Matrix34 m = Matrix34::FromTRS(translation, rotation, scale);
	Matrix34 inv = m.Inverse();

	Vector3D point(5, 7, 11);
	Vector3D transformed = m.TransformPoint(point);
	Vector3D restored = inv.TransformPoint(transformed);

	ExpectVec3Near(restored, point);
}

// ==============================================================================
// Matrix34ExtractionTest
// ==============================================================================

TEST(Matrix34ExtractionTest, GetTranslation_ReturnsRightColumn)
{
	Matrix34 m = Matrix34::FromTranslation(Vector3D(10, 20, 30));
	Vector3D translation = m.GetTranslation();

	ExpectVec3Near(translation, Vector3D(10, 20, 30));
}

TEST(Matrix34ExtractionTest, GetRotation_ExtractsRotationFromTRS)
{
	Vector3D translation(10, 20, 30);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg45);
	Vector3D scale(2, 3, 4);
	Matrix34 m = Matrix34::FromTRS(translation, rotation, scale);

	Quaternion extracted = m.GetRotation();

	// Compare by transforming a vector with both quaternions
	Vector3D testVec(1, 0, 0);
	Vector3D v1 = rotation.Rotate(testVec);
	Vector3D v2 = extracted.Rotate(testVec);

	ExpectVec3Near(v1, v2);
}

TEST(Matrix34ExtractionTest, GetScale_ExtractsScaleFromTRS)
{
	Vector3D translation(10, 20, 30);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(0, 1, 0), Angle::Deg45);
	Vector3D scale(2, 3, 4);
	Matrix34 m = Matrix34::FromTRS(translation, rotation, scale);

	Vector3D extracted = m.GetScale();

	ExpectVec3Near(extracted, scale);
}

TEST(Matrix34ExtractionTest, RoundTrip_TRS)
{
	Vector3D translation(5, 10, 15);
	Quaternion rotation = Quaternion::FromAxisAngle(Vector3D(1, 1, 1).AsNormal(), Angle::Deg30);
	Vector3D scale(1.5f, 2.0f, 2.5f);
	Matrix34 m = Matrix34::FromTRS(translation, rotation, scale);

	Vector3D extractedT = m.GetTranslation();
	Quaternion extractedR = m.GetRotation();
	Vector3D extractedS = m.GetScale();

	Matrix34 reconstructed = Matrix34::FromTRS(extractedT, extractedR, extractedS);

	ExpectMatrix34Near(reconstructed, m);
}

TEST(Matrix34ExtractionTest, Determinant_ReturnsUpperLeft3x3Determinant)
{
	// Scale matrix should have determinant = product of scale factors
	Matrix34 m = Matrix34::FromScale(Vector3D(2, 3, 4));
	float det = m.Determinant();

	EXPECT_NEAR(det, 24.0f, kEpsilon);  // 2 * 3 * 4 = 24
}
