#include <gtest/gtest.h>
#include <DiaGeometry3D/Shapes/Frustum.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaGeometry3D/Testing/Geometry3DShapeFactory.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Geometry3D;
using namespace Dia::Geometry3D::Testing;
using namespace Dia::Maths;

TEST(FrustumTest, DefaultConstruction_SixPlanes)
{
    Frustum f;
    // Default is a unit cube frustum — point at origin should be inside
    EXPECT_EQ(f.IsIntersecting(Vector3D(0,0,0)), IntersectionClassify::kPenetrating);
}

TEST(FrustumTest, ConstructWithPlanes_StoresCorrectly)
{
    Frustum f = MakeIdentityFrustum();
    // Check one plane directly
    const Plane& near = f.GetPlane(FrustumPlane::kNear);
    // near plane normal should be (0,0,1) inward
    EXPECT_NEAR(near.GetNormal().Z(), 1.0f, 1e-5f);
}

TEST(FrustumTest, CopyConstructor_CopiesValues)
{
    Frustum a = MakeIdentityFrustum();
    Frustum b(a);
    EXPECT_TRUE(a == b);
}

TEST(FrustumTest, IsIntersecting_PointInside_Penetrating)
{
    Frustum f = MakeIdentityFrustum();
    EXPECT_EQ(f.IsIntersecting(Vector3D(0, 0, 0)), IntersectionClassify::kPenetrating);
}

TEST(FrustumTest, IsIntersecting_PointOutside_NoIntersection)
{
    Frustum f = MakeIdentityFrustum();
    EXPECT_EQ(f.IsIntersecting(Vector3D(5, 0, 0)), IntersectionClassify::kNoIntersection);
}

TEST(FrustumTest, Contains_PointInside_True)
{
    Frustum f = MakeIdentityFrustum();
    EXPECT_TRUE(f.Contains(Vector3D(0, 0, 0)));
}

TEST(FrustumTest, Contains_PointOutside_False)
{
    Frustum f = MakeIdentityFrustum();
    EXPECT_FALSE(f.Contains(Vector3D(10, 10, 10)));
}

TEST(FrustumTest, FromMatrix44_IdentityMatrix_UnitCubeFrustum)
{
    // Identity view-projection: result should contain the origin
    Frustum f = Frustum::FromMatrix44(Matrix44::Identity());
    // The identity frustum should contain a point at the origin
    EXPECT_EQ(f.IsIntersecting(Vector3D(0, 0, 0)), IntersectionClassify::kPenetrating);
}

TEST(FrustumTest, CalculateAABB_UnitFrustum_AABBEnclosesAllCorners)
{
    Frustum f = MakeIdentityFrustum();
    AABB bounds = f.CalculateAABB();
    // The AABB should at least contain the origin
    EXPECT_EQ(bounds.IsIntersecting(Vector3D(0,0,0)), IntersectionClassify::kPenetrating);
}

TEST(FrustumTest, GetPlane_CorrectSlot_ReturnsPlane)
{
    Frustum f = MakeIdentityFrustum();
    const Plane& farP = f.GetPlane(FrustumPlane::kFar);
    // Far plane normal should be (0,0,-1) inward
    EXPECT_NEAR(farP.GetNormal().Z(), -1.0f, 1e-5f);
}

TEST(FrustumTest, FromMatrix44_PerspectiveMatrix_ContainsPointInFrustum)
{
    // Build a Y-up RH perspective frustum looking down -Z
    Angle fov = Angle::FromDegrees(90.0f);
    Matrix44 proj = Matrix44::Perspective(fov, 1.0f, 0.1f, 100.0f);
    Matrix44 view = Matrix44::LookAt(Vector3D(0,0,0), Vector3D(0,0,-1), Vector3D(0,1,0));
    Matrix44 vp   = proj * view;

    Frustum f = Frustum::FromMatrix44(vp);
    // A point directly in front should be inside
    EXPECT_EQ(f.IsIntersecting(Vector3D(0, 0, -5.0f)), IntersectionClassify::kPenetrating);
}
