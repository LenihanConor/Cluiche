#include <gtest/gtest.h>
#include <DiaGeometry3D/Shapes/Triangle.h>
#include <DiaGeometry3D/Testing/Geometry3DShapeFactory.h>

using namespace Dia::Geometry3D;
using namespace Dia::Geometry3D::Testing;
using namespace Dia::Maths;

TEST(TriangleTest, DefaultConstruction_HasThreeVertices)
{
    Triangle t;
    // default is a non-degenerate triangle
    Vector3D n = t.CalculateNormal();
    EXPECT_NEAR(n.Magnitude(), 1.0f, 1e-5f);
}

TEST(TriangleTest, ConstructWithValues_StoresCorrectly)
{
    Triangle t(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,-1));
    EXPECT_FLOAT_EQ(t.GetV0().X(), 0.0f);
    EXPECT_FLOAT_EQ(t.GetV1().X(), 1.0f);
    EXPECT_FLOAT_EQ(t.GetV2().Z(), -1.0f);
}

TEST(TriangleTest, CopyConstructor_CopiesValues)
{
    Triangle a(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,-1));
    Triangle b(a);
    EXPECT_TRUE(a == b);
}

TEST(TriangleTest, Equality_SameOrder_Equal)
{
    Triangle a(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,-1));
    Triangle b(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,-1));
    EXPECT_TRUE(a == b);
}

TEST(TriangleTest, Equality_DifferentOrder_NotEqual)
{
    Triangle a(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,-1));
    Triangle b(Vector3D(1,0,0), Vector3D(0,0,0), Vector3D(0,0,-1));
    EXPECT_FALSE(a == b);
}

TEST(TriangleTest, CalculateNormal_RightHandedCounterClockwise_PlusY)
{
    // CCW in XZ plane when viewed from +Y: (0,0,0) -> (1,0,0) -> (0,0,-1)
    // (v1-v0) = (1,0,0), (v2-v0) = (0,0,-1)
    // cross = (0,0,0)x_axis (1,0,0) x (0,0,-1) = (0*(-1)-0*0, 0*0-1*(-1), 1*0-0*0) = (0,1,0) -> +Y
    Triangle t = MakeAxisAlignedTriangle();
    Vector3D n = t.CalculateNormal();
    EXPECT_NEAR(n.X(), 0.0f, 1e-5f);
    EXPECT_NEAR(n.Y(), 1.0f, 1e-5f);
    EXPECT_NEAR(n.Z(), 0.0f, 1e-5f);
}

TEST(TriangleTest, CalculateArea_UnitRightTriangle_HalfUnit)
{
    Triangle t(Vector3D(0,0,0), Vector3D(1,0,0), Vector3D(0,0,1));
    EXPECT_NEAR(t.CalculateArea(), 0.5f, 1e-5f);
}

TEST(TriangleTest, CalculateCentroid_EquilateralLike_AverageOfVertices)
{
    Triangle t(Vector3D(0,0,0), Vector3D(3,0,0), Vector3D(0,0,3));
    Vector3D c = t.CalculateCentroid();
    EXPECT_NEAR(c.X(), 1.0f, 1e-5f);
    EXPECT_NEAR(c.Y(), 0.0f, 1e-5f);
    EXPECT_NEAR(c.Z(), 1.0f, 1e-5f);
}
