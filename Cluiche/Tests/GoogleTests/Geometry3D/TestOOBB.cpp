#include <gtest/gtest.h>
#include <DiaGeometry3D/Shapes/OOBB.h>
#include <DiaMaths/Quaternion/Quaternion.h>

using namespace Dia::Geometry3D;
using namespace Dia::Maths;

TEST(OOBBTest, DefaultConstruction_ZeroHalfExtents)
{
    OOBB o;
    EXPECT_EQ(o.GetCenter().X(), 0.0f);
    EXPECT_EQ(o.GetHalfExtents().X(), 0.0f);
}

TEST(OOBBTest, ConstructWithValues_StoresCorrectly)
{
    Vector3D center(1.0f, 2.0f, 3.0f);
    Vector3D halfExt(0.5f, 1.0f, 1.5f);
    Quaternion q = Quaternion::Identity();
    OOBB o(center, halfExt, q);
    EXPECT_FLOAT_EQ(o.GetCenter().X(), 1.0f);
    EXPECT_FLOAT_EQ(o.GetHalfExtents().Y(), 1.0f);
    EXPECT_TRUE(o.GetOrientation() == Quaternion::Identity());
}

TEST(OOBBTest, CopyConstructor_CopiesValues)
{
    OOBB a(Vector3D(1,2,3), Vector3D(1,1,1), Quaternion::Identity());
    OOBB b(a);
    EXPECT_TRUE(a == b);
}

TEST(OOBBTest, GetAxes_IdentityOrientation_ReturnsWorldAxes)
{
    OOBB o(Vector3D(0,0,0), Vector3D(1,1,1), Quaternion::Identity());
    Vector3D ax, ay, az;
    o.GetAxes(ax, ay, az);
    EXPECT_NEAR(ax.X(), 1.0f, 1e-5f);
    EXPECT_NEAR(ay.Y(), 1.0f, 1e-5f);
    EXPECT_NEAR(az.Z(), 1.0f, 1e-5f);
}

TEST(OOBBTest, IsIntersecting_PointAtCenter_Penetrating)
{
    OOBB o(Vector3D(0,0,0), Vector3D(1,1,1), Quaternion::Identity());
    EXPECT_EQ(o.IsIntersecting(Vector3D(0,0,0)), IntersectionClassify::kPenetrating);
}

TEST(OOBBTest, IsIntersecting_PointOutside_NoIntersection)
{
    OOBB o(Vector3D(0,0,0), Vector3D(1,1,1), Quaternion::Identity());
    EXPECT_EQ(o.IsIntersecting(Vector3D(5,0,0)), IntersectionClassify::kNoIntersection);
}

TEST(OOBBTest, Contains_PointInside_True)
{
    OOBB o(Vector3D(0,0,0), Vector3D(2,2,2), Quaternion::Identity());
    EXPECT_TRUE(o.Contains(Vector3D(0.5f, 0.5f, 0.5f)));
}
