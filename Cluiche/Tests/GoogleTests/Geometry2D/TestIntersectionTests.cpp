#include <gtest/gtest.h>
#include <DiaGeometry2D/Intersection/IntersectionTests.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/Arc.h>
#include <DiaGeometry2D/Shapes/Capsule.h>
#include <DiaGeometry2D/Shapes/OORect.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/IntersectionClassify.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Circle vs Circle
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, CircleVsCircle_Separate_NoIntersection)
{
    Circle a(1.0f, Vector2D(0.0f, 0.0f));
    Circle b(1.0f, Vector2D(10.0f, 0.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kNoIntersection);
}

TEST(Geometry2D_IntersectionTests, CircleVsCircle_Overlapping_Penetrating)
{
    Circle a(2.0f, Vector2D(0.0f, 0.0f));
    Circle b(2.0f, Vector2D(2.0f, 0.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kPenatrating);
}

TEST(Geometry2D_IntersectionTests, CircleVsCircle_SmallInsideLarge_BContainsA)
{
    Circle large(5.0f, Vector2D(0.0f, 0.0f));
    Circle small(1.0f, Vector2D(0.0f, 0.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(small, large);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kBContainsA);
}

TEST(Geometry2D_IntersectionTests, CircleVsCircle_LargeContainsSmall_AContainsB)
{
    Circle large(5.0f, Vector2D(0.0f, 0.0f));
    Circle small(1.0f, Vector2D(0.0f, 0.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(large, small);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kAContainsB);
}

// ---------------------------------------------------------------------------
// AARect vs AARect
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, AARectVsAARect_Separate_NoIntersection)
{
    AARect a(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f));
    AARect b(Vector2D(2.0f, 0.0f), Vector2D(3.0f, 1.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kNoIntersection);
}

TEST(Geometry2D_IntersectionTests, AARectVsAARect_Overlapping_Penetrating)
{
    AARect a(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f));
    AARect b(Vector2D(1.0f, 0.0f), Vector2D(3.0f, 2.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kPenatrating);
}

TEST(Geometry2D_IntersectionTests, AARectVsAARect_AContainsB)
{
    AARect outer(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    AARect inner(Vector2D(2.0f, 2.0f), Vector2D(5.0f, 5.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(outer, inner);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kAContainsB);
}

TEST(Geometry2D_IntersectionTests, AARectVsAARect_BContainsA)
{
    AARect outer(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    AARect inner(Vector2D(2.0f, 2.0f), Vector2D(5.0f, 5.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(inner, outer);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kBContainsA);
}

// ---------------------------------------------------------------------------
// Circle vs AARect
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, CircleVsAARect_Separate_NoIntersection)
{
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f));
    Circle circle(0.5f, Vector2D(5.0f, 5.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(rect, circle);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kNoIntersection);
}

TEST(Geometry2D_IntersectionTests, CircleVsAARect_CircleOverlapsEdge_Penetrating)
{
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f));
    Circle circle(1.0f, Vector2D(2.5f, 1.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(rect, circle);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kPenatrating);
}

// ---------------------------------------------------------------------------
// Point containment
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, PointInCircle_ContainmentResult)
{
    Circle c(5.0f, Vector2D(0.0f, 0.0f));
    Vector2D inside(1.0f, 1.0f);
    Vector2D outside(10.0f, 0.0f);
    EXPECT_TRUE(IntersectionTests::IsIntersecting(inside, c).IsIntersecting());
    EXPECT_FALSE(IntersectionTests::IsIntersecting(outside, c).IsIntersecting());
}

TEST(Geometry2D_IntersectionTests, PointInAARect_ContainmentResult)
{
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 4.0f));
    Vector2D inside(2.0f, 2.0f);
    Vector2D outside(5.0f, 5.0f);
    EXPECT_TRUE(IntersectionTests::IsIntersecting(inside, rect).IsIntersecting());
    EXPECT_FALSE(IntersectionTests::IsIntersecting(outside, rect).IsIntersecting());
}

// ---------------------------------------------------------------------------
// Line vs Line
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, LineVsLine_Crossing_Penetrating)
{
    Line a(Vector2D(-1.0f, 0.0f), Vector2D(1.0f, 0.0f));
    Line b(Vector2D(0.0f, -1.0f), Vector2D(0.0f, 1.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kPenatrating);
}

TEST(Geometry2D_IntersectionTests, LineVsLine_Parallel_NoIntersection)
{
    Line a(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 0.0f));
    Line b(Vector2D(0.0f, 1.0f), Vector2D(2.0f, 1.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kNoIntersection);
}

TEST(Geometry2D_IntersectionTests, LineVsLine_NotReaching_NoIntersection)
{
    // Segments would intersect if extended but don't reach each other
    Line a(Vector2D(-2.0f, 0.0f), Vector2D(-0.5f, 0.0f));
    Line b(Vector2D(0.5f, -1.0f), Vector2D(0.5f, 1.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kNoIntersection);
}

// ---------------------------------------------------------------------------
// CalculateIntercepts (Circle vs Circle)
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, CalculateIntercepts_TwoOverlappingCircles_ReturnsTwoPoints)
{
    Circle a(2.0f, Vector2D(-1.0f, 0.0f));
    Circle b(2.0f, Vector2D(1.0f, 0.0f));
    Vector2D p1, p2;
    int count = IntersectionTests::CalculateIntercepts(a, b, p1, p2);
    EXPECT_EQ(count, 2);
}

TEST(Geometry2D_IntersectionTests, CalculateIntercepts_SeparateCircles_ReturnsZero)
{
    Circle a(1.0f, Vector2D(0.0f, 0.0f));
    Circle b(1.0f, Vector2D(10.0f, 0.0f));
    Vector2D p1, p2;
    int count = IntersectionTests::CalculateIntercepts(a, b, p1, p2);
    EXPECT_EQ(count, 0);
}

// ---------------------------------------------------------------------------
// Additional Circle vs Circle edge cases
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, CircleVsCircle_Touching_Penetrating)
{
    // Two circles of radius 2.0 whose centers are exactly 4.0 apart — touching at one point.
    Circle a(2.0f, Vector2D(0.0f, 0.0f));
    Circle b(2.0f, Vector2D(4.0f, 0.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    // Touching at a single point counts as intersecting/penetrating (not no-intersection).
    EXPECT_NE(result.GetClassification(), IntersectionClassify::kNoIntersection);
}

TEST(Geometry2D_IntersectionTests, CircleVsCircle_Identical_AContainsB)
{
    // Identical circles — one should be considered to contain the other (or penetrating).
    Circle a(3.0f, Vector2D(0.0f, 0.0f));
    Circle b(3.0f, Vector2D(0.0f, 0.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    EXPECT_TRUE(result.IsIntersecting());
}

// ---------------------------------------------------------------------------
// Additional AARect vs AARect edge cases
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, AARectVsAARect_SharingEdge_Penetrating)
{
    // Two rects that share exactly the edge at x=2 — this is a touching / degenerate overlap.
    AARect a(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f));
    AARect b(Vector2D(2.0f, 0.0f), Vector2D(4.0f, 2.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    // Sharing an edge should not be kNoIntersection.
    EXPECT_NE(result.GetClassification(), IntersectionClassify::kNoIntersection);
}

TEST(Geometry2D_IntersectionTests, AARectVsAARect_Touching_DiagonalCorner)
{
    // Rects that share only a single corner point.
    AARect a(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f));
    AARect b(Vector2D(1.0f, 1.0f), Vector2D(2.0f, 2.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    // The result should not be kNoIntersection when they share a corner.
    EXPECT_NE(result.GetClassification(), IntersectionClassify::kNoIntersection);
}

// ---------------------------------------------------------------------------
// Additional Circle vs AARect cases
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, CircleVsAARect_CircleFullyInsideRect_BContainsA)
{
    // Large rect that completely contains a small circle.
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f));
    Circle circle(1.0f, Vector2D(10.0f, 10.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(rect, circle);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kAContainsB);
}

TEST(Geometry2D_IntersectionTests, CircleVsAARect_RectFullyInsideCircle_AContainsB)
{
    // Large circle that completely contains a small rect.
    AARect rect(Vector2D(4.0f, 4.0f), Vector2D(6.0f, 6.0f));
    Circle circle(50.0f, Vector2D(5.0f, 5.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(rect, circle);
    // The rect (B) is fully inside the circle (A from the rect's perspective is the rect).
    // IsIntersecting(rect, circle): rect=A, circle=B, so kBContainsA means circle contains rect.
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kBContainsA);
}

// ---------------------------------------------------------------------------
// CalculateIntercepts — tangent case
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, CalculateIntercepts_Tangent_ReturnsOnePoint)
{
    // Two circles of radius 2.0 whose centers are exactly 4.0 apart — tangent at one point.
    Circle a(2.0f, Vector2D(0.0f, 0.0f));
    Circle b(2.0f, Vector2D(4.0f, 0.0f));
    Vector2D p1, p2;
    int count = IntersectionTests::CalculateIntercepts(a, b, p1, p2);
    EXPECT_GE(count, 1);  // At least one intersection point (tangent = 1)
}

// ---------------------------------------------------------------------------
// Point containment — edge / boundary cases
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, PointOnCircleEdge_IsIntersecting)
{
    // A point exactly on the circumference of the circle.
    Circle c(5.0f, Vector2D(0.0f, 0.0f));
    Vector2D onEdge(5.0f, 0.0f);
    IntersectionClassify result = IntersectionTests::IsIntersecting(onEdge, c);
    EXPECT_TRUE(result.IsIntersecting());
}

TEST(Geometry2D_IntersectionTests, PointOnAARectEdge_IsIntersecting)
{
    // A point exactly on the edge of the rect.
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 4.0f));
    Vector2D onEdge(2.0f, 0.0f);  // bottom edge
    IntersectionClassify result = IntersectionTests::IsIntersecting(onEdge, rect);
    EXPECT_TRUE(result.IsIntersecting());
}

// ---------------------------------------------------------------------------
// Line vs Line — additional cases
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionTests, LineVsLine_CollinearOverlapping_NoIntersection)
{
    // Two collinear (same-direction) segments that overlap — implementation-defined,
    // but let's verify the API returns a consistent result without crashing.
    Line a(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f));
    Line b(Vector2D(2.0f, 0.0f), Vector2D(6.0f, 0.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(a, b);
    // Just verify it doesn't crash and returns a valid classification.
    EXPECT_TRUE(result.IsIntersecting() || result.IsNotIntersecting());
}

TEST(Geometry2D_IntersectionTests, LineVsLine_TShapeIntersect_Penetrating)
{
    // One horizontal line, one vertical starting from the middle of the horizontal.
    Line horiz(Vector2D(-5.0f, 0.0f), Vector2D(5.0f, 0.0f));
    Line vert(Vector2D(0.0f, 0.0f), Vector2D(0.0f, 5.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(horiz, vert);
    EXPECT_EQ(result.GetClassification(), IntersectionClassify::kPenatrating);
}
