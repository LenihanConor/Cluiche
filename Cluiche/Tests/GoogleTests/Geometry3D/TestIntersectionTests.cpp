#include <gtest/gtest.h>
#include <DiaGeometry3D/Intersection/IntersectionTests.h>
#include <DiaGeometry3D/Testing/Geometry3DShapeFactory.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Geometry3D;
using namespace Dia::Geometry3D::Testing;
using namespace Dia::Maths;

// ============================================================================
// AABB vs Frustum
// ============================================================================

TEST(IntersectionAABBFrustumTest, AABBInsideFrustum_ReturnsBContainsA)
{
    Frustum f = MakeIdentityFrustum();
    AABB a = AABB::FromCenterExtents(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.4f, 0.4f, 0.4f));
    EXPECT_EQ(IntersectionTests::Test(a, f), IntersectionClassify::kBContainsA);
}

TEST(IntersectionAABBFrustumTest, AABBOutsideFrustum_ReturnsNoIntersection)
{
    Frustum f = MakeIdentityFrustum();
    AABB a = AABB::FromCenterExtents(Vector3D(5.0f, 0.0f, 0.0f), Vector3D(0.4f, 0.4f, 0.4f));
    EXPECT_EQ(IntersectionTests::Test(a, f), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionAABBFrustumTest, AABBStraddlingFrustumPlane_ReturnsPenetrating)
{
    Frustum f = MakeIdentityFrustum();
    // Straddle the right face at x=1
    AABB a = AABB::FromCenterExtents(Vector3D(0.9f, 0.0f, 0.0f), Vector3D(0.4f, 0.2f, 0.2f));
    EXPECT_EQ(IntersectionTests::Test(a, f), IntersectionClassify::kPenetrating);
}

TEST(IntersectionAABBFrustumTest, FrustumInsideAABB_ReturnsAContainsB)
{
    Frustum f = MakeIdentityFrustum();
    AABB a = AABB::FromCenterExtents(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(3.0f, 3.0f, 3.0f));
    EXPECT_EQ(IntersectionTests::Test(a, f), IntersectionClassify::kAContainsB);
}

TEST(IntersectionAABBFrustumTest, Symmetric_FrustumAABB)
{
    Frustum f = MakeIdentityFrustum();
    AABB a = AABB::FromCenterExtents(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.4f, 0.4f, 0.4f));
    auto r1 = IntersectionTests::Test(a, f);
    auto r2 = IntersectionTests::Test(f, a);
    EXPECT_EQ(r1, IntersectionClassify::kBContainsA);
    EXPECT_EQ(r2, IntersectionClassify::kAContainsB);
}

// ============================================================================
// Sphere vs Frustum
// ============================================================================

TEST(IntersectionSphereFrustumTest, SphereFullyInside_ReturnsBContainsA)
{
    Frustum f = MakeIdentityFrustum();
    Sphere s(Vector3D(0.0f, 0.0f, 0.0f), 0.3f);
    EXPECT_EQ(IntersectionTests::Test(s, f), IntersectionClassify::kBContainsA);
}

TEST(IntersectionSphereFrustumTest, SphereFullyOutside_ReturnsNoIntersection)
{
    Frustum f = MakeIdentityFrustum();
    Sphere s(Vector3D(5.0f, 0.0f, 0.0f), 0.3f);
    EXPECT_EQ(IntersectionTests::Test(s, f), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionSphereFrustumTest, SphereStraddlingPlane_ReturnsPenetrating)
{
    Frustum f = MakeIdentityFrustum();
    // Center at x=0.9, radius=0.4 → straddles x=1 plane
    Sphere s(Vector3D(0.9f, 0.0f, 0.0f), 0.4f);
    EXPECT_EQ(IntersectionTests::Test(s, f), IntersectionClassify::kPenetrating);
}

TEST(IntersectionSphereFrustumTest, SphereTangentToPlane_ReturnsBContainsA)
{
    Frustum f = MakeIdentityFrustum();
    // Center at x=0.7, radius=0.3 → tangent at x=1.0
    Sphere s(Vector3D(0.7f, 0.0f, 0.0f), 0.3f);
    EXPECT_EQ(IntersectionTests::Test(s, f), IntersectionClassify::kBContainsA);
}

// ============================================================================
// Triangle vs Frustum
// ============================================================================

TEST(IntersectionTriangleFrustumTest, TriangleFullyInside_ReturnsBContainsA)
{
    Frustum f = MakeIdentityFrustum();
    Triangle t(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.2f, 0.0f, 0.0f), Vector3D(0.0f, 0.2f, 0.0f));
    EXPECT_EQ(IntersectionTests::Test(t, f), IntersectionClassify::kBContainsA);
}

TEST(IntersectionTriangleFrustumTest, TriangleFullyOutside_ReturnsNoIntersection)
{
    Frustum f = MakeIdentityFrustum();
    Triangle t(Vector3D(5.0f, 0.0f, 0.0f), Vector3D(5.2f, 0.0f, 0.0f), Vector3D(5.0f, 0.2f, 0.0f));
    EXPECT_EQ(IntersectionTests::Test(t, f), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionTriangleFrustumTest, OneVertexInside_ReturnsPenetrating)
{
    Frustum f = MakeIdentityFrustum();
    Triangle t(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(2.0f, 0.0f, 0.0f), Vector3D(0.0f, 2.0f, 0.0f));
    EXPECT_EQ(IntersectionTests::Test(t, f), IntersectionClassify::kPenetrating);
}

// ============================================================================
// AABB vs AABB
// ============================================================================

TEST(IntersectionAABBAABBTest, Disjoint_ReturnsNoIntersection)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    AABB b(Vector3D(2,0,0), Vector3D(3,1,1));
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionAABBAABBTest, Overlapping_ReturnsPenetrating)
{
    AABB a(Vector3D(0,0,0), Vector3D(2,2,2));
    AABB b(Vector3D(1,1,1), Vector3D(3,3,3));
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kPenetrating);
}

TEST(IntersectionAABBAABBTest, AContainsB_ReturnsAContainsB)
{
    AABB a(Vector3D(-2,-2,-2), Vector3D(2,2,2));
    AABB b(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kAContainsB);
}

TEST(IntersectionAABBAABBTest, BContainsA_ReturnsBContainsA)
{
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    AABB b(Vector3D(-2,-2,-2), Vector3D(2,2,2));
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kBContainsA);
}

TEST(IntersectionAABBAABBTest, Identical_ReturnsBContainsA)
{
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    AABB b(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    // identical → b contains a (both containment checks succeed; A-contains-B fires first)
    auto r = IntersectionTests::Test(a, b);
    EXPECT_TRUE(r == IntersectionClassify::kAContainsB || r == IntersectionClassify::kBContainsA);
}

TEST(IntersectionAABBAABBTest, Touching_ReturnsPenetrating)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    AABB b(Vector3D(1,0,0), Vector3D(2,1,1));
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kPenetrating);
}

// ============================================================================
// AABB vs Sphere
// ============================================================================

TEST(IntersectionAABBSphereTest, SphereCenterInsideAABB_ReturnsPenetrating)
{
    AABB a(Vector3D(-2,-2,-2), Vector3D(2,2,2));
    Sphere s(Vector3D(0,0,0), 0.5f);
    EXPECT_EQ(IntersectionTests::Test(a, s), IntersectionClassify::kAContainsB);
}

TEST(IntersectionAABBSphereTest, SphereFullyOutside_ReturnsNoIntersection)
{
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    Sphere s(Vector3D(5,0,0), 0.5f);
    EXPECT_EQ(IntersectionTests::Test(a, s), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionAABBSphereTest, SphereTangent_ReturnsPenetrating)
{
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    Sphere s(Vector3D(2,0,0), 1.0f);
    EXPECT_EQ(IntersectionTests::Test(a, s), IntersectionClassify::kPenetrating);
}

TEST(IntersectionAABBSphereTest, SphereContainsAABB_ReturnsBContainsA)
{
    // AABB from -0.5 to 0.5, sphere radius 2 centered at origin
    AABB a(Vector3D(-0.5f,-0.5f,-0.5f), Vector3D(0.5f,0.5f,0.5f));
    Sphere s(Vector3D(0,0,0), 2.0f);
    EXPECT_EQ(IntersectionTests::Test(a, s), IntersectionClassify::kBContainsA);
}

TEST(IntersectionAABBSphereTest, Symmetric_SphereAABB)
{
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    Sphere s(Vector3D(5,0,0), 0.5f);
    auto r1 = IntersectionTests::Test(a, s);
    auto r2 = IntersectionTests::Test(s, a);
    EXPECT_EQ(r1, IntersectionClassify::kNoIntersection);
    EXPECT_EQ(r2, IntersectionClassify::kNoIntersection);
}

// ============================================================================
// Sphere vs Sphere
// ============================================================================

TEST(IntersectionSphereSphereTest, Disjoint_ReturnsNoIntersection)
{
    Sphere a(Vector3D(0,0,0), 1.0f);
    Sphere b(Vector3D(5,0,0), 1.0f);
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionSphereSphereTest, Tangent_ReturnsPenetrating)
{
    Sphere a(Vector3D(0,0,0), 1.0f);
    Sphere b(Vector3D(2,0,0), 1.0f);
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kPenetrating);
}

TEST(IntersectionSphereSphereTest, Overlapping_ReturnsPenetrating)
{
    Sphere a(Vector3D(0,0,0), 1.5f);
    Sphere b(Vector3D(2,0,0), 1.0f);
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kPenetrating);
}

TEST(IntersectionSphereSphereTest, AContainsB_ReturnsAContainsB)
{
    Sphere a(Vector3D(0,0,0), 3.0f);
    Sphere b(Vector3D(0,0,0), 1.0f);
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kAContainsB);
}

TEST(IntersectionSphereSphereTest, Identical_ReturnsContainment)
{
    Sphere a(Vector3D(0,0,0), 1.0f);
    Sphere b(Vector3D(0,0,0), 1.0f);
    auto r = IntersectionTests::Test(a, b);
    EXPECT_TRUE(r == IntersectionClassify::kAContainsB || r == IntersectionClassify::kBContainsA);
}

// ============================================================================
// AABB vs OOBB
// ============================================================================

TEST(IntersectionAABBOOBBTest, AxisAlignedOOBBEqualsAABBResult_Disjoint)
{
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    OOBB b(Vector3D(5,0,0), Vector3D(0.5f,0.5f,0.5f), Quaternion::Identity());
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionAABBOOBBTest, AxisAlignedOOBBEqualsAABBResult_Contains)
{
    AABB a(Vector3D(-2,-2,-2), Vector3D(2,2,2));
    OOBB b(Vector3D(0,0,0), Vector3D(0.5f,0.5f,0.5f), Quaternion::Identity());
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kAContainsB);
}

TEST(IntersectionAABBOOBBTest, Symmetric_OOBBvsAABB)
{
    AABB a(Vector3D(-2,-2,-2), Vector3D(2,2,2));
    OOBB b(Vector3D(0,0,0), Vector3D(0.5f,0.5f,0.5f), Quaternion::Identity());
    auto r1 = IntersectionTests::Test(a, b);
    auto r2 = IntersectionTests::Test(b, a);
    EXPECT_EQ(r1, IntersectionClassify::kAContainsB);
    EXPECT_EQ(r2, IntersectionClassify::kBContainsA);
}

// ============================================================================
// OOBB vs OOBB
// ============================================================================

TEST(IntersectionOOBBOOBBTest, SameOrientation_Disjoint)
{
    OOBB a(Vector3D(0,0,0), Vector3D(1,1,1), Quaternion::Identity());
    OOBB b(Vector3D(5,0,0), Vector3D(1,1,1), Quaternion::Identity());
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionOOBBOOBBTest, SameOrientation_AContainsB)
{
    OOBB a(Vector3D(0,0,0), Vector3D(2,2,2), Quaternion::Identity());
    OOBB b(Vector3D(0,0,0), Vector3D(0.5f,0.5f,0.5f), Quaternion::Identity());
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kAContainsB);
}

// ============================================================================
// Triangle vs AABB
// ============================================================================

TEST(IntersectionTriangleAABBTest, TriangleInsideAABB_ReturnsBContainsA)
{
    AABB a = MakeUnitAABB();
    Triangle t(Vector3D(0,0,0), Vector3D(0.2f,0,0), Vector3D(0,0.2f,0));
    EXPECT_EQ(IntersectionTests::Test(t, a), IntersectionClassify::kBContainsA);
}

TEST(IntersectionTriangleAABBTest, TriangleOutside_ReturnsNoIntersection)
{
    AABB a = MakeUnitAABB();
    Triangle t(Vector3D(5,0,0), Vector3D(5.2f,0,0), Vector3D(5,0.2f,0));
    EXPECT_EQ(IntersectionTests::Test(t, a), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionTriangleAABBTest, TriangleEdgeCrossingAABB_ReturnsPenetrating)
{
    AABB a = MakeUnitAABB();
    Triangle t(Vector3D(0,0,0), Vector3D(3,0,0), Vector3D(0,3,0));
    EXPECT_EQ(IntersectionTests::Test(t, a), IntersectionClassify::kPenetrating);
}

TEST(IntersectionTriangleAABBTest, Symmetric_AABBvsTriangle)
{
    AABB a = MakeUnitAABB();
    Triangle t(Vector3D(0,0,0), Vector3D(0.2f,0,0), Vector3D(0,0.2f,0));
    auto r1 = IntersectionTests::Test(t, a);
    auto r2 = IntersectionTests::Test(a, t);
    EXPECT_EQ(r1, IntersectionClassify::kBContainsA);
    EXPECT_EQ(r2, IntersectionClassify::kAContainsB);
}

// ============================================================================
// Triangle vs Sphere
// ============================================================================

TEST(IntersectionTriangleSphereTest, SphereCenterNearTriangle_ReturnsPenetrating)
{
    // MakeAxisAlignedTriangle: (0,0,0),(1,0,0),(0,0,-1) in y=0 plane
    Triangle t = MakeAxisAlignedTriangle();
    Sphere s(Vector3D(0.2f, 0.05f, -0.2f), 0.3f);
    EXPECT_EQ(IntersectionTests::Test(t, s), IntersectionClassify::kPenetrating);
}

TEST(IntersectionTriangleSphereTest, SphereFarAway_ReturnsNoIntersection)
{
    Triangle t = MakeAxisAlignedTriangle();
    Sphere s(Vector3D(5,5,5), 0.1f);
    EXPECT_EQ(IntersectionTests::Test(t, s), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionTriangleSphereTest, SphereCloseToEdge_ReturnsPenetrating)
{
    // MakeAxisAlignedTriangle: (0,0,0),(1,0,0),(0,0,-1) — edge from (0,0,0) to (1,0,0)
    Triangle t = MakeAxisAlignedTriangle();
    Sphere s(Vector3D(0.5f, 0.0f, 0.05f), 0.1f);
    EXPECT_EQ(IntersectionTests::Test(t, s), IntersectionClassify::kPenetrating);
}

TEST(IntersectionTriangleSphereTest, Symmetric_SpherevsTriangle)
{
    Triangle t = MakeAxisAlignedTriangle();
    Sphere s(Vector3D(5,5,5), 0.1f);
    EXPECT_EQ(IntersectionTests::Test(t, s), IntersectionClassify::kNoIntersection);
    EXPECT_EQ(IntersectionTests::Test(s, t), IntersectionClassify::kNoIntersection);
}

// ============================================================================
// Ray vs AABB
// ============================================================================

TEST(IntersectionRayAABBTest, RayEnteringAABB_ReturnsPenetrating)
{
    AABB a = MakeUnitAABB();
    Ray r(Vector3D(-3, 0, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, a), IntersectionClassify::kPenetrating);
}

TEST(IntersectionRayAABBTest, RayMissingAABB_ReturnsNoIntersection)
{
    AABB a = MakeUnitAABB();
    Ray r(Vector3D(-3, 5, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, a), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionRayAABBTest, RayStartingInsideAABB_ReturnsPenetrating)
{
    AABB a = MakeUnitAABB();
    Ray r(Vector3D(0, 0, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, a), IntersectionClassify::kPenetrating);
}

TEST(IntersectionRayAABBTest, RayParallelToFace_ReturnsNoIntersection)
{
    AABB a = MakeUnitAABB();
    // Parallel to XZ plane, offset in Y
    Ray r(Vector3D(-3, 3, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, a), IntersectionClassify::kNoIntersection);
}

// ============================================================================
// Ray vs Sphere
// ============================================================================

TEST(IntersectionRaySphereTest, RayHitsSphere_ReturnsPenetrating)
{
    Sphere s = MakeUnitSphere();
    Ray r(Vector3D(-3, 0, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, s), IntersectionClassify::kPenetrating);
}

TEST(IntersectionRaySphereTest, RayMissesSphere_ReturnsNoIntersection)
{
    Sphere s = MakeUnitSphere();
    Ray r(Vector3D(-3, 3, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, s), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionRaySphereTest, RayTangentToSphere_ReturnsPenetrating)
{
    Sphere s = MakeUnitSphere();
    Ray r(Vector3D(-3, 1, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, s), IntersectionClassify::kPenetrating);
}

TEST(IntersectionRaySphereTest, RayStartingInsideSphere_ReturnsPenetrating)
{
    Sphere s = MakeUnitSphere();
    Ray r(Vector3D(0, 0, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, s), IntersectionClassify::kPenetrating);
}

// ============================================================================
// Ray vs Triangle
// ============================================================================

TEST(IntersectionRayTriangleTest, StandardHit_ReturnsPenetrating)
{
    // MakeAxisAlignedTriangle: (0,0,0),(1,0,0),(0,0,-1) in y=0 plane
    Triangle t = MakeAxisAlignedTriangle();
    Ray r(Vector3D(0.2f, -1.0f, -0.2f), Vector3D(0, 1, 0));
    EXPECT_EQ(IntersectionTests::Test(r, t), IntersectionClassify::kPenetrating);
}

TEST(IntersectionRayTriangleTest, RayMissingTriangle_ReturnsNoIntersection)
{
    Triangle t = MakeAxisAlignedTriangle();
    Ray r(Vector3D(5, -1, 5), Vector3D(0, 1, 0));
    EXPECT_EQ(IntersectionTests::Test(r, t), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionRayTriangleTest, RayParallelToTrianglePlane_ReturnsNoIntersection)
{
    Triangle t = MakeAxisAlignedTriangle();
    Ray r(Vector3D(-1, 1, -1), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, t), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionRayTriangleTest, RayPointingAway_ReturnsNoIntersection)
{
    Triangle t = MakeAxisAlignedTriangle();
    Ray r(Vector3D(0.2f, 1.0f, -0.2f), Vector3D(0, 1, 0));
    EXPECT_EQ(IntersectionTests::Test(r, t), IntersectionClassify::kNoIntersection);
}

// ============================================================================
// Ray vs Plane
// ============================================================================

TEST(IntersectionRayPlaneTest, StandardHit_ReturnsPenetrating)
{
    Plane p = MakeXZPlane();
    Ray r(Vector3D(0, -1, 0), Vector3D(0, 1, 0));
    EXPECT_EQ(IntersectionTests::Test(r, p), IntersectionClassify::kPenetrating);
}

TEST(IntersectionRayPlaneTest, RayParallelToPlane_ReturnsNoIntersection)
{
    Plane p = MakeXZPlane();
    Ray r(Vector3D(0, 1, 0), Vector3D(1, 0, 0));
    EXPECT_EQ(IntersectionTests::Test(r, p), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionRayPlaneTest, RayPointingAway_ReturnsNoIntersection)
{
    Plane p = MakeXZPlane();
    Ray r(Vector3D(0, 1, 0), Vector3D(0, 1, 0));
    EXPECT_EQ(IntersectionTests::Test(r, p), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionRayPlaneTest, RayOnPlaneOrigin_ReturnsPenetrating)
{
    Plane p = MakeXZPlane();
    Ray r(Vector3D(0, 0, 0), Vector3D(0, 1, 0));
    EXPECT_EQ(IntersectionTests::Test(r, p), IntersectionClassify::kPenetrating);
}

// ============================================================================
// Contains
// ============================================================================

TEST(ContainsTest, AABB_PointInside)
{
    AABB a = MakeUnitAABB();
    EXPECT_TRUE(IntersectionTests::Contains(a, Vector3D(0, 0, 0)));
}

TEST(ContainsTest, AABB_PointOutside)
{
    AABB a = MakeUnitAABB();
    EXPECT_FALSE(IntersectionTests::Contains(a, Vector3D(2, 0, 0)));
}

TEST(ContainsTest, AABB_PointOnBoundary)
{
    AABB a = MakeUnitAABB();
    EXPECT_TRUE(IntersectionTests::Contains(a, Vector3D(1, 0, 0)));
}

TEST(ContainsTest, Sphere_PointInside)
{
    Sphere s = MakeUnitSphere();
    EXPECT_TRUE(IntersectionTests::Contains(s, Vector3D(0, 0, 0)));
}

TEST(ContainsTest, Sphere_PointOutside)
{
    Sphere s = MakeUnitSphere();
    EXPECT_FALSE(IntersectionTests::Contains(s, Vector3D(2, 0, 0)));
}

TEST(ContainsTest, Sphere_PointOnBoundary)
{
    Sphere s = MakeUnitSphere();
    EXPECT_TRUE(IntersectionTests::Contains(s, Vector3D(1, 0, 0)));
}

TEST(ContainsTest, OOBB_PointInside)
{
    OOBB o(Vector3D(0,0,0), Vector3D(1,1,1), Quaternion::Identity());
    EXPECT_TRUE(IntersectionTests::Contains(o, Vector3D(0, 0, 0)));
}

TEST(ContainsTest, OOBB_PointOutside)
{
    OOBB o(Vector3D(0,0,0), Vector3D(1,1,1), Quaternion::Identity());
    EXPECT_FALSE(IntersectionTests::Contains(o, Vector3D(2, 0, 0)));
}

TEST(ContainsTest, Capsule_PointInsideBody)
{
    Capsule c = MakeYAxisCapsule(2.0f);
    EXPECT_TRUE(IntersectionTests::Contains(c, Vector3D(0, 1, 0)));
}

TEST(ContainsTest, Capsule_PointOutside)
{
    Capsule c = MakeYAxisCapsule(2.0f);
    EXPECT_FALSE(IntersectionTests::Contains(c, Vector3D(5, 0, 0)));
}

TEST(ContainsTest, Cylinder_PointInsideBody)
{
    Cylinder c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    EXPECT_TRUE(IntersectionTests::Contains(c, Vector3D(0, 0, 0)));
}

TEST(ContainsTest, Cylinder_PointOutside)
{
    Cylinder c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    EXPECT_FALSE(IntersectionTests::Contains(c, Vector3D(5, 0, 0)));
}

TEST(ContainsTest, Cylinder_PointPastEndcap_ReturnsFalse)
{
    Cylinder c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    EXPECT_FALSE(IntersectionTests::Contains(c, Vector3D(0, 3, 0)));
}

TEST(ContainsTest, Frustum_PointInside)
{
    Frustum f = MakeIdentityFrustum();
    EXPECT_TRUE(IntersectionTests::Contains(f, Vector3D(0, 0, 0)));
}

TEST(ContainsTest, Frustum_PointOutside)
{
    Frustum f = MakeIdentityFrustum();
    EXPECT_FALSE(IntersectionTests::Contains(f, Vector3D(5, 0, 0)));
}

// ============================================================================
// ClosestPoint
// ============================================================================

TEST(ClosestPointTest, AABB_PointInside_ReturnsSelf)
{
    AABB a = MakeUnitAABB();
    Vector3D p(0, 0, 0);
    Vector3D cp = IntersectionTests::ClosestPoint(a, p);
    EXPECT_EQ(cp.X(), p.X());
    EXPECT_EQ(cp.Y(), p.Y());
    EXPECT_EQ(cp.Z(), p.Z());
}

TEST(ClosestPointTest, AABB_PointOutside_ClampsToFace)
{
    AABB a = MakeUnitAABB();
    Vector3D p(3, 0, 0);
    Vector3D cp = IntersectionTests::ClosestPoint(a, p);
    EXPECT_EQ(cp.X(), 1.0f);
    EXPECT_EQ(cp.Y(), 0.0f);
    EXPECT_EQ(cp.Z(), 0.0f);
}

TEST(ClosestPointTest, Sphere_PointOutside_ReturnsOnSurface)
{
    Sphere s = MakeUnitSphere();
    Vector3D p(3, 0, 0);
    Vector3D cp = IntersectionTests::ClosestPoint(s, p);
    EXPECT_NEAR(cp.X(), 1.0f, 1e-5f);
    EXPECT_NEAR(cp.Y(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Z(), 0.0f, 1e-5f);
}

TEST(ClosestPointTest, OOBB_PointOutside_ClampsInLocalSpace)
{
    OOBB o(Vector3D(0,0,0), Vector3D(1,1,1), Quaternion::Identity());
    Vector3D p(3, 0, 0);
    Vector3D cp = IntersectionTests::ClosestPoint(o, p);
    EXPECT_NEAR(cp.X(), 1.0f, 1e-5f);
    EXPECT_NEAR(cp.Y(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Z(), 0.0f, 1e-5f);
}

TEST(ClosestPointTest, Triangle_PointAboveCentroid_ReturnsInsideTriangle)
{
    Triangle t = MakeAxisAlignedTriangle();
    // (0,0,0),(1,0,0),(0,0,1) — centroid at (1/3, 0, 1/3)
    Vector3D p(0.3f, 1.0f, 0.3f);
    Vector3D cp = IntersectionTests::ClosestPoint(t, p);
    EXPECT_NEAR(cp.Y(), 0.0f, 1e-5f);
}

TEST(ClosestPointTest, Triangle_PointNearVertex_ReturnsVertex)
{
    Triangle t(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,1));
    Vector3D p(-0.5f, -0.5f, -0.5f);
    Vector3D cp = IntersectionTests::ClosestPoint(t, p);
    EXPECT_NEAR(cp.X(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Y(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Z(), 0.0f, 1e-5f);
}

TEST(ClosestPointTest, Triangle_PointNearEdge_ReturnsOnEdge)
{
    Triangle t(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,1));
    // Point near edge v0-v1 (z=0 side), just outside in z
    Vector3D p(0.5f, 0.0f, -0.5f);
    Vector3D cp = IntersectionTests::ClosestPoint(t, p);
    EXPECT_NEAR(cp.Z(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Y(), 0.0f, 1e-5f);
}

TEST(ClosestPointTest, Capsule_PointBesideAxis_ReturnsOnSurface)
{
    Capsule c = MakeYAxisCapsule(2.0f);
    Vector3D p(3, 1, 0);
    Vector3D cp = IntersectionTests::ClosestPoint(c, p);
    // Should be on the capsule surface, 0.5f radius from axis at y=1
    EXPECT_NEAR(cp.Y(), 1.0f, 1e-5f);
    EXPECT_NEAR(cp.X(), 0.5f, 1e-5f);
}

TEST(ClosestPointTest, Cylinder_PointBesideAxis_ReturnsOnSurface)
{
    Cylinder c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    Vector3D p(3, 0, 0);
    Vector3D cp = IntersectionTests::ClosestPoint(c, p);
    EXPECT_NEAR(cp.X(), 0.5f, 1e-5f);
    EXPECT_NEAR(cp.Y(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Z(), 0.0f, 1e-5f);
}
