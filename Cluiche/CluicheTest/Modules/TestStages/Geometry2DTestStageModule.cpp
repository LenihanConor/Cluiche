#include "Modules/TestStages/Geometry2DTestStageModule.h"

#ifdef DIA_DEBUG
#include "Modules/TestStages/Drawers/Geometry2DShapesDrawer.h"
#include "Modules/TestStages/Drawers/Geometry2DLabelsDrawer.h"
#include "Modules/TestStages/Drawers/Geometry2DAABBDrawer.h"
#include "Modules/VisualDebuggerModule.h"
#include <DiaGeometry2D/Shapes/Ray.h>
#endif

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace CluicheTest {

const Dia::Core::StringCRC Geometry2DTestStageModule::kTypeId("Geometry2DTestStageModule");

Geometry2DTestStageModule::Geometry2DTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Dia::Core::StringCRC Geometry2DTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("Geometry2DTestStage");
}

const Dia::Core::StringCRC* Geometry2DTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.geometry2d.passed")
    };
    outCount = 1;
    return names;
}

void Geometry2DTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    SetupGallery();

#ifdef DIA_DEBUG
    SetupIntersectionPairs();
#endif

    // Register metrics as Gauges so orchestrator can read via dia.automation.get_metric
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricShapeCount)
        mMetricShapeCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.geometry2d.shape_count"));
    if (!mMetricIntersectionPairCount)
        mMetricIntersectionPairCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.geometry2d.intersection_pair_count"));

    if (mMetricShapeCount)
        mMetricShapeCount->Set(static_cast<double>(mShapeCount));

#ifdef DIA_DEBUG
    if (mMetricIntersectionPairCount)
        mMetricIntersectionPairCount->Set(static_cast<double>(mIntersectionPairCount));
#endif

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.geometry2d.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool passed = IsResolved();
            return { passed, passed ? "all shapes set up" : "pending", 0.0f };
        });

    // REPL commands — allow the orchestrator to query metrics
    {
        Dia::API::CommandInfoJson cmd;
        cmd.name        = Dia::Core::StringCRC("dia.geometry2d.shape_count");
        cmd.description = "Return the number of geometry shapes in the gallery";
        cmd.category    = Dia::Core::StringCRC("dia.geometry2d");
        cmd.owner       = "CluicheTest";
        cmd.callback    = [this](const Json::Value&) -> Json::Value {
            Json::Value data;
            data["value"] = mShapeCount;
            return data;
        };
        Dia::API::RegisterCommandJson(cmd);
    }
    {
        Dia::API::CommandInfoJson cmd;
        cmd.name        = Dia::Core::StringCRC("dia.geometry2d.intersection_pairs");
        cmd.description = "Return the number of intersection pairs evaluated";
        cmd.category    = Dia::Core::StringCRC("dia.geometry2d");
        cmd.owner       = "CluicheTest";
        cmd.callback    = [this](const Json::Value&) -> Json::Value {
            Json::Value data;
#ifdef DIA_DEBUG
            data["value"] = mIntersectionPairCount;
#else
            data["value"] = 0;
#endif
            return data;
        };
        Dia::API::RegisterCommandJson(cmd);
    }
}

void Geometry2DTestStageModule::OnUpdate(float /*deltaTime*/)
{
#ifdef DIA_DEBUG
    if (!mShapesDrawer)
    {
        if (auto* mgr = Cluiche::AppFlow::VisualDebuggerModule::GetStaticLayerManager())
        {
            const Dia::Core::StringCRC stageTag(GetStageName());

            mShapesDrawer = std::make_unique<Geometry2DShapesDrawer>(
                mCircle, mAARect, mOORect, mLine, mRay, mTriangle,
                mCapsule, mConvexPoly, mArc, mSector, *mgr);
            mgr->Register(mShapesDrawer.get(), 20, stageTag);

            mLabelsDrawer = std::make_unique<Geometry2DLabelsDrawer>(
                mCircle, mAARect, mOORect, mLine, mRay, mTriangle,
                mCapsule, mConvexPoly, mArc, mSector);
            mgr->Register(mLabelsDrawer.get(), 21, stageTag);

            mIntersectionsDrawer = std::make_unique<Geometry2DIntersectionsDrawer>(mIntersectionPairs, *mgr);
            mgr->Register(mIntersectionsDrawer.get(), 22, stageTag);

            mAABBDrawer = std::make_unique<Geometry2DAABBDrawer>(
                mCircle, mAARect, mOORect, mLine, mRay, mTriangle,
                mCapsule, mConvexPoly, mArc, mSector, *mgr);
            mgr->Register(mAABBDrawer.get(), 23, stageTag);
        }
    }
#endif

    if (!IsResolved())
        ReportPassed();
}

void Geometry2DTestStageModule::OnStop()
{
    mShapeCount = 0;
    mMetricShapeCount = nullptr;
    mMetricIntersectionPairCount = nullptr;

#ifdef DIA_DEBUG
    mIntersectionPairs.RemoveAll();
    mIntersectionPairCount = 0;

    mShapesDrawer.reset();
    mLabelsDrawer.reset();
    mIntersectionsDrawer.reset();
    mAABBDrawer.reset();
#endif
}

void Geometry2DTestStageModule::SetupGallery()
{
    // 10 primitives placed in a horizontal band at y~400, x from 150 to 1230 (120px spacing)

    mCircle = Dia::Geometry2D::Circle(40.0f, Dia::Maths::Vector2D(150.0f, 400.0f));

    mAARect = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(240.0f, 360.0f),
        Dia::Maths::Vector2D(320.0f, 440.0f));

    // OORect: 80x40 box centred at (380, 400), rotated 30°
    // cos30 = 0.866, sin30 = 0.5; half-extents hw=40, hh=20
    // local axis X = (cos30, sin30) = (0.866, 0.5)
    // local axis Y = (-sin30, cos30) = (-0.5, 0.866)
    // corners: centre ± hw*axisX ± hh*axisY
    {
        const float cx = 380.0f, cy = 400.0f;
        const float hw = 40.0f, hh = 20.0f;
        const float cosA = 0.866f, sinA = 0.5f;
        // axisX = (cosA, sinA), axisY = (-sinA, cosA)
        Dia::Maths::Vector2D pt0(cx + hw * cosA - hh * sinA, cy + hw * sinA + hh * cosA);
        Dia::Maths::Vector2D pt1(cx - hw * cosA - hh * sinA, cy - hw * sinA + hh * cosA);
        Dia::Maths::Vector2D pt2(cx - hw * cosA + hh * sinA, cy - hw * sinA - hh * cosA);
        Dia::Maths::Vector2D pt3(cx + hw * cosA + hh * sinA, cy + hw * sinA - hh * cosA);
        mOORect = Dia::Geometry2D::OORect(pt0, pt1, pt2, pt3);
    }

    mLine = Dia::Geometry2D::Line(
        Dia::Maths::Vector2D(460.0f, 370.0f),
        Dia::Maths::Vector2D(540.0f, 430.0f));

    mRay = Dia::Geometry2D::Ray(
        Dia::Maths::Vector2D(580.0f, 400.0f),
        Dia::Maths::Vector2D(1.0f, 0.5f));

    mTriangle = Dia::Geometry2D::Triangle(
        Dia::Maths::Vector2D(680.0f, 360.0f),
        Dia::Maths::Vector2D(730.0f, 440.0f),
        Dia::Maths::Vector2D(780.0f, 360.0f));

    mCapsule = Dia::Geometry2D::Capsule(
        25.0f,
        Dia::Maths::Vector2D(830.0f, 375.0f),
        Dia::Maths::Vector2D(870.0f, 425.0f));

    // ConvexPolygon: regular pentagon centred at (950, 400), radius 40
    // Vertices at angles 90°, 162°, 234°, 306°, 18° (CCW winding)
    {
        const float cx = 950.0f, cy = 400.0f, r = 40.0f;
        Dia::Maths::Vector2D verts[5];
        verts[0] = Dia::Maths::Vector2D(cx + r *  0.000f,  cy + r *  1.000f);   // 90°
        verts[1] = Dia::Maths::Vector2D(cx + r * -0.951f,  cy + r *  0.309f);   // 162°
        verts[2] = Dia::Maths::Vector2D(cx + r * -0.588f,  cy + r * -0.809f);   // 234°
        verts[3] = Dia::Maths::Vector2D(cx + r *  0.588f,  cy + r * -0.809f);   // 306°
        verts[4] = Dia::Maths::Vector2D(cx + r *  0.951f,  cy + r *  0.309f);   // 18°
        mConvexPoly = Dia::Geometry2D::ConvexPolygon(verts, 5);
    }

    mArc = Dia::Geometry2D::Arc(
        40.0f,
        Dia::Maths::Angle::FromDegrees(120.0f),
        Dia::Maths::Vector2D(1070.0f, 400.0f),
        Dia::Maths::Vector2D(0.0f, 1.0f));

    mSector = Dia::Geometry2D::Sector(
        Dia::Maths::Vector2D(1190.0f, 400.0f),
        40.0f,
        Dia::Maths::Vector2D(0.0f, 1.0f),
        Dia::Maths::Angle::FromDegrees(60.0f));

    mShapeCount = 10;
}

#ifdef DIA_DEBUG
void Geometry2DTestStageModule::SetupIntersectionPairs()
{
    // 6 pairs laid out in a horizontal band at y~200, spaced ~200px from x=100

    // --- Pair 0: Circle/Circle — HIT (overlapping circles) ---
    {
        IntersectionPair pair;
        pair.kind    = IntersectionPair::Kind::CircleCircle;
        pair.circleA = Dia::Geometry2D::Circle(30.0f, Dia::Maths::Vector2D(100.0f, 200.0f));
        pair.circleB = Dia::Geometry2D::Circle(30.0f, Dia::Maths::Vector2D(140.0f, 200.0f));
        // distance between centres = 40, sum of radii = 60 → overlapping
        pair.hit = pair.circleA.IsIntersecting(pair.circleB).IsIntersecting();
        mIntersectionPairs.Add(pair);
    }

    // --- Pair 1: Circle/AARect — MISS (circle well left of rect) ---
    {
        IntersectionPair pair;
        pair.kind    = IntersectionPair::Kind::CircleAARect;
        pair.circleA = Dia::Geometry2D::Circle(20.0f, Dia::Maths::Vector2D(290.0f, 190.0f));
        pair.aaRect  = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(340.0f, 180.0f),
            Dia::Maths::Vector2D(400.0f, 220.0f));
        // circle edge at x=310, rect starts at x=340 → gap of 30 → no overlap
        pair.hit = pair.circleA.IsIntersecting(pair.aaRect).IsIntersecting();
        mIntersectionPairs.Add(pair);
    }

    // --- Pair 2: AARect/Triangle — HIT (triangle fully inside rect) ---
    {
        IntersectionPair pair;
        pair.kind     = IntersectionPair::Kind::AARectTriangle;
        pair.aaRect   = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(460.0f, 170.0f),
            Dia::Maths::Vector2D(560.0f, 230.0f));
        pair.triangle = Dia::Geometry2D::Triangle(
            Dia::Maths::Vector2D(480.0f, 180.0f),
            Dia::Maths::Vector2D(540.0f, 180.0f),
            Dia::Maths::Vector2D(510.0f, 220.0f));
        // All triangle vertices inside rect — test via point containment
        bool anyVertexInside =
            pair.aaRect.IsIntersecting(pair.triangle.GetPt(Dia::Geometry2D::Triangle::kPt0)).IsIntersecting() ||
            pair.aaRect.IsIntersecting(pair.triangle.GetPt(Dia::Geometry2D::Triangle::kPt1)).IsIntersecting() ||
            pair.aaRect.IsIntersecting(pair.triangle.GetPt(Dia::Geometry2D::Triangle::kPt2)).IsIntersecting();
        pair.hit = anyVertexInside;
        mIntersectionPairs.Add(pair);
    }

    // --- Pair 3: Ray/Circle — HIT (ray aimed directly at circle) ---
    {
        IntersectionPair pair;
        pair.kind    = IntersectionPair::Kind::RayCircle;
        pair.ray     = Dia::Geometry2D::Ray(
            Dia::Maths::Vector2D(620.0f, 200.0f),
            Dia::Maths::Vector2D(1.0f, 0.0f));   // pointing +X
        pair.circleA = Dia::Geometry2D::Circle(25.0f, Dia::Maths::Vector2D(720.0f, 200.0f));
        // ray at y=200 pointing right, circle centred at (720,200) radius 25 → direct hit
        Dia::Geometry2D::RaycastHit hitInfo;
        pair.hit = Dia::Geometry2D::Raycast::CastCircle(pair.ray, pair.circleA, hitInfo);
        mIntersectionPairs.Add(pair);
    }

    // --- Pair 4: Line/AARect — MISS (line endpoints both left of rect) ---
    {
        IntersectionPair pair;
        pair.kind   = IntersectionPair::Kind::LineAARect;
        pair.line   = Dia::Geometry2D::Line(
            Dia::Maths::Vector2D(800.0f, 160.0f),
            Dia::Maths::Vector2D(860.0f, 180.0f));
        pair.aaRect = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(890.0f, 185.0f),
            Dia::Maths::Vector2D(950.0f, 215.0f));
        // No direct Line::IsIntersecting(AARect) API — test endpoint containment as proxy
        // Both endpoints are left of rect (x ≤ 860, rect starts at 890) → MISS
        bool ep1Inside = pair.aaRect.IsIntersecting(pair.line.GetPt1()).IsIntersecting();
        bool ep2Inside = pair.aaRect.IsIntersecting(pair.line.GetPt2()).IsIntersecting();
        pair.hit = ep1Inside || ep2Inside;
        mIntersectionPairs.Add(pair);
    }

    // --- Pair 5: Circle/Triangle — MISS (circle far left of triangle) ---
    {
        IntersectionPair pair;
        pair.kind     = IntersectionPair::Kind::CircleTriangle;
        pair.circleA  = Dia::Geometry2D::Circle(20.0f, Dia::Maths::Vector2D(970.0f, 160.0f));
        pair.triangle = Dia::Geometry2D::Triangle(
            Dia::Maths::Vector2D(1060.0f, 180.0f),
            Dia::Maths::Vector2D(1120.0f, 180.0f),
            Dia::Maths::Vector2D(1090.0f, 220.0f));
        // circle edge at x=990, triangle starts at x=1060 → gap of 70 → no overlap
        pair.hit = pair.circleA.IsIntersecting(pair.triangle).IsIntersecting();
        mIntersectionPairs.Add(pair);
    }

    mIntersectionPairCount = static_cast<int>(mIntersectionPairs.Size());

    DIA_LOG_INFO("CluicheTest", "Geometry2DTestStageModule — %d shapes, %d intersection pairs",
        mShapeCount, mIntersectionPairCount);
}
#endif

} // namespace CluicheTest

namespace { using Geometry2DTestStageModule_ = CluicheTest::Geometry2DTestStageModule; }
DIA_MODULE(Geometry2DTestStageModule_);
