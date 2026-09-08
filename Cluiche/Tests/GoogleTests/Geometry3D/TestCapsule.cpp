#include <gtest/gtest.h>
#include <cmath>
#include <DiaGeometry3D/Shapes/Capsule.h>
#include <DiaGeometry3D/Testing/Geometry3DShapeFactory.h>

using namespace Dia::Geometry3D;
using namespace Dia::Geometry3D::Testing;
using namespace Dia::Maths;

TEST(CapsuleTest, DefaultConstruction_ZeroRadius)
{
    Capsule c;
    EXPECT_FLOAT_EQ(c.GetRadius(), 0.0f);
}

TEST(CapsuleTest, ConstructWithValues_StoresCorrectly)
{
    Capsule c(Vector3D(0,0,0), Vector3D(0,2,0), 1.0f);
    EXPECT_FLOAT_EQ(c.GetRadius(), 1.0f);
    EXPECT_FLOAT_EQ(c.GetEndB().Y(), 2.0f);
}

TEST(CapsuleTest, CopyConstructor_CopiesValues)
{
    Capsule a(Vector3D(0,0,0), Vector3D(0,1,0), 0.5f);
    Capsule b(a);
    EXPECT_TRUE(a == b);
}

TEST(CapsuleTest, CalculateLength_CorrectValue)
{
    Capsule c = MakeYAxisCapsule(4.0f);
    EXPECT_NEAR(c.CalculateLength(), 4.0f, 1e-5f);
}

TEST(CapsuleTest, CalculateAxis_CorrectDirection)
{
    Capsule c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    Vector3D axis = c.CalculateAxis();
    EXPECT_NEAR(axis.X(), 0.0f, 1e-5f);
    EXPECT_NEAR(axis.Y(), 2.0f, 1e-5f);
    EXPECT_NEAR(axis.Z(), 0.0f, 1e-5f);
}

TEST(CapsuleTest, CalculateAxisDirection_IsNormalized)
{
    Capsule c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    Vector3D dir = c.CalculateAxisDirection();
    EXPECT_NEAR(dir.Magnitude(), 1.0f, 1e-5f);
}

TEST(CapsuleTest, IsIntersecting_PointInsideCylinderCore_Penetrating)
{
    Capsule c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    EXPECT_EQ(c.IsIntersecting(Vector3D(0,0,0)), IntersectionClassify::kPenetrating);
}

TEST(CapsuleTest, IsIntersecting_PointInsideSphericalCap_Penetrating)
{
    Capsule c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    // Point above endB, within radius
    EXPECT_EQ(c.IsIntersecting(Vector3D(0.3f, 1.3f, 0.0f)), IntersectionClassify::kPenetrating);
}

TEST(CapsuleTest, IsIntersecting_PointOutside_NoIntersection)
{
    Capsule c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    EXPECT_EQ(c.IsIntersecting(Vector3D(5,0,0)), IntersectionClassify::kNoIntersection);
}

TEST(CapsuleTest, Contains_PointInside_True)
{
    Capsule c(Vector3D(0,-1,0), Vector3D(0,1,0), 0.5f);
    EXPECT_TRUE(c.Contains(Vector3D(0.0f, 0.0f, 0.0f)));
}
