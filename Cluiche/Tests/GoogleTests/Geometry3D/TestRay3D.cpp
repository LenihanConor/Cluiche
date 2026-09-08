#include <gtest/gtest.h>
#include <DiaGeometry3D/Shapes/Ray.h>

using namespace Dia::Geometry3D;
using namespace Dia::Maths;

TEST(Ray3DTest, ConstructWithValues_StoresCorrectly)
{
    Ray r(Vector3D(1,2,3), Vector3D(0,0,1));
    EXPECT_FLOAT_EQ(r.GetOrigin().X(), 1.0f);
    EXPECT_FLOAT_EQ(r.GetDirection().Z(), 1.0f);
}

TEST(Ray3DTest, CopyConstructor_CopiesValues)
{
    Ray a(Vector3D(0,0,0), Vector3D(1,0,0));
    Ray b(a);
    EXPECT_TRUE(a == b);
}

TEST(Ray3DTest, GetPointAt_t0_ReturnsOrigin)
{
    Ray r(Vector3D(1,2,3), Vector3D(0,0,1));
    Vector3D p = r.GetPointAt(0.0f);
    EXPECT_FLOAT_EQ(p.X(), 1.0f);
    EXPECT_FLOAT_EQ(p.Y(), 2.0f);
    EXPECT_FLOAT_EQ(p.Z(), 3.0f);
}

TEST(Ray3DTest, GetPointAt_t1_ReturnsOriginPlusDirection)
{
    Ray r(Vector3D(1,0,0), Vector3D(0,0,1));
    Vector3D p = r.GetPointAt(1.0f);
    EXPECT_FLOAT_EQ(p.X(), 1.0f);
    EXPECT_FLOAT_EQ(p.Z(), 1.0f);
}

TEST(Ray3DTest, GetPointAt_tNegative_PointBehindOrigin)
{
    Ray r(Vector3D(0,0,0), Vector3D(0,0,1));
    Vector3D p = r.GetPointAt(-2.0f);
    EXPECT_FLOAT_EQ(p.Z(), -2.0f);
}
