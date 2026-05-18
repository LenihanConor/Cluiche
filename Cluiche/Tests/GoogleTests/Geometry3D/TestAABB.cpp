#include <gtest/gtest.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaGeometry3D/Testing/Geometry3DShapeFactory.h>

using namespace Dia::Geometry3D;
using namespace Dia::Geometry3D::Testing;
using namespace Dia::Maths;

TEST(AABBTest, DefaultConstruction_ZeroVolumeAtOrigin)
{
    AABB a;
    EXPECT_EQ(a.GetMin().X(), 0.0f);
    EXPECT_EQ(a.GetMin().Y(), 0.0f);
    EXPECT_EQ(a.GetMin().Z(), 0.0f);
    EXPECT_EQ(a.GetMax().X(), 0.0f);
    EXPECT_EQ(a.GetMax().Y(), 0.0f);
    EXPECT_EQ(a.GetMax().Z(), 0.0f);
}

TEST(AABBTest, ConstructFromMinMax_StoresCorrectly)
{
    AABB a(Vector3D(-1, -2, -3), Vector3D(1, 2, 3));
    EXPECT_EQ(a.GetMin().X(), -1.0f);
    EXPECT_EQ(a.GetMax().Z(),  3.0f);
}

TEST(AABBTest, CopyConstructor_CopiesValues)
{
    AABB a(Vector3D(1, 2, 3), Vector3D(4, 5, 6));
    AABB b(a);
    EXPECT_TRUE(a == b);
}

TEST(AABBTest, Equality_SameValues_Equal)
{
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    AABB b(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(AABBTest, FromCenterExtents_CorrectMinMax)
{
    AABB a = AABB::FromCenterExtents(Vector3D(0,0,0), Vector3D(1,1,1));
    EXPECT_FLOAT_EQ(a.GetMin().X(), -1.0f);
    EXPECT_FLOAT_EQ(a.GetMax().X(),  1.0f);
}

TEST(AABBTest, CalculateCenter_ReturnsCenter)
{
    AABB a(Vector3D(0,0,0), Vector3D(2,4,6));
    Vector3D c = a.CalculateCenter();
    EXPECT_FLOAT_EQ(c.X(), 1.0f);
    EXPECT_FLOAT_EQ(c.Y(), 2.0f);
    EXPECT_FLOAT_EQ(c.Z(), 3.0f);
}

TEST(AABBTest, CalculateExtents_ReturnsHalfSize)
{
    AABB a(Vector3D(-2,-4,-6), Vector3D(2,4,6));
    Vector3D e = a.CalculateExtents();
    EXPECT_FLOAT_EQ(e.X(), 2.0f);
    EXPECT_FLOAT_EQ(e.Y(), 4.0f);
    EXPECT_FLOAT_EQ(e.Z(), 6.0f);
}

TEST(AABBTest, CalculateVolume_CorrectValue)
{
    AABB a(Vector3D(0,0,0), Vector3D(2,3,4));
    EXPECT_FLOAT_EQ(a.CalculateVolume(), 24.0f);
}

TEST(AABBTest, CalculateSurfaceArea_CorrectValue)
{
    // 2x2x2 cube: 6 faces * 4 = 24
    AABB a(Vector3D(-1,-1,-1), Vector3D(1,1,1));
    EXPECT_FLOAT_EQ(a.CalculateSurfaceArea(), 24.0f);
}

TEST(AABBTest, EncapsulatePoint_GrowsAABB)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    a.Encapsulate(Vector3D(2, 3, 4));
    EXPECT_FLOAT_EQ(a.GetMax().X(), 2.0f);
    EXPECT_FLOAT_EQ(a.GetMax().Y(), 3.0f);
    EXPECT_FLOAT_EQ(a.GetMax().Z(), 4.0f);
}

TEST(AABBTest, EncapsulatePoint_ShrinkDoesNothing)
{
    AABB a(Vector3D(-5,-5,-5), Vector3D(5,5,5));
    a.Encapsulate(Vector3D(0, 0, 0));
    EXPECT_FLOAT_EQ(a.GetMin().X(), -5.0f);
    EXPECT_FLOAT_EQ(a.GetMax().X(),  5.0f);
}

TEST(AABBTest, EncapsulateAABB_MergesTwoAABBs)
{
    AABB a(Vector3D(0,0,0), Vector3D(1,1,1));
    AABB b(Vector3D(-1,-1,-1), Vector3D(2,2,2));
    a.Encapsulate(b);
    EXPECT_FLOAT_EQ(a.GetMin().X(), -1.0f);
    EXPECT_FLOAT_EQ(a.GetMax().X(),  2.0f);
}

TEST(AABBTest, IsIntersecting_PointInside_Penetrating)
{
    AABB a = MakeUnitAABB();
    EXPECT_EQ(a.IsIntersecting(Vector3D(0,0,0)), IntersectionClassify::kPenetrating);
}

TEST(AABBTest, IsIntersecting_PointOutside_NoIntersection)
{
    AABB a = MakeUnitAABB();
    EXPECT_EQ(a.IsIntersecting(Vector3D(2,0,0)), IntersectionClassify::kNoIntersection);
}

TEST(AABBTest, IsIntersecting_PointOnEdge_Penetrating)
{
    AABB a = MakeUnitAABB();
    EXPECT_EQ(a.IsIntersecting(Vector3D(1,0,0)), IntersectionClassify::kPenetrating);
}

TEST(AABBTest, Contains_PointInside_True)
{
    AABB a = MakeUnitAABB();
    EXPECT_TRUE(a.Contains(Vector3D(0.5f, 0.5f, 0.5f)));
}

TEST(AABBTest, Contains_PointOutside_False)
{
    AABB a = MakeUnitAABB();
    EXPECT_FALSE(a.Contains(Vector3D(5.0f, 0.0f, 0.0f)));
}

TEST(AABBTest, MakeUnitAABB_CorrectBounds)
{
    AABB a = MakeUnitAABB();
    EXPECT_FLOAT_EQ(a.GetMin().X(), -1.0f);
    EXPECT_FLOAT_EQ(a.GetMax().X(),  1.0f);
}
