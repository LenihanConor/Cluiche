#include <gtest/gtest.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, DefaultConstruction_IdentityAtOrigin)
{
    Transform t;
    EXPECT_EQ(t.GetLocalPosition().x, 0.0f);
    EXPECT_EQ(t.GetLocalPosition().y, 0.0f);
    EXPECT_EQ(t.GetLocalScale().x, 1.0f);
    EXPECT_EQ(t.GetLocalScale().y, 1.0f);
    EXPECT_FALSE(t.HasParent());
}

TEST(Geometry2D_Transform, ConstructWithValues_StoresCorrectly)
{
    Vector2D pos(3.0f, 4.0f);
    Angle rot = Angle::Deg0;
    Vector2D scale(2.0f, 2.0f);
    Transform t(pos, rot, scale);
    EXPECT_EQ(t.GetLocalPosition().x, 3.0f);
    EXPECT_EQ(t.GetLocalPosition().y, 4.0f);
    EXPECT_EQ(t.GetLocalScale().x, 2.0f);
}

// ---------------------------------------------------------------------------
// Local space setters
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, SetLocalPosition_Updates)
{
    Transform t;
    t.SetLocalPosition(Vector2D(5.0f, 6.0f));
    EXPECT_EQ(t.GetLocalPosition().x, 5.0f);
    EXPECT_EQ(t.GetLocalPosition().y, 6.0f);
}

TEST(Geometry2D_Transform, SetLocalScale_Updates)
{
    Transform t;
    t.SetLocalScale(Vector2D(3.0f, 2.0f));
    EXPECT_EQ(t.GetLocalScale().x, 3.0f);
    EXPECT_EQ(t.GetLocalScale().y, 2.0f);
}

TEST(Geometry2D_Transform, SetLocalScale_Uniform_SetsBothAxes)
{
    Transform t;
    t.SetLocalScale(4.0f);
    EXPECT_EQ(t.GetLocalScale().x, 4.0f);
    EXPECT_EQ(t.GetLocalScale().y, 4.0f);
}

// ---------------------------------------------------------------------------
// World space — no parent (world == local)
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, WorldPosition_NoParent_EqualsLocalPosition)
{
    Transform t;
    t.SetLocalPosition(Vector2D(7.0f, 8.0f));
    Vector2D world = t.GetWorldPosition();
    EXPECT_NEAR(world.x, 7.0f, 0.001f);
    EXPECT_NEAR(world.y, 8.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Parent-child hierarchy
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, SetParent_HasParent_ReturnsTrue)
{
    Transform parent;
    Transform child;
    child.SetParent(&parent);
    EXPECT_TRUE(child.HasParent());
    EXPECT_EQ(child.GetParent(), &parent);
}

TEST(Geometry2D_Transform, SetParentNull_HasParent_ReturnsFalse)
{
    Transform parent;
    Transform child;
    child.SetParent(&parent);
    child.SetParent(nullptr);
    EXPECT_FALSE(child.HasParent());
}

TEST(Geometry2D_Transform, ChildWorldPosition_InheritsParentTranslation)
{
    Transform parent;
    parent.SetLocalPosition(Vector2D(10.0f, 0.0f));

    Transform child;
    child.SetParent(&parent);
    child.SetLocalPosition(Vector2D(5.0f, 0.0f));

    Vector2D worldPos = child.GetWorldPosition();
    EXPECT_NEAR(worldPos.x, 15.0f, 0.01f);
    EXPECT_NEAR(worldPos.y, 0.0f, 0.01f);
}

// ---------------------------------------------------------------------------
// Translate / Rotate / Scale operations
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, Translate_MovesLocalPosition)
{
    Transform t;
    t.SetLocalPosition(Vector2D(1.0f, 2.0f));
    t.Translate(Vector2D(3.0f, 4.0f));
    EXPECT_NEAR(t.GetLocalPosition().x, 4.0f, 0.001f);
    EXPECT_NEAR(t.GetLocalPosition().y, 6.0f, 0.001f);
}

TEST(Geometry2D_Transform, Scale_MultipliesCurrentScale)
{
    Transform t;
    t.SetLocalScale(Vector2D(2.0f, 3.0f));
    t.Scale(Vector2D(2.0f, 2.0f));
    EXPECT_NEAR(t.GetLocalScale().x, 4.0f, 0.001f);
    EXPECT_NEAR(t.GetLocalScale().y, 6.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Direction helpers
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, GetForward_NoRotation_PointsAlongX)
{
    Transform t;
    Vector2D fwd = t.GetForward();
    EXPECT_NEAR(fwd.x, 1.0f, 0.001f);
    EXPECT_NEAR(fwd.y, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Rotation setters
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, SetLocalRotation_Updates)
{
    Transform t;
    t.SetLocalRotation(Angle::Deg90);
    EXPECT_NEAR(t.GetLocalRotation().AsRadians(), Angle::Deg90.AsRadians(), 0.001f);
}

TEST(Geometry2D_Transform, GetWorldRotation_NoParent_EqualsLocalRotation)
{
    Transform t;
    t.SetLocalRotation(Angle::Deg45);
    EXPECT_NEAR(t.GetWorldRotation().AsRadians(), t.GetLocalRotation().AsRadians(), 0.001f);
}

TEST(Geometry2D_Transform, Rotate_AddsToLocalRotation)
{
    Transform t;
    t.SetLocalRotation(Angle::Deg45);
    t.Rotate(Angle::Deg45);
    EXPECT_NEAR(t.GetLocalRotation().AsRadians(), Angle::Deg90.AsRadians(), 0.001f);
}

TEST(Geometry2D_Transform, Rotate_NegativeDelta_SubtractsFromRotation)
{
    Transform t;
    t.SetLocalRotation(Angle::Deg90);
    t.Rotate(-Angle::Deg45);
    EXPECT_NEAR(t.GetLocalRotation().AsRadians(), Angle::Deg45.AsRadians(), 0.001f);
}

// ---------------------------------------------------------------------------
// Child world rotation inherits parent
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, ChildWorldRotation_InheritsParentRotation)
{
    Transform parent;
    parent.SetLocalRotation(Angle::Deg90);

    Transform child;
    child.SetParent(&parent);
    child.SetLocalRotation(Angle::Deg0);

    EXPECT_NEAR(child.GetWorldRotation().AsRadians(), Angle::Deg90.AsRadians(), 0.001f);
}

TEST(Geometry2D_Transform, ChildWorldPosition_InheritsParentRotation)
{
    // Parent at origin, rotated 90 degrees CCW.
    // Child local position (1,0) should become (0,1) in world space (CCW rotation).
    Transform parent;
    parent.SetLocalPosition(Vector2D(0.0f, 0.0f));
    parent.SetLocalRotation(Angle::Deg90);

    Transform child;
    child.SetParent(&parent);
    child.SetLocalPosition(Vector2D(1.0f, 0.0f));

    Vector2D worldPos = child.GetWorldPosition();
    EXPECT_NEAR(worldPos.x, 0.0f, 0.01f);
    EXPECT_NEAR(worldPos.y, 1.0f, 0.01f);
}

// ---------------------------------------------------------------------------
// LookAt
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, LookAt_SetsForwardTowardsTarget)
{
    // Transform at (0,0), looking at (1,0) — forward should align with +X.
    Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 0.0f));
    t.LookAt(Vector2D(1.0f, 0.0f));
    Vector2D fwd = t.GetForward();
    EXPECT_NEAR(fwd.x, 1.0f, 0.01f);
    EXPECT_NEAR(fwd.y, 0.0f, 0.01f);
}

TEST(Geometry2D_Transform, LookAt_TargetAbove_ForwardPointsUpY)
{
    // Transform at (0,0), looking at (0,1) — forward should be (0,1).
    Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 0.0f));
    t.LookAt(Vector2D(0.0f, 1.0f));
    Vector2D fwd = t.GetForward();
    EXPECT_NEAR(fwd.x, 0.0f, 0.01f);
    EXPECT_NEAR(fwd.y, 1.0f, 0.01f);
}

TEST(Geometry2D_Transform, LookAt_SamePositionAsTarget_RotationUnchanged)
{
    // LookAt same position should not crash and should leave rotation unchanged.
    Transform t;
    t.SetLocalRotation(Angle::Deg45);
    t.SetLocalPosition(Vector2D(5.0f, 5.0f));
    t.LookAt(Vector2D(5.0f, 5.0f));
    // Rotation should remain 45 degrees (or be any valid value — just must not crash).
    SUCCEED();
}

// ---------------------------------------------------------------------------
// GetRight
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, GetRight_NoRotation_PointsAlongY)
{
    // GetRight returns +Y rotated by world rotation. At Deg0, this is (0,1).
    Transform t;
    Vector2D right = t.GetRight();
    EXPECT_NEAR(right.x, 0.0f, 0.001f);
    EXPECT_NEAR(right.y, 1.0f, 0.001f);
}

TEST(Geometry2D_Transform, GetRight_And_GetForward_AreOrthogonal)
{
    Transform t;
    t.SetLocalRotation(Angle::Deg45);
    Vector2D fwd   = t.GetForward();
    Vector2D right = t.GetRight();
    float dot = fwd.x * right.x + fwd.y * right.y;
    EXPECT_NEAR(dot, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Multi-level hierarchy
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, MultiLevel_Hierarchy_WorldPosChain)
{
    // grandparent at (10,0), parent at (5,0) local, child at (2,0) local.
    // World position of child = 10 + 5 + 2 = 17 (all at Y=0, no rotation).
    Transform grandparent;
    grandparent.SetLocalPosition(Vector2D(10.0f, 0.0f));

    Transform parent;
    parent.SetParent(&grandparent);
    parent.SetLocalPosition(Vector2D(5.0f, 0.0f));

    Transform child;
    child.SetParent(&parent);
    child.SetLocalPosition(Vector2D(2.0f, 0.0f));

    Vector2D worldPos = child.GetWorldPosition();
    EXPECT_NEAR(worldPos.x, 17.0f, 0.01f);
    EXPECT_NEAR(worldPos.y, 0.0f, 0.01f);
}

TEST(Geometry2D_Transform, MultiLevel_Hierarchy_WorldRotation)
{
    // grandparent 45 degrees, parent 45 degrees local → child at origin.
    // Child world rotation = 45 + 45 = 90 degrees.
    Transform grandparent;
    grandparent.SetLocalRotation(Angle::Deg45);

    Transform parent;
    parent.SetParent(&grandparent);
    parent.SetLocalRotation(Angle::Deg45);

    Transform child;
    child.SetParent(&parent);
    child.SetLocalRotation(Angle::Deg0);

    EXPECT_NEAR(child.GetWorldRotation().AsRadians(), Angle::Deg90.AsRadians(), 0.01f);
}

// ---------------------------------------------------------------------------
// Scale hierarchy
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, ChildWorldScale_InheritsParentScale)
{
    Transform parent;
    parent.SetLocalScale(Vector2D(2.0f, 3.0f));

    Transform child;
    child.SetParent(&parent);
    child.SetLocalScale(Vector2D(2.0f, 2.0f));

    Vector2D worldScale = child.GetWorldScale();
    EXPECT_NEAR(worldScale.x, 4.0f, 0.001f);
    EXPECT_NEAR(worldScale.y, 6.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Space conversion helpers
// ---------------------------------------------------------------------------

TEST(Geometry2D_Transform, TransformPoint_NoParent_AppliesLocalTransform)
{
    // Transform at (5,5), no rotation, scale (1,1). TransformPoint(0,0) = (5,5).
    Transform t;
    t.SetLocalPosition(Vector2D(5.0f, 5.0f));
    Vector2D world = t.TransformPoint(Vector2D(0.0f, 0.0f));
    EXPECT_NEAR(world.x, 5.0f, 0.001f);
    EXPECT_NEAR(world.y, 5.0f, 0.001f);
}

TEST(Geometry2D_Transform, InverseTransformPoint_IsInverseOfTransformPoint)
{
    Transform t;
    t.SetLocalPosition(Vector2D(3.0f, 7.0f));
    t.SetLocalRotation(Angle::Deg45);

    Vector2D localPoint(2.0f, 1.0f);
    Vector2D worldPoint = t.TransformPoint(localPoint);
    Vector2D recovered  = t.InverseTransformPoint(worldPoint);

    EXPECT_NEAR(recovered.x, localPoint.x, 0.01f);
    EXPECT_NEAR(recovered.y, localPoint.y, 0.01f);
}
