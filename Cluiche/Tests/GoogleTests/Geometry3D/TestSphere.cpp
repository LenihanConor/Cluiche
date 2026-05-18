#include <gtest/gtest.h>
#include <cmath>
#include <DiaGeometry3D/Shapes/Sphere.h>
#include <DiaGeometry3D/Testing/Geometry3DShapeFactory.h>

using namespace Dia::Geometry3D;
using namespace Dia::Geometry3D::Testing;
using namespace Dia::Maths;

static constexpr float kPi = 3.14159265358979323846f;

TEST(SphereTest, DefaultConstruction_ZeroRadiusAtOrigin)
{
    Sphere s;
    EXPECT_EQ(s.GetRadius(), 0.0f);
    EXPECT_EQ(s.GetCenter().X(), 0.0f);
}

TEST(SphereTest, ConstructWithValues_StoresCorrectly)
{
    Sphere s(Vector3D(1,2,3), 5.0f);
    EXPECT_FLOAT_EQ(s.GetRadius(), 5.0f);
    EXPECT_FLOAT_EQ(s.GetCenter().Y(), 2.0f);
}

TEST(SphereTest, CopyConstructor_CopiesValues)
{
    Sphere a(Vector3D(1,2,3), 4.0f);
    Sphere b(a);
    EXPECT_TRUE(a == b);
}

TEST(SphereTest, CalculateVolume_CorrectValue)
{
    Sphere s(Vector3D(0,0,0), 1.0f);
    EXPECT_NEAR(s.CalculateVolume(), (4.0f / 3.0f) * kPi, 1e-5f);
}

TEST(SphereTest, CalculateSurfaceArea_CorrectValue)
{
    Sphere s(Vector3D(0,0,0), 1.0f);
    EXPECT_NEAR(s.CalculateSurfaceArea(), 4.0f * kPi, 1e-5f);
}

TEST(SphereTest, EncapsulatePoint_GrowsRadius)
{
    Sphere s(Vector3D(0,0,0), 1.0f);
    s.Encapsulate(Vector3D(5,0,0));
    EXPECT_FLOAT_EQ(s.GetRadius(), 5.0f);
}

TEST(SphereTest, EncapsulatePoint_PointInsideDoesNotShrink)
{
    Sphere s(Vector3D(0,0,0), 10.0f);
    s.Encapsulate(Vector3D(1,0,0));
    EXPECT_FLOAT_EQ(s.GetRadius(), 10.0f);
}

TEST(SphereTest, IsIntersecting_PointInside_Penetrating)
{
    Sphere s = MakeUnitSphere();
    EXPECT_EQ(s.IsIntersecting(Vector3D(0,0,0)), IntersectionClassify::kPenetrating);
}

TEST(SphereTest, IsIntersecting_PointOutside_NoIntersection)
{
    Sphere s = MakeUnitSphere();
    EXPECT_EQ(s.IsIntersecting(Vector3D(2,0,0)), IntersectionClassify::kNoIntersection);
}

TEST(SphereTest, IsIntersecting_PointOnSurface_Penetrating)
{
    Sphere s = MakeUnitSphere();
    EXPECT_EQ(s.IsIntersecting(Vector3D(1,0,0)), IntersectionClassify::kPenetrating);
}

TEST(SphereTest, Contains_PointInside_True)
{
    Sphere s = MakeUnitSphere();
    EXPECT_TRUE(s.Contains(Vector3D(0.5f, 0.0f, 0.0f)));
}

TEST(SphereTest, Contains_PointOutside_False)
{
    Sphere s = MakeUnitSphere();
    EXPECT_FALSE(s.Contains(Vector3D(2.0f, 0.0f, 0.0f)));
}
