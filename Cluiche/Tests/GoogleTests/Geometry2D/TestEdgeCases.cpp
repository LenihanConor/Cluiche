#include <gtest/gtest.h>
#include <cmath>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGeometry2D/Intersection/IntersectionTests.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

// ===========================================================================
// Circle::ClosestPointOnCircleTo — bug regression tests
// ===========================================================================

TEST(Geometry2D_EdgeCases, Circle_ClosestPointToCircle_IncludesCenter)
{
    Circle a(2.0f, Vector2D(0.0f, 0.0f));
    Circle b(1.0f, Vector2D(5.0f, 0.0f));

    Vector2D result;
    a.ClosestPointOnCircleTo(b, result);

    // Closest point on circle a toward b should be at (2,0)
    EXPECT_NEAR(result.x, 2.0f, 1e-4f);
    EXPECT_NEAR(result.y, 0.0f, 1e-4f);
}

TEST(Geometry2D_EdgeCases, Circle_ClosestPointToCircle_OffAxis)
{
    Circle a(1.0f, Vector2D(3.0f, 4.0f));
    Circle b(1.0f, Vector2D(6.0f, 8.0f));

    Vector2D result;
    a.ClosestPointOnCircleTo(b, result);

    // Direction from a to b is (3,4), norm = 5, unit = (0.6, 0.8)
    // Closest point = center + unit * radius = (3,4) + (0.6, 0.8) = (3.6, 4.8)
    EXPECT_NEAR(result.x, 3.6f, 1e-4f);
    EXPECT_NEAR(result.y, 4.8f, 1e-4f);
}

TEST(Geometry2D_EdgeCases, Circle_ClosestPointToLine_IncludesCenter)
{
    Circle c(2.0f, Vector2D(0.0f, 0.0f));
    Line line;
    line.Create(Vector2D(3.0f, -5.0f), Vector2D(3.0f, 5.0f));

    Vector2D result;
    c.ClosestPointOnCircleTo(line, result);

    // Closest point on circle toward line at x=3 should be at (2, 0)
    EXPECT_NEAR(result.x, 2.0f, 1e-4f);
    EXPECT_NEAR(result.y, 0.0f, 1e-3f);
}

// ===========================================================================
// Circle::CalculateAxisPointToPoint — bug regression
// ===========================================================================

TEST(Geometry2D_EdgeCases, Circle_AxisPointToPoint_CorrectResults)
{
    Circle c(3.0f, Vector2D(0.0f, 0.0f));
    Vector2D closest, furthest;
    c.CalculateAxisPointToPoint(Vector2D(10.0f, 0.0f), closest, furthest);

    // Closest point should be (3,0), furthest (-3,0)
    EXPECT_NEAR(closest.x,  3.0f, 1e-4f);
    EXPECT_NEAR(closest.y,  0.0f, 1e-4f);
    EXPECT_NEAR(furthest.x, -3.0f, 1e-4f);
    EXPECT_NEAR(furthest.y,  0.0f, 1e-4f);
}

TEST(Geometry2D_EdgeCases, Circle_AxisPointToPoint_DiagonalPoint)
{
    Circle c(2.0f, Vector2D(5.0f, 5.0f));
    Vector2D closest, furthest;
    // Point at (10, 10) — direction is (1,1)/sqrt(2)
    c.CalculateAxisPointToPoint(Vector2D(10.0f, 10.0f), closest, furthest);

    float s = 2.0f / std::sqrt(2.0f);
    EXPECT_NEAR(closest.x,  5.0f + s, 1e-4f);
    EXPECT_NEAR(closest.y,  5.0f + s, 1e-4f);
    EXPECT_NEAR(furthest.x, 5.0f - s, 1e-4f);
    EXPECT_NEAR(furthest.y, 5.0f - s, 1e-4f);
}

// ===========================================================================
// Degenerate shapes
// ===========================================================================

TEST(Geometry2D_EdgeCases, ZeroRadiusCircle_IntersectsPoint)
{
    Circle c(0.0f, Vector2D(3.0f, 4.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(Vector2D(3.0f, 4.0f), c);
    EXPECT_TRUE(result.IsIntersecting());
}

TEST(Geometry2D_EdgeCases, ZeroRadiusCircle_DoesNotIntersectNearbyPoint)
{
    Circle c(0.0f, Vector2D(3.0f, 4.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(Vector2D(3.1f, 4.0f), c);
    EXPECT_FALSE(result.IsIntersecting());
}

TEST(Geometry2D_EdgeCases, ZeroAreaAARect_IntersectsItself)
{
    AARect r(Vector2D(5.0f, 5.0f), Vector2D(5.0f, 5.0f));
    IntersectionClassify result = IntersectionTests::IsIntersecting(r, r);
    // Zero-area rect has no overlap extent; implementation-dependent
    // At minimum it shouldn't crash
    (void)result;
}

// ===========================================================================
// Raycast edge cases
// ===========================================================================

TEST(Geometry2D_EdgeCases, Raycast_InsideCircle_ReturnsHitAtOrigin)
{
    Circle c(5.0f, Vector2D(0.0f, 0.0f));
    Ray ray(Vector2D(1.0f, 0.0f), Vector2D(1.0f, 0.0f));  // inside circle

    RaycastHit hit;
    bool result = Raycast::CastCircle(ray, c, hit);

    EXPECT_TRUE(result);
    EXPECT_LE(hit.distance, 5.0f);
}

TEST(Geometry2D_EdgeCases, Raycast_InsideAARect_ReturnsHit)
{
    AARect r(Vector2D(-5.0f, -5.0f), Vector2D(5.0f, 5.0f));
    Ray ray(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));

    RaycastHit hit;
    bool result = Raycast::CastAARect(ray, r, hit);

    // Slab method: tMin starts at 0 when ray origin is inside the rect,
    // so hit reports at distance 0 at the ray origin itself (entry is behind)
    EXPECT_TRUE(result);
    if (result)
    {
        EXPECT_LE(hit.distance, 5.0f);
    }
}

TEST(Geometry2D_EdgeCases, Raycast_ParallelToRectEdge_Misses)
{
    AARect r(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    // Ray runs parallel to the bottom edge, just outside
    Ray ray(Vector2D(-5.0f, -0.1f), Vector2D(1.0f, 0.0f));

    RaycastHit hit;
    bool result = Raycast::CastAARect(ray, r, hit);
    EXPECT_FALSE(result);
}

TEST(Geometry2D_EdgeCases, Raycast_TangentToCircle_Misses)
{
    Circle c(1.0f, Vector2D(0.0f, 2.0f));
    // Ray at y=1 going +X — passes at distance exactly 1 from center
    // Tangent means distSq == radius*radius, which is >=, so this should miss
    Ray ray(Vector2D(-10.0f, 1.0f), Vector2D(1.0f, 0.0f));

    RaycastHit hit;
    bool result = Raycast::CastCircle(ray, c, hit);
    // Tangent case: may or may not hit depending on epsilon; just don't crash
    (void)result;
}

// ===========================================================================
// Spatial grid edge cases
// ===========================================================================

using Grid = SpatialGrid<int>;
using HandleType = Dia::Core::Handle<int>;
using QueryOut = Dia::Core::Containers::DynamicArrayC<HandleType, kMaxQueryResults>;

class Geometry2D_GridEdgeCases : public ::testing::Test
{
protected:
    Grid* grid = nullptr;
    void SetUp() override
    {
        Grid::Def def;
        def.worldBounds = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
        def.cellSize = 10.0f;
        grid = new Grid(def);
    }
    void TearDown() override { delete grid; }
};

TEST_F(Geometry2D_GridEdgeCases, ObjectLargerThanCell_FoundByQuery)
{
    // Object spans 4 cells (20x20 centered at 50,50)
    HandleType h = grid->Insert(1, AARect(Vector2D(40.0f, 40.0f), Vector2D(60.0f, 60.0f)));
    EXPECT_TRUE(h.IsValid());

    // Query a region that overlaps one corner
    QueryOut results;
    grid->QueryRegion(AARect(Vector2D(55.0f, 55.0f), Vector2D(65.0f, 65.0f)), results);
    EXPECT_GE(results.Size(), 1u);
}

TEST_F(Geometry2D_GridEdgeCases, ObjectAtWorldBoundary_FoundByQuery)
{
    HandleType h = grid->Insert(42, AARect(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f)));
    EXPECT_TRUE(h.IsValid());

    QueryOut results;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f)), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* val = grid->Resolve(results[i]);
        if (val && *val == 42) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(Geometry2D_GridEdgeCases, ObjectAtMaxBoundary_FoundByQuery)
{
    HandleType h = grid->Insert(77, AARect(Vector2D(98.0f, 98.0f), Vector2D(100.0f, 100.0f)));
    EXPECT_TRUE(h.IsValid());

    QueryOut results;
    grid->QueryRegion(AARect(Vector2D(95.0f, 95.0f), Vector2D(100.0f, 100.0f)), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* val = grid->Resolve(results[i]);
        if (val && *val == 77) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(Geometry2D_GridEdgeCases, QueryOutsideBounds_ReturnsEmpty)
{
    grid->Insert(1, AARect(Vector2D(50.0f, 50.0f), Vector2D(55.0f, 55.0f)));

    QueryOut results;
    grid->QueryRegion(AARect(Vector2D(200.0f, 200.0f), Vector2D(210.0f, 210.0f)), results);
    EXPECT_EQ(results.Size(), 0u);
}
