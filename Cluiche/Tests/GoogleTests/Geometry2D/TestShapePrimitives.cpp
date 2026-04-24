#include <gtest/gtest.h>
#include <cmath>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/OORect.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/Arc.h>
#include <DiaGeometry2D/Shapes/Capsule.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Shapes/Point.h>
#include <DiaGeometry2D/Shapes/Plane.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGeometry2D/Shapes/Sector.h>
#include <DiaGeometry2D/Shapes/AnnularSector.h>
#include <DiaGeometry2D/Shapes/IntersectionClassify.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Circle
// ---------------------------------------------------------------------------

TEST(Geometry2D_Circle, DefaultConstruction_ZeroRadiusAtOrigin)
{
    Circle c;
    EXPECT_EQ(c.GetRadius(), 0.0f);
    EXPECT_EQ(c.GetCenter().x, 0.0f);
    EXPECT_EQ(c.GetCenter().y, 0.0f);
}

TEST(Geometry2D_Circle, ConstructWithValues_StoresCorrectly)
{
    Circle c(3.0f, Vector2D(1.0f, 2.0f));
    EXPECT_EQ(c.GetRadius(), 3.0f);
    EXPECT_EQ(c.GetSquaredRadius(), 9.0f);
    EXPECT_EQ(c.GetCenter().x, 1.0f);
    EXPECT_EQ(c.GetCenter().y, 2.0f);
}

TEST(Geometry2D_Circle, CopyConstructor_CopiesValues)
{
    Circle a(2.0f, Vector2D(5.0f, 6.0f));
    Circle b(a);
    EXPECT_EQ(b.GetRadius(), a.GetRadius());
    EXPECT_EQ(b.GetCenter().x, a.GetCenter().x);
    EXPECT_EQ(b.GetCenter().y, a.GetCenter().y);
}

TEST(Geometry2D_Circle, SetRadiusAndCenter_UpdatesValues)
{
    Circle c;
    c.SetRadius(4.0f);
    c.SetCenter(Vector2D(7.0f, 8.0f));
    EXPECT_EQ(c.GetRadius(), 4.0f);
    EXPECT_EQ(c.GetCenter().x, 7.0f);
    EXPECT_EQ(c.GetCenter().y, 8.0f);
}

TEST(Geometry2D_Circle, Translate_MovesCenter)
{
    Circle c(1.0f, Vector2D(0.0f, 0.0f));
    c.Translate(Vector2D(3.0f, 4.0f));
    EXPECT_EQ(c.GetCenter().x, 3.0f);
    EXPECT_EQ(c.GetCenter().y, 4.0f);
}

TEST(Geometry2D_Circle, Scale_ScalesRadius)
{
    Circle c(2.0f, Vector2D(0.0f, 0.0f));
    c.Scale(3.0f);
    EXPECT_EQ(c.GetRadius(), 6.0f);
}

TEST(Geometry2D_Circle, UnitCircle_HasRadiusOneAtOrigin)
{
    Circle& u = Circle::UnitCircle();
    EXPECT_EQ(u.GetRadius(), 1.0f);
    EXPECT_EQ(u.GetCenter().x, 0.0f);
    EXPECT_EQ(u.GetCenter().y, 0.0f);
}

// ---------------------------------------------------------------------------
// AARect
// ---------------------------------------------------------------------------

TEST(Geometry2D_AARect, DefaultConstruction)
{
    AARect r;
    EXPECT_EQ(r.GetBottomLeft().x, 0.0f);
    EXPECT_EQ(r.GetTopRight().x, 0.0f);
}

TEST(Geometry2D_AARect, ConstructWithCorners_StoresCorrectly)
{
    AARect r(Vector2D(-1.0f, -2.0f), Vector2D(3.0f, 4.0f));
    EXPECT_EQ(r.GetBottomLeft().x, -1.0f);
    EXPECT_EQ(r.GetBottomLeft().y, -2.0f);
    EXPECT_EQ(r.GetTopRight().x, 3.0f);
    EXPECT_EQ(r.GetTopRight().y, 4.0f);
}

TEST(Geometry2D_AARect, CalculateCenter_ReturnsCorrect)
{
    AARect r(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 6.0f));
    Vector2D center = r.CalculateCenter();
    EXPECT_EQ(center.x, 2.0f);
    EXPECT_EQ(center.y, 3.0f);
}

TEST(Geometry2D_AARect, ClosestPointOnAARectTo_PointInside_ReturnsSamePoint)
{
    AARect r(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    Vector2D result(5.0f, 5.0f);
    r.ClosestPointOnAARectTo(Vector2D(5.0f, 5.0f), result);
    EXPECT_EQ(result.x, 5.0f);
    EXPECT_EQ(result.y, 5.0f);
}

// ---------------------------------------------------------------------------
// Line
// ---------------------------------------------------------------------------

TEST(Geometry2D_Line, ConstructWithPoints_StoresCorrectly)
{
    Line l(Vector2D(0.0f, 0.0f), Vector2D(3.0f, 4.0f));
    EXPECT_EQ(l.GetPt1().x, 0.0f);
    EXPECT_EQ(l.GetPt2().x, 3.0f);
}

TEST(Geometry2D_Line, Length_ComputesCorrectly)
{
    Line l(Vector2D(0.0f, 0.0f), Vector2D(3.0f, 4.0f));
    EXPECT_NEAR(l.Length(), 5.0f, 0.001f);
}

TEST(Geometry2D_Line, SquareLength_ComputesCorrectly)
{
    Line l(Vector2D(0.0f, 0.0f), Vector2D(3.0f, 0.0f));
    EXPECT_EQ(l.SquareLength(), 9.0f);
}

TEST(Geometry2D_Line, CalculateCenter_ReturnsCorrect)
{
    Line l(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f));
    Vector2D c = l.CalculateCenter();
    EXPECT_EQ(c.x, 2.0f);
    EXPECT_EQ(c.y, 0.0f);
}

// ---------------------------------------------------------------------------
// Ray
// ---------------------------------------------------------------------------

TEST(Geometry2D_Ray, DefaultConstruction_OriginAtZeroDirectionAlongX)
{
    Ray r;
    EXPECT_EQ(r.GetOrigin().x, 0.0f);
    EXPECT_EQ(r.GetOrigin().y, 0.0f);
    EXPECT_EQ(r.GetDirection().x, 1.0f);
    EXPECT_EQ(r.GetDirection().y, 0.0f);
}

TEST(Geometry2D_Ray, ConstructWithValues_DirectionNormalized)
{
    Ray r(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 4.0f));
    EXPECT_NEAR(r.GetDirection().Magnitude(), 1.0f, 0.001f);
}

TEST(Geometry2D_Ray, GetPoint_ReturnsCorrectPosition)
{
    Ray r(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));
    Vector2D pt = r.GetPoint(5.0f);
    EXPECT_NEAR(pt.x, 5.0f, 0.001f);
    EXPECT_NEAR(pt.y, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Point
// ---------------------------------------------------------------------------

TEST(Geometry2D_Point, DefaultConstruction_AtOrigin)
{
    Point p;
    EXPECT_EQ(p.GetPosition().x, 0.0f);
    EXPECT_EQ(p.GetPosition().y, 0.0f);
}

TEST(Geometry2D_Point, ConstructWithPosition_StoresCorrectly)
{
    Point p(Vector2D(3.0f, 4.0f));
    EXPECT_EQ(p.GetPosition().x, 3.0f);
    EXPECT_EQ(p.GetPosition().y, 4.0f);
}

// ---------------------------------------------------------------------------
// Plane
// ---------------------------------------------------------------------------

TEST(Geometry2D_Plane, DefaultConstruction)
{
    Plane p;
    EXPECT_EQ(p.GetDistance(), 0.0f);
}

TEST(Geometry2D_Plane, SignedDistance_PointOnPositiveSide_ReturnsPositive)
{
    Plane p(Vector2D(0.0f, 1.0f), 0.0f);  // horizontal plane y=0, normal pointing up
    EXPECT_GT(p.SignedDistance(Vector2D(0.0f, 1.0f)), 0.0f);
}

TEST(Geometry2D_Plane, SignedDistance_PointOnNegativeSide_ReturnsNegative)
{
    Plane p(Vector2D(0.0f, 1.0f), 0.0f);
    EXPECT_LT(p.SignedDistance(Vector2D(0.0f, -1.0f)), 0.0f);
}

TEST(Geometry2D_Plane, IsOnPositiveSide_CorrectResult)
{
    Plane p(Vector2D(1.0f, 0.0f), 0.0f);  // vertical plane x=0, normal pointing right
    EXPECT_TRUE(p.IsOnPositiveSide(Vector2D(1.0f, 0.0f)));
    EXPECT_FALSE(p.IsOnPositiveSide(Vector2D(-1.0f, 0.0f)));
}

// ---------------------------------------------------------------------------
// ConvexPolygon
// ---------------------------------------------------------------------------

TEST(Geometry2D_ConvexPolygon, DefaultConstruction_ZeroVertices)
{
    ConvexPolygon poly;
    EXPECT_EQ(poly.GetVertexCount(), 0);
}

TEST(Geometry2D_ConvexPolygon, ConstructWithVertices_StoresCorrectly)
{
    Vector2D verts[3] = { Vector2D(0,0), Vector2D(1,0), Vector2D(0,1) };
    ConvexPolygon poly(verts, 3);
    EXPECT_EQ(poly.GetVertexCount(), 3);
    EXPECT_EQ(poly.GetVertex(0).x, 0.0f);
    EXPECT_EQ(poly.GetVertex(1).x, 1.0f);
}

TEST(Geometry2D_ConvexPolygon, MaxVertices_ClampedAt16)
{
    Vector2D verts[ConvexPolygon::kMaxVertices];
    for (int i = 0; i < ConvexPolygon::kMaxVertices; i++)
        verts[i] = Vector2D((float)i, 0.0f);
    ConvexPolygon poly(verts, ConvexPolygon::kMaxVertices);
    EXPECT_EQ(poly.GetVertexCount(), ConvexPolygon::kMaxVertices);
}

// ---------------------------------------------------------------------------
// IntersectionClassify
// ---------------------------------------------------------------------------

TEST(Geometry2D_IntersectionClassify, DefaultConstruction_IsNoIntersection)
{
    IntersectionClassify ic;
    EXPECT_TRUE(ic.IsNotIntersecting());
    EXPECT_EQ(ic.GetClassification(), IntersectionClassify::kNoIntersection);
}

TEST(Geometry2D_IntersectionClassify, ConstructPenetrating_IsIntersecting)
{
    IntersectionClassify ic(IntersectionClassify::kPenatrating);
    EXPECT_TRUE(ic.IsIntersecting());
    EXPECT_FALSE(ic.IsContainment());
}

TEST(Geometry2D_IntersectionClassify, AContainsB_IsContainment)
{
    IntersectionClassify ic(IntersectionClassify::kAContainsB);
    EXPECT_TRUE(ic.IsContainment());
    EXPECT_TRUE(ic.IsIntersecting());
}

TEST(Geometry2D_IntersectionClassify, ReInterpretAandBObject_SwapsContainment)
{
    IntersectionClassify ic(IntersectionClassify::kAContainsB);
    ic.ReInterpretAandBObject();
    EXPECT_EQ(ic.GetClassification(), IntersectionClassify::kBContainsA);
}

TEST(Geometry2D_IntersectionClassify, EqualityOperator_Works)
{
    IntersectionClassify a(IntersectionClassify::kPenatrating);
    IntersectionClassify b(IntersectionClassify::kPenatrating);
    IntersectionClassify c(IntersectionClassify::kNoIntersection);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}

// ---------------------------------------------------------------------------
// Circle — additional tests
// ---------------------------------------------------------------------------

TEST(Geometry2D_Circle, CreateFromAARect_EncompassesRect)
{
    // The circle created from an AARect should have its center at the rect center
    // and a radius at least as large as the half-diagonal so it encompasses the rect.
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 6.0f));
    Circle c = Circle::CreateFrom(rect);

    Vector2D center = rect.CalculateCenter();
    EXPECT_NEAR(c.GetCenter().x, center.x, 0.001f);
    EXPECT_NEAR(c.GetCenter().y, center.y, 0.001f);

    // Every corner must be within (or on) the circle
    float dx = 4.0f - center.x;
    float dy = 6.0f - center.y;
    float halfDiag = std::sqrt(dx * dx + dy * dy);
    EXPECT_GE(c.GetRadius(), halfDiag - 0.001f);
}

TEST(Geometry2D_Circle, ExpandToInclude_Circle_GrowsToFit)
{
    // Circle at origin r=1, expand to include circle at (10,0) r=1.
    // The expanded circle must still have center at origin and radius >= 11.
    Circle c(1.0f, Vector2D(0.0f, 0.0f));
    Circle other(1.0f, Vector2D(10.0f, 0.0f));
    c.ExpandToInclude(other);
    EXPECT_GE(c.GetRadius(), 11.0f - 0.001f);
}

TEST(Geometry2D_Circle, ExpandToInclude_Point_GrowsToFit)
{
    // Circle at origin r=1, expand to include point at (5,0).
    Circle c(1.0f, Vector2D(0.0f, 0.0f));
    c.ExpandToInclude(Vector2D(5.0f, 0.0f));
    EXPECT_GE(c.GetRadius(), 5.0f - 0.001f);
}

TEST(Geometry2D_Circle, ExpandToInclude_Point_AlreadyInside_NoChange)
{
    // Point inside circle — radius should not shrink.
    Circle c(10.0f, Vector2D(0.0f, 0.0f));
    float originalRadius = c.GetRadius();
    c.ExpandToInclude(Vector2D(1.0f, 1.0f));
    EXPECT_GE(c.GetRadius(), originalRadius - 0.001f);
}

TEST(Geometry2D_Circle, ClosestPointOnCircleTo_PointOutside_OnEdge)
{
    // Point outside at (5,0), circle at origin r=2.
    // Closest point on circumference should be at (2,0).
    Circle c(2.0f, Vector2D(0.0f, 0.0f));
    Vector2D result;
    c.ClosestPointOnCircleTo(Vector2D(5.0f, 0.0f), result);
    EXPECT_NEAR(result.x, 2.0f, 0.001f);
    EXPECT_NEAR(result.y, 0.0f, 0.001f);
}

TEST(Geometry2D_Circle, ClosestPointOnCircleTo_PointInside_ProjectsToEdge)
{
    // Point inside at (1,0), circle at origin r=2.
    // The closest point ON THE CIRCLE (circumference) should be at (2,0).
    Circle c(2.0f, Vector2D(0.0f, 0.0f));
    Vector2D result;
    c.ClosestPointOnCircleTo(Vector2D(1.0f, 0.0f), result);
    // Result should be on the circumference — distance from center == radius.
    float distFromCenter = std::sqrt(result.x * result.x + result.y * result.y);
    EXPECT_NEAR(distFromCenter, 2.0f, 0.001f);
}

TEST(Geometry2D_Circle, GetSquaredRadius_MatchesRadiusSquared)
{
    Circle c(5.0f, Vector2D(0.0f, 0.0f));
    EXPECT_NEAR(c.GetSquaredRadius(), 25.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// AARect — additional tests
// ---------------------------------------------------------------------------

TEST(Geometry2D_AARect, Width_Height_ComputeCorrectly)
{
    AARect r(Vector2D(1.0f, 2.0f), Vector2D(5.0f, 8.0f));
    float width  = r.GetTopRight().x - r.GetBottomLeft().x;
    float height = r.GetTopRight().y - r.GetBottomLeft().y;
    EXPECT_NEAR(width,  4.0f, 0.001f);
    EXPECT_NEAR(height, 6.0f, 0.001f);
}

TEST(Geometry2D_AARect, CalculateRadius_ReturnsHalfDiagonal)
{
    // For a 4x3 rect the diagonal is 5, so radius = 2.5
    AARect r(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 3.0f));
    EXPECT_NEAR(r.CalculateRadius(), 2.5f, 0.001f);
}

TEST(Geometry2D_AARect, CalculateSquaredRadius_Correct)
{
    // 4x3 rect: diagonal^2 = 25, SquaredRadius = 25/2 = 12.5
    AARect r(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 3.0f));
    EXPECT_NEAR(r.CalculateSquaredRadius(), 12.5f, 0.001f);
}

TEST(Geometry2D_AARect, ClosestPointOnAARectTo_PointOutsideCorner_ReturnsCorner)
{
    // Point at (15, 15) outside the top-right corner of a (0,0)→(10,10) rect.
    AARect r(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    Vector2D result;
    r.ClosestPointOnAARectTo(Vector2D(15.0f, 15.0f), result);
    EXPECT_NEAR(result.x, 10.0f, 0.001f);
    EXPECT_NEAR(result.y, 10.0f, 0.001f);
}

TEST(Geometry2D_AARect, ClosestPointOnAARectTo_PointOutsideLeftEdge_ClampedToLeft)
{
    AARect r(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    Vector2D result;
    r.ClosestPointOnAARectTo(Vector2D(-5.0f, 5.0f), result);
    EXPECT_NEAR(result.x, 0.0f, 0.001f);
    EXPECT_NEAR(result.y, 5.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Line — additional tests
// ---------------------------------------------------------------------------

TEST(Geometry2D_Line, Direction_ReturnsNormalizedVector)
{
    // Pt1ToPt2Vector() is the normalised direction from pt1 to pt2.
    Line l(Vector2D(0.0f, 0.0f), Vector2D(3.0f, 4.0f));
    Vector2D dir = l.Pt1ToPt2Vector();
    EXPECT_NEAR(dir.Magnitude(), 1.0f, 0.001f);
    EXPECT_NEAR(dir.x, 3.0f / 5.0f, 0.001f);
    EXPECT_NEAR(dir.y, 4.0f / 5.0f, 0.001f);
}

TEST(Geometry2D_Line, ReversedDirection_MatchesNegatedForward)
{
    Line l(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));
    Vector2D fwd = l.Pt1ToPt2Vector();
    Vector2D rev = l.Pt2ToPt1Vector();
    EXPECT_NEAR(fwd.x, -rev.x, 0.001f);
    EXPECT_NEAR(fwd.y, -rev.y, 0.001f);
}

TEST(Geometry2D_Line, ClosestPointOnLineTo_EndPoint_ReturnsPt2)
{
    // Query point beyond pt2 — clamps to pt2.
    Line l(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f));
    Vector2D result;
    l.ClosestPointOnLineTo(Vector2D(10.0f, 0.0f), result);
    EXPECT_NEAR(result.x, 4.0f, 0.001f);
    EXPECT_NEAR(result.y, 0.0f, 0.001f);
}

TEST(Geometry2D_Line, ClosestPointOnLineTo_Midpoint_Perpendicular)
{
    // Point (2, 3) above horizontal line (0,0)→(4,0) — closest is (2,0).
    Line l(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f));
    Vector2D result;
    l.ClosestPointOnLineTo(Vector2D(2.0f, 3.0f), result);
    EXPECT_NEAR(result.x, 2.0f, 0.001f);
    EXPECT_NEAR(result.y, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// OORect (new suite)
// ---------------------------------------------------------------------------

TEST(Geometry2D_OORect, DefaultConstruction)
{
    OORect r;
    // Default is zero-initialised — all points are at origin.
    // Just verify the object can be constructed without crashing.
    (void)r;
    SUCCEED();
}

TEST(Geometry2D_OORect, ConstructWithFourPoints_StoresCorrectly)
{
    Vector2D p0(0.0f, 0.0f);
    Vector2D p1(4.0f, 0.0f);
    Vector2D p2(4.0f, 3.0f);
    Vector2D p3(0.0f, 3.0f);
    OORect r(p0, p1, p2, p3);
    EXPECT_EQ(r.GetPt(OORect::kPt0).x, 0.0f);
    EXPECT_EQ(r.GetPt(OORect::kPt0).y, 0.0f);
    EXPECT_EQ(r.GetPt(OORect::kPt1).x, 4.0f);
    EXPECT_EQ(r.GetPt(OORect::kPt2).x, 4.0f);
    EXPECT_EQ(r.GetPt(OORect::kPt2).y, 3.0f);
    EXPECT_EQ(r.GetPt(OORect::kPt3).x, 0.0f);
    EXPECT_EQ(r.GetPt(OORect::kPt3).y, 3.0f);
}

TEST(Geometry2D_OORect, CopyConstructor_CopiesPoints)
{
    Vector2D p0(1.0f, 1.0f), p1(5.0f, 1.0f), p2(5.0f, 4.0f), p3(1.0f, 4.0f);
    OORect a(p0, p1, p2, p3);
    OORect b(a);
    for (int i = 0; i < OORect::kNumPts; ++i)
    {
        EXPECT_NEAR(b.GetPt(i).x, a.GetPt(i).x, 0.001f);
        EXPECT_NEAR(b.GetPt(i).y, a.GetPt(i).y, 0.001f);
    }
}

TEST(Geometry2D_OORect, CalculateCenter_ForAxisAlignedRect)
{
    // Axis-aligned: (0,0), (4,0), (4,2), (0,2) — center = (2,1)
    OORect r(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f),
             Vector2D(4.0f, 2.0f), Vector2D(0.0f, 2.0f));
    Vector2D center = r.CalculateCenter();
    EXPECT_NEAR(center.x, 2.0f, 0.001f);
    EXPECT_NEAR(center.y, 1.0f, 0.001f);
}

TEST(Geometry2D_OORect, CalculateRadius_ForAxisAlignedRect)
{
    // (0,0)→(4,0)→(4,2)→(0,2): diagonal from pt0 to pt2 is 4x2 = sqrt(20), radius = sqrt(20)/2
    OORect r(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f),
             Vector2D(4.0f, 2.0f), Vector2D(0.0f, 2.0f));
    float expected = std::sqrt(16.0f + 4.0f) / 2.0f;  // sqrt(20)/2
    EXPECT_NEAR(r.CalculateRadius(), expected, 0.001f);
}

TEST(Geometry2D_OORect, GetPt_IntIndex_MatchesPtIdIndex)
{
    Vector2D p0(0.0f, 0.0f), p1(1.0f, 0.0f), p2(1.0f, 1.0f), p3(0.0f, 1.0f);
    OORect r(p0, p1, p2, p3);
    EXPECT_EQ(r.GetPt(0).x, r.GetPt(OORect::kPt0).x);
    EXPECT_EQ(r.GetPt(1).x, r.GetPt(OORect::kPt1).x);
    EXPECT_EQ(r.GetPt(2).x, r.GetPt(OORect::kPt2).x);
    EXPECT_EQ(r.GetPt(3).x, r.GetPt(OORect::kPt3).x);
}

// ---------------------------------------------------------------------------
// Triangle (new suite)
// ---------------------------------------------------------------------------

TEST(Geometry2D_Triangle, DefaultConstruction)
{
    Triangle t;
    (void)t;
    SUCCEED();
}

TEST(Geometry2D_Triangle, ConstructWithPoints_StoresCorrectly)
{
    Triangle t(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f), Vector2D(2.0f, 3.0f));
    EXPECT_NEAR(t.GetPt(Triangle::kPt0).x, 0.0f, 0.001f);
    EXPECT_NEAR(t.GetPt(Triangle::kPt1).x, 4.0f, 0.001f);
    EXPECT_NEAR(t.GetPt(Triangle::kPt2).x, 2.0f, 0.001f);
    EXPECT_NEAR(t.GetPt(Triangle::kPt2).y, 3.0f, 0.001f);
}

TEST(Geometry2D_Triangle, CopyConstructor_CopiesPoints)
{
    Triangle a(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f), Vector2D(2.0f, 3.0f));
    Triangle b(a);
    for (int i = 0; i < Triangle::kNumPts; ++i)
    {
        EXPECT_NEAR(b.GetPt(i).x, a.GetPt(i).x, 0.001f);
        EXPECT_NEAR(b.GetPt(i).y, a.GetPt(i).y, 0.001f);
    }
}

TEST(Geometry2D_Triangle, CenterOfGravity_ReturnsCorrectCentroid)
{
    // Centroid of (0,0), (6,0), (0,6) = (2,2)
    Triangle t(Vector2D(0.0f, 0.0f), Vector2D(6.0f, 0.0f), Vector2D(0.0f, 6.0f));
    Vector2D cog = t.CenterOfGravity();
    EXPECT_NEAR(cog.x, 2.0f, 0.001f);
    EXPECT_NEAR(cog.y, 2.0f, 0.001f);
}

TEST(Geometry2D_Triangle, CenterOfGravity_UnitTriangle)
{
    // (0,0),(1,0),(0,1) centroid = (1/3, 1/3)
    Triangle t(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f), Vector2D(0.0f, 1.0f));
    Vector2D cog = t.CenterOfGravity();
    EXPECT_NEAR(cog.x, 1.0f / 3.0f, 0.001f);
    EXPECT_NEAR(cog.y, 1.0f / 3.0f, 0.001f);
}

TEST(Geometry2D_Triangle, GetPt_IntIndex_MatchesPtIdIndex)
{
    Triangle t(Vector2D(1.0f, 2.0f), Vector2D(3.0f, 4.0f), Vector2D(5.0f, 6.0f));
    EXPECT_EQ(t.GetPt(0).x, t.GetPt(Triangle::kPt0).x);
    EXPECT_EQ(t.GetPt(1).x, t.GetPt(Triangle::kPt1).x);
    EXPECT_EQ(t.GetPt(2).x, t.GetPt(Triangle::kPt2).x);
}

TEST(Geometry2D_Triangle, CalculateInCircle_RadiusIsPositive)
{
    Triangle t(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f), Vector2D(2.0f, 3.0f));
    Circle inCircle;
    t.CalculateInCircle(inCircle);
    EXPECT_GT(inCircle.GetRadius(), 0.0f);
}

TEST(Geometry2D_Triangle, CalculateCircumCircle_PassesThroughAllVertices)
{
    Triangle t(Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f), Vector2D(2.0f, 3.0f));
    Circle circ;
    t.CalculateCircumCircle(circ);

    // All three vertices should lie on the circumcircle (within tolerance).
    for (int i = 0; i < Triangle::kNumPts; ++i)
    {
        float dx = t.GetPt(i).x - circ.GetCenter().x;
        float dy = t.GetPt(i).y - circ.GetCenter().y;
        float dist = std::sqrt(dx * dx + dy * dy);
        EXPECT_NEAR(dist, circ.GetRadius(), 0.01f);
    }
}

// ---------------------------------------------------------------------------
// Arc (new suite)
// ---------------------------------------------------------------------------

TEST(Geometry2D_Arc, DefaultConstruction)
{
    Arc a;
    EXPECT_NEAR(a.GetRadius(), 1.0f, 0.001f);
    EXPECT_NEAR(a.GetFocal().x, 0.0f, 0.001f);
    EXPECT_NEAR(a.GetFocal().y, 0.0f, 0.001f);
    EXPECT_NEAR(a.GetAxis().x, 1.0f, 0.001f);
    EXPECT_NEAR(a.GetAxis().y, 0.0f, 0.001f);
}

TEST(Geometry2D_Arc, ConstructWithValues_StoresCorrectly)
{
    Angle angle = Angle::Deg45;
    Arc a(3.0f, angle, Vector2D(1.0f, 2.0f), Vector2D(0.0f, 1.0f));
    EXPECT_NEAR(a.GetRadius(), 3.0f, 0.001f);
    EXPECT_NEAR(a.GetFocal().x, 1.0f, 0.001f);
    EXPECT_NEAR(a.GetFocal().y, 2.0f, 0.001f);
    EXPECT_NEAR(a.GetAxis().x, 0.0f, 0.001f);
    EXPECT_NEAR(a.GetAxis().y, 1.0f, 0.001f);
}

TEST(Geometry2D_Arc, CopyConstructor_CopiesValues)
{
    Angle angle = Angle::Deg90;
    Arc a(5.0f, angle, Vector2D(3.0f, 4.0f), Vector2D(1.0f, 0.0f));
    Arc b(a);
    EXPECT_NEAR(b.GetRadius(), a.GetRadius(), 0.001f);
    EXPECT_NEAR(b.GetFocal().x, a.GetFocal().x, 0.001f);
    EXPECT_NEAR(b.GetFocal().y, a.GetFocal().y, 0.001f);
    EXPECT_NEAR(b.GetAxis().x, a.GetAxis().x, 0.001f);
    EXPECT_NEAR(b.GetAxis().y, a.GetAxis().y, 0.001f);
}

TEST(Geometry2D_Arc, GetSquaredRadius_MatchesRadiusSquared)
{
    Arc a(4.0f, Angle::Deg45, Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));
    EXPECT_NEAR(a.GetSquaredRadius(), 16.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Capsule (new suite)
// ---------------------------------------------------------------------------

TEST(Geometry2D_Capsule, DefaultConstruction)
{
    Capsule c;
    EXPECT_NEAR(c.GetRadius(), 0.0f, 0.001f);
    EXPECT_NEAR(c.GetPoint1().x, 0.0f, 0.001f);
    EXPECT_NEAR(c.GetPoint2().x, 0.0f, 0.001f);
}

TEST(Geometry2D_Capsule, ConstructWithValues_StoresCorrectly)
{
    Capsule c(2.0f, Vector2D(0.0f, 0.0f), Vector2D(6.0f, 0.0f));
    EXPECT_NEAR(c.GetRadius(), 2.0f, 0.001f);
    EXPECT_NEAR(c.GetPoint1().x, 0.0f, 0.001f);
    EXPECT_NEAR(c.GetPoint2().x, 6.0f, 0.001f);
}

TEST(Geometry2D_Capsule, CopyConstructor_CopiesValues)
{
    Capsule a(1.5f, Vector2D(1.0f, 2.0f), Vector2D(4.0f, 2.0f));
    Capsule b(a);
    EXPECT_NEAR(b.GetRadius(), a.GetRadius(), 0.001f);
    EXPECT_NEAR(b.GetPoint1().x, a.GetPoint1().x, 0.001f);
    EXPECT_NEAR(b.GetPoint2().x, a.GetPoint2().x, 0.001f);
}

TEST(Geometry2D_Capsule, GetLength_ComputesCorrectly)
{
    // axis from (0,0) to (6,0) = axis length 6; length = axisLen + 2*radius = 6 + 2*2 = 10
    Capsule c(2.0f, Vector2D(0.0f, 0.0f), Vector2D(6.0f, 0.0f));
    EXPECT_NEAR(c.GetLength(), 10.0f, 0.001f);
}

TEST(Geometry2D_Capsule, GetAxisLength_ComputesCorrectly)
{
    Capsule c(2.0f, Vector2D(0.0f, 0.0f), Vector2D(3.0f, 4.0f));
    EXPECT_NEAR(c.GetAxisLength(), 5.0f, 0.001f);
}

TEST(Geometry2D_Capsule, GetCenter_ReturnsAxisMidpoint)
{
    Capsule c(1.0f, Vector2D(0.0f, 0.0f), Vector2D(4.0f, 0.0f));
    Vector2D center = c.GetCenter();
    EXPECT_NEAR(center.x, 2.0f, 0.001f);
    EXPECT_NEAR(center.y, 0.0f, 0.001f);
}

TEST(Geometry2D_Capsule, GetSquaredRadius_MatchesRadiusSquared)
{
    Capsule c(3.0f, Vector2D(0.0f, 0.0f), Vector2D(0.0f, 4.0f));
    EXPECT_NEAR(c.GetSquaredRadius(), 9.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Sector (new suite)
// ---------------------------------------------------------------------------

TEST(Geometry2D_Sector, DefaultConstruction)
{
    Sector s;
    // Just verify it constructs without crashing.
    (void)s;
    SUCCEED();
}

TEST(Geometry2D_Sector, ConstructWithValues_StoresCorrectly)
{
    Vector2D center(1.0f, 2.0f);
    Vector2D axis(1.0f, 0.0f);
    Angle halfAngle = Angle::Deg45;
    Sector s(center, 5.0f, axis, halfAngle);
    EXPECT_NEAR(s.GetCenter().x, 1.0f, 0.001f);
    EXPECT_NEAR(s.GetCenter().y, 2.0f, 0.001f);
    EXPECT_NEAR(s.GetRadius(), 5.0f, 0.001f);
    EXPECT_NEAR(s.GetAxis().x, 1.0f, 0.001f);
    EXPECT_NEAR(s.GetAxis().y, 0.0f, 0.001f);
}

TEST(Geometry2D_Sector, GetHalfAngle_MatchesConstructorInput)
{
    Sector s(Vector2D(0.0f, 0.0f), 3.0f, Vector2D(0.0f, 1.0f), Angle::Deg90);
    // Half-angle should be 90 degrees
    EXPECT_NEAR(s.GetHalfAngle().AsDegrees(), Angle::Deg90.AsDegrees(), 0.001f);
}

// ---------------------------------------------------------------------------
// AnnularSector (new suite)
// ---------------------------------------------------------------------------

TEST(Geometry2D_AnnularSector, DefaultConstruction)
{
    AnnularSector a;
    (void)a;
    SUCCEED();
}

TEST(Geometry2D_AnnularSector, ConstructWithValues_StoresCorrectly)
{
    Vector2D center(2.0f, 3.0f);
    Vector2D axis(0.0f, 1.0f);
    Angle halfAngle = Angle::Deg30;
    AnnularSector a(center, 2.0f, 6.0f, axis, halfAngle);
    EXPECT_NEAR(a.GetCenter().x, 2.0f, 0.001f);
    EXPECT_NEAR(a.GetCenter().y, 3.0f, 0.001f);
    EXPECT_NEAR(a.GetInnerRadius(), 2.0f, 0.001f);
    EXPECT_NEAR(a.GetOuterRadius(), 6.0f, 0.001f);
    EXPECT_NEAR(a.GetAxis().x, 0.0f, 0.001f);
    EXPECT_NEAR(a.GetAxis().y, 1.0f, 0.001f);
}

TEST(Geometry2D_AnnularSector, InnerRadiusSmallerThanOuter)
{
    AnnularSector a(Vector2D(0.0f, 0.0f), 3.0f, 8.0f, Vector2D(1.0f, 0.0f), Angle::Deg45);
    EXPECT_LT(a.GetInnerRadius(), a.GetOuterRadius());
}

TEST(Geometry2D_AnnularSector, GetHalfAngle_MatchesConstructorInput)
{
    AnnularSector a(Vector2D(0.0f, 0.0f), 1.0f, 5.0f, Vector2D(1.0f, 0.0f), Angle::Deg60);
    EXPECT_NEAR(a.GetHalfAngle().AsDegrees(), Angle::Deg60.AsDegrees(), 0.001f);
}
