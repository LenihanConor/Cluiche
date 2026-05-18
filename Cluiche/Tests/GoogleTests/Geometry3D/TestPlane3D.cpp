#include <gtest/gtest.h>
#include <DiaGeometry3D/Shapes/Plane.h>
#include <DiaGeometry3D/Testing/Geometry3DShapeFactory.h>

using namespace Dia::Geometry3D;
using namespace Dia::Geometry3D::Testing;
using namespace Dia::Maths;

TEST(Plane3DTest, DefaultConstruction_YUpPlaneAtOrigin)
{
    Plane p;
    EXPECT_NEAR(p.GetNormal().Y(), 1.0f, 1e-5f);
    EXPECT_FLOAT_EQ(p.GetD(), 0.0f);
}

TEST(Plane3DTest, ConstructWithValues_StoresCorrectly)
{
    Plane p(Vector3D(0,1,0), 3.0f);
    EXPECT_FLOAT_EQ(p.GetD(), 3.0f);
}

TEST(Plane3DTest, CopyConstructor_CopiesValues)
{
    Plane a(Vector3D(0,1,0), 2.0f);
    Plane b(a);
    EXPECT_TRUE(a == b);
}

TEST(Plane3DTest, FromPointAndNormal_PlanePassesThroughPoint)
{
    Vector3D point(0, 5, 0);
    Vector3D normal(0, 1, 0);
    Plane p = Plane::FromPointAndNormal(point, normal);
    // DistanceTo(point) should be 0
    EXPECT_NEAR(p.DistanceTo(point), 0.0f, 1e-5f);
}

TEST(Plane3DTest, FromThreePoints_RightHandedNormal)
{
    // CCW in XZ plane viewed from +Y: (0,0,0),(1,0,0),(0,0,-1) -> +Y normal
    Plane p = Plane::FromThreePoints(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,-1));
    EXPECT_NEAR(p.GetNormal().X(), 0.0f, 1e-5f);
    EXPECT_NEAR(p.GetNormal().Y(), 1.0f, 1e-5f);
    EXPECT_NEAR(p.GetNormal().Z(), 0.0f, 1e-5f);
}

TEST(Plane3DTest, DistanceTo_PointOnPositiveSide_Positive)
{
    Plane p = MakeXZPlane(); // Y-up plane d=0
    EXPECT_GT(p.DistanceTo(Vector3D(0, 5, 0)), 0.0f);
}

TEST(Plane3DTest, DistanceTo_PointOnNegativeSide_Negative)
{
    Plane p = MakeXZPlane();
    EXPECT_LT(p.DistanceTo(Vector3D(0, -3, 0)), 0.0f);
}

TEST(Plane3DTest, DistanceTo_PointOnPlane_Zero)
{
    Plane p = MakeXZPlane();
    EXPECT_NEAR(p.DistanceTo(Vector3D(5, 0, 3)), 0.0f, 1e-5f);
}

TEST(Plane3DTest, ClassifyPoint_Front_kFront)
{
    Plane p = MakeXZPlane();
    EXPECT_EQ(p.ClassifyPoint(Vector3D(0, 1, 0)), PlaneSide::kFront);
}

TEST(Plane3DTest, ClassifyPoint_Behind_kBehind)
{
    Plane p = MakeXZPlane();
    EXPECT_EQ(p.ClassifyPoint(Vector3D(0, -1, 0)), PlaneSide::kBehind);
}

TEST(Plane3DTest, ClassifyPoint_OnPlane_kOnPlane)
{
    Plane p = MakeXZPlane();
    EXPECT_EQ(p.ClassifyPoint(Vector3D(3, 0, 7)), PlaneSide::kOnPlane);
}
