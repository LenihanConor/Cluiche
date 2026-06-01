#include <gtest/gtest.h>
#include <cmath>
#include <DiaGeometry2D/Shapes/Spline.h>
#include <DiaGeometry2D/Shapes/SplineFactory.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float Length(const Vector2D& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

static Vector2D MakeControlPoints(float* xs, float* ys, Vector2D* out, int count)
{
    for (int i = 0; i < count; ++i)
        out[i] = Vector2D(xs[i], ys[i]);
    return out[0];
}

// ---------------------------------------------------------------------------
// BSpline — construction
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, BSpline_Construction_StoresControlPoints)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    EXPECT_EQ(s.GetControlPointCount(), 4);
    EXPECT_EQ(s.GetCurveType(), Spline::CurveType::BSpline);
    EXPECT_FLOAT_EQ(s.GetControlPoint(0).x, 0.0f);
    EXPECT_FLOAT_EQ(s.GetControlPoint(3).x, 3.0f);
}

// ---------------------------------------------------------------------------
// CatmullRom — construction
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, CatmullRom_Construction_StoresControlPoints)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f),
        Vector2D(2.0f, 1.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeCatmullRom(pts, 4);
    EXPECT_EQ(s.GetControlPointCount(), 4);
    EXPECT_EQ(s.GetCurveType(), Spline::CurveType::CatmullRom);
}

// ---------------------------------------------------------------------------
// Evaluate — BSpline endpoints
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, BSpline_Evaluate_T0_ReturnsNearFirstSegmentStart)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    const Vector2D p = s.Evaluate(0.0f);
    // B-Spline doesn't pass through first control point — just check it's finite
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
}

TEST(Geometry2D_Spline, BSpline_Evaluate_T1_ReturnsFinitePoint)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    const Vector2D p = s.Evaluate(1.0f);
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
}

TEST(Geometry2D_Spline, BSpline_Evaluate_T0_5_ReturnsFinitePoint)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 2.0f),
        Vector2D(2.0f, 2.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    const Vector2D p = s.Evaluate(0.5f);
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
}

// ---------------------------------------------------------------------------
// Evaluate — CatmullRom interpolates through internal points
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, CatmullRom_Evaluate_T0_PassesThroughFirstPoint)
{
    // At t=0, seg=0, u=0: CR formula gives p[i1]=mControlPoints[0]=p0.
    // CatmullRom passes through every control point; t=0 is p0.
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeCatmullRom(pts, 4);
    const Vector2D p = s.Evaluate(0.0f);
    EXPECT_NEAR(p.x, 0.0f, 1e-4f);
    EXPECT_NEAR(p.y, 0.0f, 1e-4f);
}

TEST(Geometry2D_Spline, CatmullRom_Evaluate_T1_PassesThroughLastPoint)
{
    // At t=1, last seg, u=1: CR formula gives p[i2]=last control point p3.
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeCatmullRom(pts, 4);
    const Vector2D p = s.Evaluate(1.0f);
    EXPECT_NEAR(p.x, 3.0f, 1e-4f);
    EXPECT_NEAR(p.y, 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// EvaluateTangent — normalised output
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, BSpline_EvaluateTangent_IsNormalised)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    const Vector2D t0 = s.EvaluateTangent(0.0f);
    const Vector2D t5 = s.EvaluateTangent(0.5f);
    const Vector2D t1 = s.EvaluateTangent(1.0f);
    EXPECT_NEAR(Length(t0), 1.0f, 1e-4f);
    EXPECT_NEAR(Length(t5), 1.0f, 1e-4f);
    EXPECT_NEAR(Length(t1), 1.0f, 1e-4f);
}

TEST(Geometry2D_Spline, CatmullRom_EvaluateTangent_IsNormalised)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f),
        Vector2D(2.0f, 1.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeCatmullRom(pts, 4);
    const Vector2D t0 = s.EvaluateTangent(0.0f);
    const Vector2D t5 = s.EvaluateTangent(0.5f);
    const Vector2D t1 = s.EvaluateTangent(1.0f);
    EXPECT_NEAR(Length(t0), 1.0f, 1e-4f);
    EXPECT_NEAR(Length(t5), 1.0f, 1e-4f);
    EXPECT_NEAR(Length(t1), 1.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// EvaluateTangent — direction
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, BSpline_EvaluateTangent_HorizontalLine_PointsRight)
{
    // All points on y=0, x increasing — tangent should point right
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    const Vector2D tan = s.EvaluateTangent(0.5f);
    EXPECT_GT(tan.x, 0.0f);
    EXPECT_NEAR(tan.y, 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Multiple control points
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, BSpline_SixControlPoints_EvaluatesAcrossAllSegments)
{
    Vector2D pts[6] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 2.0f), Vector2D(2.0f, 1.0f),
        Vector2D(3.0f, 3.0f), Vector2D(4.0f, 0.0f), Vector2D(5.0f, 1.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 6);
    for (int i = 0; i <= 10; ++i)
    {
        const float t = static_cast<float>(i) / 10.0f;
        const Vector2D p = s.Evaluate(t);
        EXPECT_FALSE(std::isnan(p.x)) << "nan at t=" << t;
        EXPECT_FALSE(std::isnan(p.y)) << "nan at t=" << t;
    }
}

// ---------------------------------------------------------------------------
// Max control points — exactly 16 works
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, BSpline_MaxControlPoints_16_Succeeds)
{
    Vector2D pts[Spline::kMaxControlPoints];
    for (int i = 0; i < Spline::kMaxControlPoints; ++i)
        pts[i] = Vector2D(static_cast<float>(i), 0.0f);
    Spline s = SplineFactory::MakeBSpline(pts, Spline::kMaxControlPoints);
    EXPECT_EQ(s.GetControlPointCount(), Spline::kMaxControlPoints);
    const Vector2D p = s.Evaluate(0.5f);
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
}

TEST(Geometry2D_Spline, CatmullRom_MaxControlPoints_16_Succeeds)
{
    Vector2D pts[Spline::kMaxControlPoints];
    for (int i = 0; i < Spline::kMaxControlPoints; ++i)
        pts[i] = Vector2D(static_cast<float>(i), 0.0f);
    Spline s = SplineFactory::MakeCatmullRom(pts, Spline::kMaxControlPoints);
    EXPECT_EQ(s.GetControlPointCount(), Spline::kMaxControlPoints);
    const Vector2D p = s.Evaluate(0.5f);
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
}

// ---------------------------------------------------------------------------
// t out of range — clamps to [0,1], does not crash
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, BSpline_Evaluate_NegativeT_ClampsToT0)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    const Vector2D clamped = s.Evaluate(-0.5f);
    const Vector2D atZero  = s.Evaluate(0.0f);
    EXPECT_NEAR(clamped.x, atZero.x, 1e-4f);
    EXPECT_NEAR(clamped.y, atZero.y, 1e-4f);
}

TEST(Geometry2D_Spline, BSpline_Evaluate_OverOneT_ClampsToT1)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    const Vector2D clamped = s.Evaluate(1.5f);
    const Vector2D atOne   = s.Evaluate(1.0f);
    EXPECT_NEAR(clamped.x, atOne.x, 1e-4f);
    EXPECT_NEAR(clamped.y, atOne.y, 1e-4f);
}

// ---------------------------------------------------------------------------
// CatmullRom — tangent direction on horizontal line
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, CatmullRom_EvaluateTangent_HorizontalLine_PointsRight)
{
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeCatmullRom(pts, 4);
    const Vector2D tan = s.EvaluateTangent(0.5f);
    EXPECT_GT(tan.x, 0.0f);
    EXPECT_NEAR(tan.y, 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// CatmullRom — passes through interior control point at its knot
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, CatmullRom_Evaluate_PassesThroughInteriorPoint)
{
    // With 4 control points there are 3 segments (indices 0,1,2).
    // The knot at t = 1/3 should land exactly on pts[1] = (1, 5).
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 5.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 5.0f)
    };
    Spline s = SplineFactory::MakeCatmullRom(pts, 4);
    // t = 1/3 maps to seg=1, u=0 → CR formula at u=0 gives p[i1] = pts[1]
    const Vector2D p = s.Evaluate(1.0f / 3.0f);
    EXPECT_NEAR(p.x, 1.0f, 1e-3f);
    EXPECT_NEAR(p.y, 5.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// BSpline — monotonicity on a straight horizontal line
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, BSpline_Evaluate_Monotone_XIncreasesWithT)
{
    // Points on y=0 with increasing x — x at t=0.75 must exceed x at t=0.25
    Vector2D pts[4] = {
        Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f),
        Vector2D(2.0f, 0.0f), Vector2D(3.0f, 0.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    EXPECT_GT(s.Evaluate(0.75f).x, s.Evaluate(0.25f).x);
}

// ---------------------------------------------------------------------------
// GetControlPoint — valid index round-trip
// ---------------------------------------------------------------------------

TEST(Geometry2D_Spline, GetControlPoint_ValidIndex_ReturnsStoredValue)
{
    Vector2D pts[4] = {
        Vector2D(10.0f, 20.0f), Vector2D(30.0f, 40.0f),
        Vector2D(50.0f, 60.0f), Vector2D(70.0f, 80.0f)
    };
    Spline s = SplineFactory::MakeBSpline(pts, 4);
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_FLOAT_EQ(s.GetControlPoint(i).x, pts[i].x);
        EXPECT_FLOAT_EQ(s.GetControlPoint(i).y, pts[i].y);
    }
}
