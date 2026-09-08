#include <gtest/gtest.h>
#include <DiaGeometry3D/Shapes/Cylinder.h>

using namespace Dia::Geometry3D;
using namespace Dia::Maths;

TEST(CylinderTest, DefaultConstruction_ZeroRadius)
{
    Cylinder c;
    EXPECT_FLOAT_EQ(c.GetRadius(), 0.0f);
}

TEST(CylinderTest, ConstructWithValues_StoresCorrectly)
{
    Cylinder c(Vector3D(0,0,0), Vector3D(0,2,0), 1.0f);
    EXPECT_FLOAT_EQ(c.GetRadius(), 1.0f);
    EXPECT_FLOAT_EQ(c.GetEndB().Y(), 2.0f);
}

TEST(CylinderTest, CopyConstructor_CopiesValues)
{
    Cylinder a(Vector3D(0,0,0), Vector3D(0,1,0), 0.5f);
    Cylinder b(a);
    EXPECT_TRUE(a == b);
}

TEST(CylinderTest, CalculateLength_CorrectValue)
{
    Cylinder c(Vector3D(0,-2,0), Vector3D(0,2,0), 1.0f);
    EXPECT_NEAR(c.CalculateLength(), 4.0f, 1e-5f);
}

TEST(CylinderTest, CalculateAxisDirection_IsNormalized)
{
    Cylinder c(Vector3D(0,0,0), Vector3D(0,3,0), 1.0f);
    Vector3D dir = c.CalculateAxisDirection();
    EXPECT_NEAR(dir.Magnitude(), 1.0f, 1e-5f);
}

TEST(CylinderTest, IsIntersecting_PointInsideCylinder_Penetrating)
{
    Cylinder c(Vector3D(0,-1,0), Vector3D(0,1,0), 1.0f);
    EXPECT_EQ(c.IsIntersecting(Vector3D(0.5f, 0.0f, 0.0f)), IntersectionClassify::kPenetrating);
}

TEST(CylinderTest, IsIntersecting_PointBeyondFlatCap_NoIntersection)
{
    Cylinder c(Vector3D(0,-1,0), Vector3D(0,1,0), 1.0f);
    // Above flat cap
    EXPECT_EQ(c.IsIntersecting(Vector3D(0.0f, 2.0f, 0.0f)), IntersectionClassify::kNoIntersection);
}

TEST(CylinderTest, IsIntersecting_PointOutsideRadius_NoIntersection)
{
    Cylinder c(Vector3D(0,-1,0), Vector3D(0,1,0), 1.0f);
    EXPECT_EQ(c.IsIntersecting(Vector3D(5.0f, 0.0f, 0.0f)), IntersectionClassify::kNoIntersection);
}

TEST(CylinderTest, Contains_PointInside_True)
{
    Cylinder c(Vector3D(0,-1,0), Vector3D(0,1,0), 1.0f);
    EXPECT_TRUE(c.Contains(Vector3D(0,0,0)));
}
