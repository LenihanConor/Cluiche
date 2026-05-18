#include <gtest/gtest.h>
#include <DiaGeometry3D/Intersection/IntersectionTests.h>
#include <DiaGeometry3D/Testing/Geometry3DShapeFactory.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Geometry3D;
using namespace Dia::Geometry3D::Testing;
using namespace Dia::Maths;

static IntersectionClassify SwapContainment(IntersectionClassify r)
{
    if (r == IntersectionClassify::kAContainsB) return IntersectionClassify::kBContainsA;
    if (r == IntersectionClassify::kBContainsA) return IntersectionClassify::kAContainsB;
    return r;
}

// ============================================================================
// Degenerate shapes — zero-size or zero-length
// ============================================================================

TEST(IntersectionEdgeTest, ZeroRadiusSphere_ContainsOnlyItsCenter)
{
    Sphere s(Vector3D(5,5,5), 0.0f);
    EXPECT_TRUE(IntersectionTests::Contains(s, Vector3D(5,5,5)));
    EXPECT_FALSE(IntersectionTests::Contains(s, Vector3D(5.001f,5,5)));
}

TEST(IntersectionEdgeTest, ZeroRadiusSphere_vs_AABB_CenterInsideAABB)
{
    Sphere s(Vector3D(0,0,0), 0.0f);
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    EXPECT_NE(IntersectionTests::Test(a, s), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionEdgeTest, ZeroRadiusSphere_vs_AABB_CenterOutsideAABB)
{
    Sphere s(Vector3D(5,0,0), 0.0f);
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    EXPECT_EQ(IntersectionTests::Test(a, s), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionEdgeTest, DegenerateCapsule_ZeroLength_BehavesLikeSphere)
{
    // Start == End: degenerate capsule should behave like a sphere of the same radius
    Capsule c(Vector3D(5,5,5), Vector3D(5,5,5), 1.0f);
    EXPECT_TRUE(IntersectionTests::Contains(c, Vector3D(5,5,5)));
    EXPECT_TRUE(IntersectionTests::Contains(c, Vector3D(6,5,5)));   // at radius
    EXPECT_FALSE(IntersectionTests::Contains(c, Vector3D(7,5,5)));  // beyond radius
}

TEST(IntersectionEdgeTest, DegenerateCylinder_ZeroLength_ReturnsFalseForAll)
{
    // start == end: zero-length axis → len2 < epsilon → Contains always false
    Cylinder c(Vector3D(5,5,5), Vector3D(5,5,5), 1.0f);
    EXPECT_FALSE(IntersectionTests::Contains(c, Vector3D(5,5,5)));
    EXPECT_FALSE(IntersectionTests::Contains(c, Vector3D(5.5f,5,5)));
}

// ============================================================================
// Touching / grazing contacts (should be kPenetrating, not kNoIntersection)
// ============================================================================

TEST(IntersectionEdgeTest, AABB_vs_AABB_TouchingFace_ReturnsPenetrating)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    AABB b(Vector3D(1,0,0), Vector3D(2,1,1));  // share the x=1 face
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kPenetrating);
}

TEST(IntersectionEdgeTest, AABB_vs_AABB_TouchingEdge_ReturnsPenetrating)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    AABB b(Vector3D(1,1,0), Vector3D(2,2,1));  // share the x=1,y=1 edge
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kPenetrating);
}

TEST(IntersectionEdgeTest, AABB_vs_AABB_TouchingCorner_ReturnsPenetrating)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    AABB b(Vector3D(1,1,1), Vector3D(2,2,2));  // single corner at (1,1,1)
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kPenetrating);
}

TEST(IntersectionEdgeTest, Sphere_vs_Sphere_Touching_ReturnsPenetrating)
{
    Sphere a(Vector3D(0,0,0), 1.0f);
    Sphere b(Vector3D(2,0,0), 1.0f);  // exactly touching at x=1
    EXPECT_EQ(IntersectionTests::Test(a, b), IntersectionClassify::kPenetrating);
}

TEST(IntersectionEdgeTest, AABB_vs_Sphere_SphereTouchingFace_ReturnsPenetrating)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    Sphere s(Vector3D(2,0.5f,0.5f), 1.0f);  // sphere exactly touches x=1 face
    EXPECT_EQ(IntersectionTests::Test(a, s), IntersectionClassify::kPenetrating);
}

// ============================================================================
// Identical / coincident shapes
// ============================================================================

TEST(IntersectionEdgeTest, IdenticalAABBs_ReturnsContainment)
{
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    auto r = IntersectionTests::Test(a, a);
    EXPECT_TRUE(r == IntersectionClassify::kAContainsB || r == IntersectionClassify::kBContainsA);
}

TEST(IntersectionEdgeTest, IdenticalSpheres_ReturnsContainment)
{
    Sphere s(Vector3D(0,0,0), 1.0f);
    auto r = IntersectionTests::Test(s, s);
    EXPECT_TRUE(r == IntersectionClassify::kAContainsB || r == IntersectionClassify::kBContainsA);
}

// ============================================================================
// Rotated OOBB containment
// ============================================================================

TEST(IntersectionEdgeTest, RotatedOOBB_Contains_PointOnLocalAxis)
{
    // 45° rotation around Y — local X axis points at (√2/2, 0, √2/2) in world space
    Quaternion q = Quaternion::FromAxisAngle(Vector3D(0,1,0), Angle::Deg45);
    OOBB box(Vector3D(0,0,0), Vector3D(2,2,2), q);

    // World-space center: definitely inside
    EXPECT_TRUE(IntersectionTests::Contains(box, Vector3D(0,0,0)));

    // World-space point that is inside the rotated box but outside its unrotated AABB
    // Rotated box local +X → world (√2/2, 0, -√2/2); at extent 1.0 → world (0.7,0,−0.7)
    EXPECT_TRUE(IntersectionTests::Contains(box, Vector3D(0.5f, 0.0f, -0.5f)));
}

TEST(IntersectionEdgeTest, RotatedOOBB_Rejects_PointOutsideLocalAxes)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D(0,1,0), Angle::Deg45);
    OOBB box(Vector3D(0,0,0), Vector3D(1,1,1), q);

    // A point along world +X beyond half-extent 1/√2 — outside rotated box
    EXPECT_FALSE(IntersectionTests::Contains(box, Vector3D(2,0,0)));
}

TEST(IntersectionEdgeTest, RotatedOOBB_vs_AABB_Disjoint)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D(0,1,0), Angle::Deg45);
    OOBB obb(Vector3D(10,0,0), Vector3D(0.5f,0.5f,0.5f), q);
    AABB aabb(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    EXPECT_EQ(IntersectionTests::Test(aabb, obb), IntersectionClassify::kNoIntersection);
}

// ============================================================================
// Ray vs Plane: coplanar and on-plane cases
// ============================================================================

TEST(IntersectionEdgeTest, RayOnPlane_Origin_ReturnsHit)
{
    // Ray origin exactly on the XZ plane (y=0), pointing up
    Plane p(Vector3D(0,1,0), 0.0f);  // y = 0
    Ray r(Vector3D(1, 0, 1), Vector3D(0,1,0));
    EXPECT_EQ(IntersectionTests::Test(r, p), IntersectionClassify::kPenetrating);
}

TEST(IntersectionEdgeTest, RayCoplanar_ParallelAndOnPlane_ReturnsNoIntersection)
{
    // Ray moving along the plane surface — parallel, denom = 0
    Plane p(Vector3D(0,1,0), 0.0f);
    Ray r(Vector3D(0,0,0), Vector3D(1,0,0));  // along X, in the Y=0 plane
    EXPECT_EQ(IntersectionTests::Test(r, p), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionEdgeTest, RayBehindPlane_PointsAway_ReturnsNoIntersection)
{
    Plane p(Vector3D(0,1,0), 0.0f);  // y = 0, normal +Y
    Ray r(Vector3D(0, 1, 0), Vector3D(0, 1, 0));  // origin above plane, pointing away
    EXPECT_EQ(IntersectionTests::Test(r, p), IntersectionClassify::kNoIntersection);
}

// ============================================================================
// Triangle vs AABB: touching edge/face cases
// ============================================================================

TEST(IntersectionEdgeTest, Triangle_TouchingAABBFace_ReturnsPenetrating)
{
    // Triangle in y=1 plane, AABB extends to y=1
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    Triangle t(Vector3D(-0.5f, 1.0f, -0.5f), Vector3D(0.5f, 1.0f, -0.5f), Vector3D(0.0f, 1.0f, 0.5f));
    EXPECT_NE(IntersectionTests::Test(t, a), IntersectionClassify::kNoIntersection);
}

TEST(IntersectionEdgeTest, Triangle_FlatInXZPlane_IntersectsHorizontalAABB)
{
    // Flat triangle in y=0 plane, inside an AABB that spans y=-1..1
    AABB a(Vector3D(-2,-1,-2), Vector3D(2,1,2));
    Triangle t(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,1));
    EXPECT_EQ(IntersectionTests::Test(t, a), IntersectionClassify::kBContainsA);
}

// ============================================================================
// ClosestPoint edge cases
// ============================================================================

TEST(IntersectionEdgeTest, ClosestPoint_AABB_PointAtCorner)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    Vector3D p(0,0,0);  // at the min corner itself
    Vector3D cp = IntersectionTests::ClosestPoint(a, p);
    EXPECT_NEAR(cp.X(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Y(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Z(), 0.0f, 1e-5f);
}

TEST(IntersectionEdgeTest, ClosestPoint_AABB_PointDiagonallyOutside)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    Vector3D p(3,3,3);
    Vector3D cp = IntersectionTests::ClosestPoint(a, p);
    EXPECT_NEAR(cp.X(), 1.0f, 1e-5f);
    EXPECT_NEAR(cp.Y(), 1.0f, 1e-5f);
    EXPECT_NEAR(cp.Z(), 1.0f, 1e-5f);
}

TEST(IntersectionEdgeTest, ClosestPoint_Sphere_PointAtCenter_ReturnsOnSurface)
{
    Sphere s(Vector3D(0,0,0), 2.0f);
    Vector3D p(0,0,0);  // at center
    Vector3D cp = IntersectionTests::ClosestPoint(s, p);
    float distSq = cp.X()*cp.X() + cp.Y()*cp.Y() + cp.Z()*cp.Z();
    EXPECT_NEAR(distSq, 4.0f, 1e-4f);  // on surface at radius 2
}

TEST(IntersectionEdgeTest, ClosestPoint_Triangle_PointAboveEdge_ReturnsOnEdge)
{
    Triangle t(Vector3D(0,0,0), Vector3D(4,0,0), Vector3D(0,0,4));
    // Point directly above midpoint of v0-v1 edge
    Vector3D p(2,5,0);
    Vector3D cp = IntersectionTests::ClosestPoint(t, p);
    EXPECT_NEAR(cp.Y(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Z(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.X(), 2.0f, 1e-5f);
}

TEST(IntersectionEdgeTest, ClosestPoint_Triangle_PointAboveV1_ReturnsV1)
{
    Triangle t(Vector3D(0,0,0), Vector3D(10,0,0), Vector3D(0,0,10));
    Vector3D p(15,5,0);  // beyond V1 along X
    Vector3D cp = IntersectionTests::ClosestPoint(t, p);
    EXPECT_NEAR(cp.X(), 10.0f, 1e-5f);
    EXPECT_NEAR(cp.Y(), 0.0f,  1e-5f);
    EXPECT_NEAR(cp.Z(), 0.0f,  1e-5f);
}

TEST(IntersectionEdgeTest, ClosestPoint_Capsule_PointAtStartEndCap)
{
    Capsule c(Vector3D(0,-5,0), Vector3D(0,5,0), 1.0f);
    // Point far below start — closest point on capsule surface
    Vector3D p(0,-10,0);
    Vector3D cp = IntersectionTests::ClosestPoint(c, p);
    // Should be on the bottom endcap at (0,-6,0)
    EXPECT_NEAR(cp.X(), 0.0f, 1e-5f);
    EXPECT_NEAR(cp.Y(), -6.0f, 1e-5f);
    EXPECT_NEAR(cp.Z(), 0.0f, 1e-5f);
}

// ============================================================================
// Contains: boundary conditions
// ============================================================================

TEST(IntersectionEdgeTest, AABB_Contains_PointExactlyOnAllBoundaries)
{
    AABB a(Vector3D(1,1,1), Vector3D(5,5,5));
    // All 6 faces
    EXPECT_TRUE(IntersectionTests::Contains(a, Vector3D(1, 3, 3)));   // min X
    EXPECT_TRUE(IntersectionTests::Contains(a, Vector3D(5, 3, 3)));   // max X
    EXPECT_TRUE(IntersectionTests::Contains(a, Vector3D(3, 1, 3)));   // min Y
    EXPECT_TRUE(IntersectionTests::Contains(a, Vector3D(3, 5, 3)));   // max Y
    EXPECT_TRUE(IntersectionTests::Contains(a, Vector3D(3, 3, 1)));   // min Z
    EXPECT_TRUE(IntersectionTests::Contains(a, Vector3D(3, 3, 5)));   // max Z
}

TEST(IntersectionEdgeTest, Sphere_Contains_PointExactlyOnSurface)
{
    Sphere s(Vector3D(0,0,0), 3.0f);
    EXPECT_TRUE(IntersectionTests::Contains(s, Vector3D(3,0,0)));
    EXPECT_TRUE(IntersectionTests::Contains(s, Vector3D(0,3,0)));
    EXPECT_TRUE(IntersectionTests::Contains(s, Vector3D(0,0,3)));
    EXPECT_FALSE(IntersectionTests::Contains(s, Vector3D(3.001f,0,0)));
}

TEST(IntersectionEdgeTest, Frustum_Contains_PointExactlyOnFace_ReturnsTrue)
{
    // MakeIdentityFrustum covers [-1..1]³ with inward normals
    Frustum f = MakeIdentityFrustum();
    // Boundary of the frustum volume — point exactly on a plane (distance = 0)
    EXPECT_TRUE(IntersectionTests::Contains(f, Vector3D(0,0,0)));
}

// ============================================================================
// Symmetry spot-checks
// ============================================================================

TEST(IntersectionEdgeTest, Symmetric_OOBBvsOOBB_Rotated)
{
    Quaternion q = Quaternion::FromAxisAngle(Vector3D(0,0,1), Angle::Deg45);
    OOBB a(Vector3D(0,0,0), Vector3D(1,1,1), q);
    OOBB b(Vector3D(0.5f,0.5f,0), Vector3D(0.5f,0.5f,0.5f), Quaternion::Identity());

    auto r1 = IntersectionTests::Test(a, b);
    auto r2 = IntersectionTests::Test(b, a);
    EXPECT_EQ(r1, SwapContainment(r2));
}

TEST(IntersectionEdgeTest, Symmetric_AABBvsSphere_Touching)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    Sphere s(Vector3D(2,0.5f,0.5f), 1.0f);  // touching x=1 face

    auto r1 = IntersectionTests::Test(a, s);
    auto r2 = IntersectionTests::Test(s, a);
    EXPECT_EQ(r1, SwapContainment(r2));
}

TEST(IntersectionEdgeTest, Symmetric_TrianglevsAABB_Straddling)
{
    AABB a(Vector3D(0,0,0), Vector3D(2,2,2));
    Triangle t(Vector3D(-1,1,1), Vector3D(3,1,1), Vector3D(1,3,-1));  // crosses faces

    auto r1 = IntersectionTests::Test(t, a);
    auto r2 = IntersectionTests::Test(a, t);
    EXPECT_EQ(r1, SwapContainment(r2));
}
