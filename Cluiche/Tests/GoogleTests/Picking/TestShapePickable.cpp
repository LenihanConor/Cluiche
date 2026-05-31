////////////////////////////////////////////////////////////////////////////////
// Filename: TestShapePickable.cpp
// Tests: ShapePickable — Circle hit/miss, AARect hit/miss, ConvexPolygon hit/miss
// AC5
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaGeometry2DPicking/Adapters/ShapePickable.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;
using namespace Dia::Geometry2DPicking;

// ---------------------------------------------------------------------------
// Circle
// ---------------------------------------------------------------------------

TEST(ShapePickable_Circle, TryPick_Centre_Hits)
{
    Circle circle(10.0f, Vector2D(50.0f, 50.0f));
    ShapePickable p(&circle, Dia::Core::StringCRC("shape"), 1, 0);
    PickHit2D hit;
    EXPECT_TRUE(p.TryPick(Vector2D(50.0f, 50.0f), hit));
    EXPECT_EQ(hit.kind, PickHit2D::Kind::kObject);
    EXPECT_EQ(hit.objectIdx, 1u);
}

TEST(ShapePickable_Circle, TryPick_JustInsideEdge_Hits)
{
    Circle circle(10.0f, Vector2D(0.0f, 0.0f));
    ShapePickable p(&circle, Dia::Core::StringCRC("shape"), 0, 0);
    PickHit2D hit;
    // Point at radius - epsilon
    EXPECT_TRUE(p.TryPick(Vector2D(9.0f, 0.0f), hit));
}

TEST(ShapePickable_Circle, TryPick_OutsideRadius_Misses)
{
    Circle circle(10.0f, Vector2D(0.0f, 0.0f));
    ShapePickable p(&circle, Dia::Core::StringCRC("shape"), 0, 0);
    PickHit2D hit;
    EXPECT_FALSE(p.TryPick(Vector2D(20.0f, 0.0f), hit));
}

// ---------------------------------------------------------------------------
// AARect
// ---------------------------------------------------------------------------

TEST(ShapePickable_AARect, TryPick_Inside_Hits)
{
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
    ShapePickable p(&rect, Dia::Core::StringCRC("rect"), 2, 0);
    PickHit2D hit;
    EXPECT_TRUE(p.TryPick(Vector2D(50.0f, 50.0f), hit));
    EXPECT_EQ(hit.kind, PickHit2D::Kind::kObject);
}

TEST(ShapePickable_AARect, TryPick_Outside_Misses)
{
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    ShapePickable p(&rect, Dia::Core::StringCRC("rect"), 0, 0);
    PickHit2D hit;
    EXPECT_FALSE(p.TryPick(Vector2D(50.0f, 50.0f), hit));
}

// ---------------------------------------------------------------------------
// ConvexPolygon
// ---------------------------------------------------------------------------

TEST(ShapePickable_ConvexPolygon, TryPick_InsideTriangle_Hits)
{
    // CCW triangle at (0,0), (10,0), (5,10)
    Vector2D verts[3] = {
        Vector2D(0.0f, 0.0f),
        Vector2D(10.0f, 0.0f),
        Vector2D(5.0f, 10.0f)
    };
    ConvexPolygon poly(verts, 3);
    ShapePickable p(&poly, Dia::Core::StringCRC("poly"), 0, 0);
    PickHit2D hit;
    EXPECT_TRUE(p.TryPick(Vector2D(5.0f, 3.0f), hit));
}

TEST(ShapePickable_ConvexPolygon, TryPick_Outside_Misses)
{
    Vector2D verts[3] = {
        Vector2D(0.0f, 0.0f),
        Vector2D(10.0f, 0.0f),
        Vector2D(5.0f, 10.0f)
    };
    ConvexPolygon poly(verts, 3);
    ShapePickable p(&poly, Dia::Core::StringCRC("poly"), 0, 0);
    PickHit2D hit;
    EXPECT_FALSE(p.TryPick(Vector2D(50.0f, 50.0f), hit));
}

// ---------------------------------------------------------------------------
// PickArea
// ---------------------------------------------------------------------------

TEST(ShapePickable_PickArea, CircleOverlap_ReturnsHit)
{
    Circle circle(10.0f, Vector2D(50.0f, 50.0f));
    ShapePickable p(&circle, Dia::Core::StringCRC("shape"), 0, 0);
    AARect rect(Vector2D(40.0f, 40.0f), Vector2D(60.0f, 60.0f));
    Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits> hits;
    p.PickArea(rect, hits);
    EXPECT_EQ(hits.Size(), 1u);
}

TEST(ShapePickable_PickArea, CircleNoOverlap_ReturnsNoHit)
{
    Circle circle(5.0f, Vector2D(0.0f, 0.0f));
    ShapePickable p(&circle, Dia::Core::StringCRC("shape"), 0, 0);
    AARect rect(Vector2D(100.0f, 100.0f), Vector2D(200.0f, 200.0f));
    Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits> hits;
    p.PickArea(rect, hits);
    EXPECT_EQ(hits.Size(), 0u);
}
