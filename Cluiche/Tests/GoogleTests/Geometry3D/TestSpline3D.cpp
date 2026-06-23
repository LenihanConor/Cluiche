#include <gtest/gtest.h>
#include <cmath>
#include <DiaGeometry3D/Shapes/Spline3D.h>
#include <DiaMaths/Vector/Vector3D.h>

using namespace Dia::Geometry3D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float Length3(const Vector3D& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// ---------------------------------------------------------------------------
// BSpline — construction
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_Construction_StoresControlPoints)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    EXPECT_EQ(s.GetControlPointCount(), 4);
    EXPECT_EQ(s.GetCurveType(), Spline3D::CurveType::BSpline);
    EXPECT_FLOAT_EQ(s.GetControlPoint(0).x, 0.0f);
    EXPECT_FLOAT_EQ(s.GetControlPoint(3).x, 3.0f);
}

// ---------------------------------------------------------------------------
// CatmullRom — construction
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, CatmullRom_Construction_StoresControlPoints)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 0.0f),
        Vector3D(2.0f, 1.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    EXPECT_EQ(s.GetControlPointCount(), 4);
    EXPECT_EQ(s.GetCurveType(), Spline3D::CurveType::CatmullRom);
}

// ---------------------------------------------------------------------------
// Evaluate — BSpline endpoints
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_Evaluate_T0_ReturnsFinitePoint)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    const Vector3D p = s.Evaluate(0.0f);
    // B-Spline doesn't pass through first control point — just check finite
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
    EXPECT_FALSE(std::isnan(p.z));
}

TEST(DiaGeometry3D_Spline, BSpline_Evaluate_T1_ReturnsFinitePoint)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    const Vector3D p = s.Evaluate(1.0f);
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
    EXPECT_FALSE(std::isnan(p.z));
}

TEST(DiaGeometry3D_Spline, BSpline_Evaluate_T0_5_ReturnsFinitePoint)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 2.0f, 1.0f),
        Vector3D(2.0f, 2.0f, -1.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    const Vector3D p = s.Evaluate(0.5f);
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
    EXPECT_FALSE(std::isnan(p.z));
}

// ---------------------------------------------------------------------------
// Evaluate — CatmullRom interpolates through endpoints
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, CatmullRom_Evaluate_T0_PassesThroughFirstPoint)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    const Vector3D p = s.Evaluate(0.0f);
    EXPECT_NEAR(p.x, 0.0f, 1e-4f);
    EXPECT_NEAR(p.y, 0.0f, 1e-4f);
    EXPECT_NEAR(p.z, 0.0f, 1e-4f);
}

TEST(DiaGeometry3D_Spline, CatmullRom_Evaluate_T1_PassesThroughLastPoint)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    const Vector3D p = s.Evaluate(1.0f);
    EXPECT_NEAR(p.x, 3.0f, 1e-4f);
    EXPECT_NEAR(p.y, 0.0f, 1e-4f);
    EXPECT_NEAR(p.z, 0.0f, 1e-4f);
}

TEST(DiaGeometry3D_Spline, CatmullRom_Evaluate_PassesThroughInteriorPoint)
{
    // With 4 control points there are 3 segments.
    // t = 1/3 maps to seg=1, u=0 → CR formula gives pts[1].
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 5.0f, 2.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 5.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    const Vector3D p = s.Evaluate(1.0f / 3.0f);
    EXPECT_NEAR(p.x, 1.0f, 1e-3f);
    EXPECT_NEAR(p.y, 5.0f, 1e-3f);
    EXPECT_NEAR(p.z, 2.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// EvaluateTangent — normalised output
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_EvaluateTangent_IsNormalised)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    EXPECT_NEAR(Length3(s.EvaluateTangent(0.0f)), 1.0f, 1e-4f);
    EXPECT_NEAR(Length3(s.EvaluateTangent(0.5f)), 1.0f, 1e-4f);
    EXPECT_NEAR(Length3(s.EvaluateTangent(1.0f)), 1.0f, 1e-4f);
}

TEST(DiaGeometry3D_Spline, CatmullRom_EvaluateTangent_IsNormalised)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 0.5f),
        Vector3D(2.0f, 1.0f, -0.5f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    EXPECT_NEAR(Length3(s.EvaluateTangent(0.0f)), 1.0f, 1e-4f);
    EXPECT_NEAR(Length3(s.EvaluateTangent(0.5f)), 1.0f, 1e-4f);
    EXPECT_NEAR(Length3(s.EvaluateTangent(1.0f)), 1.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// EvaluateTangent — direction on axis-aligned lines
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_EvaluateTangent_HorizontalLine_PointsAlongX)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    const Vector3D tan = s.EvaluateTangent(0.5f);
    EXPECT_GT(tan.x, 0.0f);
    EXPECT_NEAR(tan.y, 0.0f, 1e-4f);
    EXPECT_NEAR(tan.z, 0.0f, 1e-4f);
}

TEST(DiaGeometry3D_Spline, CatmullRom_EvaluateTangent_HorizontalLine_PointsAlongX)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    const Vector3D tan = s.EvaluateTangent(0.5f);
    EXPECT_GT(tan.x, 0.0f);
    EXPECT_NEAR(tan.y, 0.0f, 1e-4f);
    EXPECT_NEAR(tan.z, 0.0f, 1e-4f);
}

TEST(DiaGeometry3D_Spline, BSpline_EvaluateTangent_LineAlongZ_PointsAlongZ)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.0f, 0.0f, 1.0f),
        Vector3D(0.0f, 0.0f, 2.0f), Vector3D(0.0f, 0.0f, 3.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    const Vector3D tan = s.EvaluateTangent(0.5f);
    EXPECT_GT(tan.z, 0.0f);
    EXPECT_NEAR(tan.x, 0.0f, 1e-4f);
    EXPECT_NEAR(tan.y, 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// t out of range — clamps to [0,1], does not crash
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_Evaluate_NegativeT_ClampsToT0)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    const Vector3D clamped = s.Evaluate(-0.5f);
    const Vector3D atZero  = s.Evaluate(0.0f);
    EXPECT_NEAR(clamped.x, atZero.x, 1e-4f);
    EXPECT_NEAR(clamped.y, atZero.y, 1e-4f);
    EXPECT_NEAR(clamped.z, atZero.z, 1e-4f);
}

TEST(DiaGeometry3D_Spline, BSpline_Evaluate_OverOneT_ClampsToT1)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    const Vector3D clamped = s.Evaluate(1.5f);
    const Vector3D atOne   = s.Evaluate(1.0f);
    EXPECT_NEAR(clamped.x, atOne.x, 1e-4f);
    EXPECT_NEAR(clamped.y, atOne.y, 1e-4f);
    EXPECT_NEAR(clamped.z, atOne.z, 1e-4f);
}

TEST(DiaGeometry3D_Spline, CatmullRom_Evaluate_NegativeT_ClampsToT0)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    const Vector3D clamped = s.Evaluate(-0.5f);
    const Vector3D atZero  = s.Evaluate(0.0f);
    EXPECT_NEAR(clamped.x, atZero.x, 1e-4f);
    EXPECT_NEAR(clamped.y, atZero.y, 1e-4f);
    EXPECT_NEAR(clamped.z, atZero.z, 1e-4f);
}

TEST(DiaGeometry3D_Spline, CatmullRom_Evaluate_OverOneT_ClampsToT1)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    const Vector3D clamped = s.Evaluate(1.5f);
    const Vector3D atOne   = s.Evaluate(1.0f);
    EXPECT_NEAR(clamped.x, atOne.x, 1e-4f);
    EXPECT_NEAR(clamped.y, atOne.y, 1e-4f);
    EXPECT_NEAR(clamped.z, atOne.z, 1e-4f);
}

// ---------------------------------------------------------------------------
// Multiple control points
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_SixControlPoints_EvaluatesAcrossAllSegments)
{
    Vector3D pts[6] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 2.0f, 1.0f), Vector3D(2.0f, 1.0f, -1.0f),
        Vector3D(3.0f, 3.0f, 0.0f), Vector3D(4.0f, 0.0f, 1.0f), Vector3D(5.0f, 1.0f, -1.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 6);
    for (int i = 0; i <= 10; ++i)
    {
        const float t = static_cast<float>(i) / 10.0f;
        const Vector3D p = s.Evaluate(t);
        EXPECT_FALSE(std::isnan(p.x)) << "nan at t=" << t;
        EXPECT_FALSE(std::isnan(p.y)) << "nan at t=" << t;
        EXPECT_FALSE(std::isnan(p.z)) << "nan at t=" << t;
    }
}

TEST(DiaGeometry3D_Spline, CatmullRom_SixControlPoints_EvaluatesAcrossAllSegments)
{
    Vector3D pts[6] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 2.0f, 1.0f), Vector3D(2.0f, 1.0f, -1.0f),
        Vector3D(3.0f, 3.0f, 0.0f), Vector3D(4.0f, 0.0f, 1.0f), Vector3D(5.0f, 1.0f, -1.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 6);
    for (int i = 0; i <= 10; ++i)
    {
        const float t = static_cast<float>(i) / 10.0f;
        const Vector3D p = s.Evaluate(t);
        EXPECT_FALSE(std::isnan(p.x)) << "nan at t=" << t;
        EXPECT_FALSE(std::isnan(p.y)) << "nan at t=" << t;
        EXPECT_FALSE(std::isnan(p.z)) << "nan at t=" << t;
    }
}

// ---------------------------------------------------------------------------
// Max control points — exactly 16 works
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_MaxControlPoints_16_Succeeds)
{
    Vector3D pts[Spline3D::kMaxControlPoints];
    for (int i = 0; i < Spline3D::kMaxControlPoints; ++i)
        pts[i] = Vector3D(static_cast<float>(i), 0.0f, 0.0f);
    Spline3D s = SplineFactory3D::MakeBSpline(pts, Spline3D::kMaxControlPoints);
    EXPECT_EQ(s.GetControlPointCount(), Spline3D::kMaxControlPoints);
    const Vector3D p = s.Evaluate(0.5f);
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
    EXPECT_FALSE(std::isnan(p.z));
}

TEST(DiaGeometry3D_Spline, CatmullRom_MaxControlPoints_16_Succeeds)
{
    Vector3D pts[Spline3D::kMaxControlPoints];
    for (int i = 0; i < Spline3D::kMaxControlPoints; ++i)
        pts[i] = Vector3D(static_cast<float>(i), 0.0f, 0.0f);
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, Spline3D::kMaxControlPoints);
    EXPECT_EQ(s.GetControlPointCount(), Spline3D::kMaxControlPoints);
    const Vector3D p = s.Evaluate(0.5f);
    EXPECT_FALSE(std::isnan(p.x));
    EXPECT_FALSE(std::isnan(p.y));
    EXPECT_FALSE(std::isnan(p.z));
}

// ---------------------------------------------------------------------------
// BSpline — monotonicity on a straight line
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_Evaluate_Monotone_XIncreasesWithT)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 0.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    EXPECT_GT(s.Evaluate(0.75f).x, s.Evaluate(0.25f).x);
}

// ---------------------------------------------------------------------------
// GetControlPoint — valid index round-trip
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, GetControlPoint_ValidIndex_ReturnsStoredValue)
{
    Vector3D pts[4] = {
        Vector3D(10.0f, 20.0f, 30.0f), Vector3D(40.0f, 50.0f, 60.0f),
        Vector3D(70.0f, 80.0f, 90.0f), Vector3D(100.0f, 110.0f, 120.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_FLOAT_EQ(s.GetControlPoint(i).x, pts[i].x);
        EXPECT_FLOAT_EQ(s.GetControlPoint(i).y, pts[i].y);
        EXPECT_FLOAT_EQ(s.GetControlPoint(i).z, pts[i].z);
    }
}

// ---------------------------------------------------------------------------
// BSpline vs CatmullRom — diverge on non-linear arrangement
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_vs_CatmullRom_DifferOnCurvedPath)
{
    // Arc-like arrangement — the two spline types should produce different midpoints
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.0f, 5.0f, 0.0f),
        Vector3D(5.0f, 5.0f, 0.0f), Vector3D(5.0f, 0.0f, 0.0f)
    };
    Spline3D bs = SplineFactory3D::MakeBSpline(pts, 4);
    Spline3D cr = SplineFactory3D::MakeCatmullRom(pts, 4);
    const Vector3D bsMid = bs.Evaluate(0.5f);
    const Vector3D crMid = cr.Evaluate(0.5f);
    // They must differ — if they're identical the curve type is being ignored
    const float diff = Length3(Vector3D(bsMid.x - crMid.x, bsMid.y - crMid.y, bsMid.z - crMid.z));
    EXPECT_GT(diff, 0.01f);
}

// ---------------------------------------------------------------------------
// EvaluateTangent — normalised across multiple t values on 3D helix-like path
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_EvaluateTangent_Normalised_3DPath)
{
    // Points with non-trivial Z to exercise 3D tangent normalisation
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 1.0f),
        Vector3D(2.0f, 0.0f, 2.0f), Vector3D(3.0f, 1.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    for (int i = 0; i <= 10; ++i)
    {
        const float t = static_cast<float>(i) / 10.0f;
        EXPECT_NEAR(Length3(s.EvaluateTangent(t)), 1.0f, 1e-4f) << "at t=" << t;
    }
}

TEST(DiaGeometry3D_Spline, CatmullRom_EvaluateTangent_Normalised_3DPath)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 1.0f),
        Vector3D(2.0f, 0.0f, 2.0f), Vector3D(3.0f, 1.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    for (int i = 0; i <= 10; ++i)
    {
        const float t = static_cast<float>(i) / 10.0f;
        EXPECT_NEAR(Length3(s.EvaluateTangent(t)), 1.0f, 1e-4f) << "at t=" << t;
    }
}

// ---------------------------------------------------------------------------
// Minimum control point count — exactly 4 works
// ---------------------------------------------------------------------------

TEST(DiaGeometry3D_Spline, BSpline_MinControlPoints_4_Succeeds)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 1.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeBSpline(pts, 4);
    EXPECT_EQ(s.GetControlPointCount(), 4);
    const Vector3D p = s.Evaluate(0.5f);
    EXPECT_FALSE(std::isnan(p.x));
}

TEST(DiaGeometry3D_Spline, CatmullRom_MinControlPoints_4_Succeeds)
{
    Vector3D pts[4] = {
        Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 0.0f),
        Vector3D(2.0f, 0.0f, 0.0f), Vector3D(3.0f, 1.0f, 0.0f)
    };
    Spline3D s = SplineFactory3D::MakeCatmullRom(pts, 4);
    EXPECT_EQ(s.GetControlPointCount(), 4);
    const Vector3D p = s.Evaluate(0.5f);
    EXPECT_FALSE(std::isnan(p.x));
}
