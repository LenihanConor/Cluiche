// TestTransform3D.cpp - Google Test unit tests for Transform3D
//
// Tests the Dia::Maths::Transform3D class for 3D hierarchical transformations

#include <gtest/gtest.h>
#include <DiaMaths/Transform/Transform3D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Matrix/Matrix34.h>
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

static void ExpectQuaternionNear(const Quaternion& a, const Quaternion& b, float eps = kEpsilon)
{
	EXPECT_NEAR(a.x, b.x, eps);
	EXPECT_NEAR(a.y, b.y, eps);
	EXPECT_NEAR(a.z, b.z, eps);
	EXPECT_NEAR(a.w, b.w, eps);
}

// ==============================================================================
// Transform3DDefaultTest
// ==============================================================================

TEST(Transform3DDefaultTest, DefaultConstructor_ProducesIdentityTransformAtOrigin)
{
	Transform3D t;

	// Position should be (0,0,0)
	ExpectVec3Near(t.GetLocalPosition(), Vector3D::Zero());

	// Rotation should be identity quaternion (0,0,0,1)
	ExpectQuaternionNear(t.GetLocalRotation(), Quaternion::Identity());

	// Scale should be (1,1,1)
	ExpectVec3Near(t.GetLocalScale(), Vector3D(1.0f, 1.0f, 1.0f));

	// No parent
	EXPECT_EQ(t.GetParent(), nullptr);
	EXPECT_FALSE(t.HasParent());
}

// ==============================================================================
// Transform3DLocalSettersTest
// ==============================================================================

TEST(Transform3DLocalSettersTest, SetLocalPosition_ReadsBackCorrectly)
{
	Transform3D t;
	Vector3D pos(10.0f, 20.0f, 30.0f);

	t.SetLocalPosition(pos);

	ExpectVec3Near(t.GetLocalPosition(), pos);
}

TEST(Transform3DLocalSettersTest, SetLocalRotation_Quaternion_ReadsBackCorrectly)
{
	Transform3D t;
	Quaternion rot = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);

	t.SetLocalRotation(rot);

	ExpectQuaternionNear(t.GetLocalRotation(), rot);
}

TEST(Transform3DLocalSettersTest, SetLocalScale_Vector3D_ReadsBackCorrectly)
{
	Transform3D t;
	Vector3D scale(2.0f, 3.0f, 4.0f);

	t.SetLocalScale(scale);

	ExpectVec3Near(t.GetLocalScale(), scale);
}

TEST(Transform3DLocalSettersTest, SetLocalScale_Uniform_ReadsBackCorrectly)
{
	Transform3D t;

	t.SetLocalScale(5.0f);

	ExpectVec3Near(t.GetLocalScale(), Vector3D(5.0f, 5.0f, 5.0f));
}

// ==============================================================================
// Transform3DEulerOverloadTest
// ==============================================================================

TEST(Transform3DEulerOverloadTest, SetLocalRotation_Euler_StoresQuaternionEquivalent)
{
	Transform3D t;
	Angle yaw = Angle::Deg90;
	Angle pitch = Angle::Deg45;
	Angle roll = Angle::Deg30;

	t.SetLocalRotation(yaw, pitch, roll);

	Quaternion expected = Quaternion::FromEuler(yaw, pitch, roll);
	ExpectQuaternionNear(t.GetLocalRotation(), expected);
}

TEST(Transform3DEulerOverloadTest, SetLocalRotation_Euler_YXZOrder)
{
	Transform3D t;
	// Yaw 90° around Y should rotate X axis to -Z
	t.SetLocalRotation(Angle::Deg90, Angle::Deg0, Angle::Deg0);

	Vector3D forward = t.GetForward();
	// Forward = -Z, rotated 90° around Y should point toward -X
	ExpectVec3Near(forward, -Vector3D::XAxis());
}

// ==============================================================================
// Transform3DWorldNoParentTest
// ==============================================================================

TEST(Transform3DWorldNoParentTest, NoParent_WorldEqualsLocal_Position)
{
	Transform3D t;
	Vector3D pos(5.0f, 10.0f, 15.0f);
	t.SetLocalPosition(pos);

	ExpectVec3Near(t.GetWorldPosition(), pos);
}

TEST(Transform3DWorldNoParentTest, NoParent_WorldEqualsLocal_Rotation)
{
	Transform3D t;
	Quaternion rot = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
	t.SetLocalRotation(rot);

	ExpectQuaternionNear(t.GetWorldRotation(), rot);
}

TEST(Transform3DWorldNoParentTest, NoParent_WorldEqualsLocal_Scale)
{
	Transform3D t;
	Vector3D scale(2.0f, 3.0f, 4.0f);
	t.SetLocalScale(scale);

	ExpectVec3Near(t.GetWorldScale(), scale);
}

// ==============================================================================
// Transform3DWorldWithParentTest
// ==============================================================================

TEST(Transform3DWorldWithParentTest, ParentAtOffset_ChildWorldPositionCombines)
{
	Transform3D parent;
	parent.SetLocalPosition(Vector3D(10.0f, 0.0f, 0.0f));

	Transform3D child;
	child.SetLocalPosition(Vector3D(1.0f, 0.0f, 0.0f));
	child.SetParent(&parent);

	// Child world position = parent position + child local
	ExpectVec3Near(child.GetWorldPosition(), Vector3D(11.0f, 0.0f, 0.0f));
}

TEST(Transform3DWorldWithParentTest, ParentRotated90Y_ChildLocalXBecomesWorldNegZ)
{
	Transform3D parent;
	parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90));

	Transform3D child;
	child.SetLocalPosition(Vector3D(1.0f, 0.0f, 0.0f));  // Local +X
	child.SetParent(&parent);

	// Rotating +X by 90° around Y gives -Z
	Vector3D expected(0.0f, 0.0f, -1.0f);
	ExpectVec3Near(child.GetWorldPosition(), expected);
}

TEST(Transform3DWorldWithParentTest, ParentScaled_ChildWorldScaleCombines)
{
	Transform3D parent;
	parent.SetLocalScale(Vector3D(2.0f, 2.0f, 2.0f));

	Transform3D child;
	child.SetLocalScale(Vector3D(3.0f, 3.0f, 3.0f));
	child.SetParent(&parent);

	ExpectVec3Near(child.GetWorldScale(), Vector3D(6.0f, 6.0f, 6.0f));
}

// ==============================================================================
// Transform3DWorldNestedTest
// ==============================================================================

TEST(Transform3DWorldNestedTest, ThreeLevelHierarchy_WorldTransformComposesAll)
{
	Transform3D a;
	a.SetLocalPosition(Vector3D(10.0f, 0.0f, 0.0f));

	Transform3D b;
	b.SetLocalPosition(Vector3D(0.0f, 5.0f, 0.0f));
	b.SetParent(&a);

	Transform3D c;
	c.SetLocalPosition(Vector3D(0.0f, 0.0f, 3.0f));
	c.SetParent(&b);

	// C's world position should be (10, 5, 3)
	ExpectVec3Near(c.GetWorldPosition(), Vector3D(10.0f, 5.0f, 3.0f));
}

TEST(Transform3DWorldNestedTest, GetWorldTransform_OptimizedPath_MatchesPerGetterResults)
{
	Transform3D parent;
	parent.SetLocalPosition(Vector3D(5.0f, 10.0f, 15.0f));
	parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg45));
	parent.SetLocalScale(Vector3D(2.0f, 2.0f, 2.0f));

	Transform3D child;
	child.SetLocalPosition(Vector3D(1.0f, 2.0f, 3.0f));
	child.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg30));
	child.SetLocalScale(Vector3D(0.5f, 0.5f, 0.5f));
	child.SetParent(&parent);

	// Get via individual getters
	Vector3D worldPos1 = child.GetWorldPosition();
	Quaternion worldRot1 = child.GetWorldRotation();
	Vector3D worldScale1 = child.GetWorldScale();

	// Get via batch method
	Vector3D worldPos2;
	Quaternion worldRot2;
	Vector3D worldScale2;
	child.GetWorldTransform(worldPos2, worldRot2, worldScale2);

	// Should match
	ExpectVec3Near(worldPos1, worldPos2);
	ExpectQuaternionNear(worldRot1, worldRot2);
	ExpectVec3Near(worldScale1, worldScale2);
}

// ==============================================================================
// Transform3DSetWorldTest
// ==============================================================================

TEST(Transform3DSetWorldTest, SetWorldPosition_ThenGetWorldPosition_RoundTrips)
{
	Transform3D parent;
	parent.SetLocalPosition(Vector3D(10.0f, 20.0f, 30.0f));
	parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90));

	Transform3D child;
	child.SetParent(&parent);

	Vector3D targetWorldPos(50.0f, 60.0f, 70.0f);
	child.SetWorldPosition(targetWorldPos);

	ExpectVec3Near(child.GetWorldPosition(), targetWorldPos);
}

TEST(Transform3DSetWorldTest, SetWorldRotation_ThenGetWorldRotation_RoundTrips)
{
	Transform3D parent;
	parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg45));

	Transform3D child;
	child.SetParent(&parent);

	Quaternion targetWorldRot = Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg60);
	child.SetWorldRotation(targetWorldRot);

	ExpectQuaternionNear(child.GetWorldRotation(), targetWorldRot);
}

TEST(Transform3DSetWorldTest, SetWorldScale_ThenGetWorldScale_RoundTrips)
{
	Transform3D parent;
	parent.SetLocalScale(Vector3D(2.0f, 2.0f, 2.0f));

	Transform3D child;
	child.SetParent(&parent);

	Vector3D targetWorldScale(6.0f, 8.0f, 10.0f);
	child.SetWorldScale(targetWorldScale);

	ExpectVec3Near(child.GetWorldScale(), targetWorldScale);
}

// ==============================================================================
// Transform3DLookAtTest
// ==============================================================================

TEST(Transform3DLookAtTest, LookAt_ForwardPointsAtTarget)
{
	Transform3D t;
	t.SetLocalPosition(Vector3D(0.0f, 0.0f, 0.0f));

	Vector3D target(10.0f, 0.0f, 0.0f);
	t.LookAt(target);

	Vector3D forward = t.GetForward();
	Vector3D expectedDirection = (target - t.GetWorldPosition()).AsNormal();

	float dot = forward.Dot(expectedDirection);
	EXPECT_NEAR(dot, 1.0f, kEpsilon) << "Forward should point at target";
}

TEST(Transform3DLookAtTest, LookAt_WithParent_StillPointsAtWorldTarget)
{
	Transform3D parent;
	parent.SetLocalPosition(Vector3D(5.0f, 0.0f, 0.0f));
	parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90));

	Transform3D child;
	child.SetLocalPosition(Vector3D(1.0f, 0.0f, 0.0f));
	child.SetParent(&parent);

	Vector3D target(0.0f, 0.0f, 10.0f);
	child.LookAt(target);

	Vector3D forward = child.GetForward();
	Vector3D expectedDirection = (target - child.GetWorldPosition()).AsNormal();

	float dot = forward.Dot(expectedDirection);
	EXPECT_NEAR(dot, 1.0f, kEpsilon) << "Forward should point at world target despite parent";
}

// ==============================================================================
// Transform3DAxesTest
// ==============================================================================

TEST(Transform3DAxesTest, GetForward_DefaultRotation_IsNegativeZ)
{
	Transform3D t;

	Vector3D forward = t.GetForward();

	ExpectVec3Near(forward, -Vector3D::ZAxis());
}

TEST(Transform3DAxesTest, GetRight_DefaultRotation_IsPositiveX)
{
	Transform3D t;

	Vector3D right = t.GetRight();

	ExpectVec3Near(right, Vector3D::XAxis());
}

TEST(Transform3DAxesTest, GetUp_DefaultRotation_IsPositiveY)
{
	Transform3D t;

	Vector3D up = t.GetUp();

	ExpectVec3Near(up, Vector3D::YAxis());
}

TEST(Transform3DAxesTest, Rotate90Y_ForwardBecomesNegativeX)
{
	Transform3D t;
	t.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90));

	Vector3D forward = t.GetForward();

	ExpectVec3Near(forward, -Vector3D::XAxis());
}

// ==============================================================================
// Transform3DMatrixTest
// ==============================================================================

TEST(Transform3DMatrixTest, GetLocalMatrix_TransformPoint_MatchesTransformPoint)
{
	Transform3D t;
	t.SetLocalPosition(Vector3D(5.0f, 10.0f, 15.0f));
	t.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg45));
	t.SetLocalScale(Vector3D(2.0f, 3.0f, 4.0f));

	Vector3D localPoint(1.0f, 2.0f, 3.0f);

	Vector3D result1 = t.TransformPoint(localPoint);

	Matrix44 matrix = t.GetLocalMatrix();
	Vector3D result2 = matrix.TransformPoint(localPoint);

	ExpectVec3Near(result1, result2);
}

TEST(Transform3DMatrixTest, GetLocalAffine_ToMatrix44_MatchesGetLocalMatrix)
{
	Transform3D t;
	t.SetLocalPosition(Vector3D(1.0f, 2.0f, 3.0f));
	t.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg30));
	t.SetLocalScale(Vector3D(2.0f, 2.0f, 2.0f));

	Matrix44 fromMatrix44 = t.GetLocalMatrix();
	Matrix34 affine = t.GetLocalAffine();
	Matrix44 fromAffine = affine.ToMatrix44();

	// Should match within epsilon
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			EXPECT_NEAR(fromMatrix44(i, j), fromAffine(i, j), kEpsilon)
				<< "Element at (" << i << "," << j << ") differs";
		}
	}
}

// ==============================================================================
// Transform3DTranslateRotateScaleTest
// ==============================================================================

TEST(Transform3DTranslateRotateScaleTest, Translate_AddsInLocalRotatedSpace)
{
	Transform3D t;
	t.SetLocalPosition(Vector3D(0.0f, 0.0f, 0.0f));
	t.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90));

	// Translate +X in local space
	// With 90° Y rotation, local +X points toward world -Z
	t.Translate(Vector3D(1.0f, 0.0f, 0.0f));

	ExpectVec3Near(t.GetLocalPosition(), Vector3D(0.0f, 0.0f, -1.0f));
}

TEST(Transform3DTranslateRotateScaleTest, TranslateWorld_AddsDirectly)
{
	Transform3D t;
	t.SetLocalPosition(Vector3D(5.0f, 10.0f, 15.0f));

	t.TranslateWorld(Vector3D(1.0f, 2.0f, 3.0f));

	ExpectVec3Near(t.GetLocalPosition(), Vector3D(6.0f, 12.0f, 18.0f));
}

TEST(Transform3DTranslateRotateScaleTest, Rotate_ComposesQuaternions)
{
	Transform3D t;
	Quaternion initial = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg45);
	t.SetLocalRotation(initial);

	Quaternion delta = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg45);
	t.Rotate(delta);

	// Result should be ~90° around Y
	Quaternion expected = Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg90);
	ExpectQuaternionNear(t.GetLocalRotation(), expected);
}

TEST(Transform3DTranslateRotateScaleTest, Scale_MultipliesElementWise)
{
	Transform3D t;
	t.SetLocalScale(Vector3D(2.0f, 3.0f, 4.0f));

	t.Scale(Vector3D(2.0f, 2.0f, 2.0f));

	ExpectVec3Near(t.GetLocalScale(), Vector3D(4.0f, 6.0f, 8.0f));
}

TEST(Transform3DTranslateRotateScaleTest, Scale_UniformOverload_MultipliesAll)
{
	Transform3D t;
	t.SetLocalScale(Vector3D(2.0f, 3.0f, 4.0f));

	t.Scale(0.5f);

	ExpectVec3Near(t.GetLocalScale(), Vector3D(1.0f, 1.5f, 2.0f));
}

// ==============================================================================
// Transform3DInverseTransformTest
// ==============================================================================

TEST(Transform3DInverseTransformTest, TransformPoint_ThenInverse_RoundTrips)
{
	Transform3D t;
	t.SetLocalPosition(Vector3D(10.0f, 20.0f, 30.0f));
	t.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg60));
	t.SetLocalScale(Vector3D(2.0f, 3.0f, 4.0f));

	Vector3D localPoint(5.0f, 7.0f, 9.0f);

	Vector3D worldPoint = t.TransformPoint(localPoint);
	Vector3D backToLocal = t.InverseTransformPoint(worldPoint);

	ExpectVec3Near(backToLocal, localPoint);
}

TEST(Transform3DInverseTransformTest, TransformDirection_ThenInverse_RoundTrips)
{
	Transform3D t;
	t.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg45));
	t.SetLocalScale(Vector3D(2.0f, 3.0f, 4.0f));

	Vector3D localDirection(1.0f, 0.0f, 0.0f);

	Vector3D worldDirection = t.TransformDirection(localDirection);
	Vector3D backToLocal = t.InverseTransformDirection(worldDirection);

	ExpectVec3Near(backToLocal, localDirection);
}

// ==============================================================================
// Transform3DCycleDetectionTest
// ==============================================================================

TEST(Transform3DCycleDetectionTest, SetParent_NonCycle_Succeeds)
{
	Transform3D a;
	Transform3D b;

	b.SetParent(&a);

	EXPECT_EQ(b.GetParent(), &a);
}

// Note: Cycle detection via DIA_ASSERT is debug-only and not covered by gtest death tests.
// The positive path (non-cycle parent) is tested above.

// ==============================================================================
// Transform3DCopyConstructorTest
// ==============================================================================

TEST(Transform3DCopyConstructorTest, CopyConstructor_CopiesAllMembers)
{
	Transform3D parent;

	Transform3D original;
	original.SetLocalPosition(Vector3D(1.0f, 2.0f, 3.0f));
	original.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::YAxis(), Angle::Deg45));
	original.SetLocalScale(Vector3D(2.0f, 3.0f, 4.0f));
	original.SetParent(&parent);

	Transform3D copy(original);

	ExpectVec3Near(copy.GetLocalPosition(), original.GetLocalPosition());
	ExpectQuaternionNear(copy.GetLocalRotation(), original.GetLocalRotation());
	ExpectVec3Near(copy.GetLocalScale(), original.GetLocalScale());
	EXPECT_EQ(copy.GetParent(), original.GetParent());
}

TEST(Transform3DCopyConstructorTest, AssignmentOperator_CopiesAllMembers)
{
	Transform3D parent;

	Transform3D original;
	original.SetLocalPosition(Vector3D(5.0f, 10.0f, 15.0f));
	original.SetLocalRotation(Quaternion::FromAxisAngle(Vector3D::XAxis(), Angle::Deg30));
	original.SetLocalScale(Vector3D(1.5f, 2.5f, 3.5f));
	original.SetParent(&parent);

	Transform3D copy;
	copy = original;

	ExpectVec3Near(copy.GetLocalPosition(), original.GetLocalPosition());
	ExpectQuaternionNear(copy.GetLocalRotation(), original.GetLocalRotation());
	ExpectVec3Near(copy.GetLocalScale(), original.GetLocalScale());
	EXPECT_EQ(copy.GetParent(), original.GetParent());
}
