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
    SetupSpatialStructures();
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
            const Dia::Core::StringCRC stageTag("Geometry2D");

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

            // Spatial structure drawers
            if (mBVH)
            {
                mBVHDrawer = std::make_unique<Dia::Geometry2DVisualDebugger::BVHDrawer<SpatialElem, kSpatialMax>>(*mBVH, *mgr);
                mgr->Register(mBVHDrawer.get(), 24, stageTag);
            }
            if (mQuadtree)
            {
                mQuadtreeDrawer = std::make_unique<Dia::Geometry2DVisualDebugger::QuadtreeDrawer<SpatialElem, kSpatialMax>>(*mQuadtree, *mgr);
                mgr->Register(mQuadtreeDrawer.get(), 25, stageTag);
            }
            if (mSpatialGrid)
            {
                mSpatialGridDrawer = std::make_unique<Dia::Geometry2DVisualDebugger::SpatialGridDrawer<SpatialElem, kSpatialMax>>(*mSpatialGrid, *mgr);
                mgr->Register(mSpatialGridDrawer.get(), 26, stageTag);
            }
            if (mHexGrid)
            {
                mHexGridDrawer = std::make_unique<Dia::Geometry2DVisualDebugger::HexGridDrawer<SpatialElem, kSpatialMax>>(*mHexGrid, *mgr);
                mgr->Register(mHexGridDrawer.get(), 27, stageTag);
            }
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

    if (mShapesDrawer)
    {
        if (auto* mgr = Cluiche::AppFlow::VisualDebuggerModule::GetStaticLayerManager())
        {
            mgr->Unregister(mShapesDrawer->GetLayerName());
            mgr->Unregister(mLabelsDrawer->GetLayerName());
            mgr->Unregister(mIntersectionsDrawer->GetLayerName());
            mgr->Unregister(mAABBDrawer->GetLayerName());

            if (mBVHDrawer)
                mgr->Unregister(mBVHDrawer->GetLayerName());
            if (mQuadtreeDrawer)
                mgr->Unregister(mQuadtreeDrawer->GetLayerName());
            if (mSpatialGridDrawer)
                mgr->Unregister(mSpatialGridDrawer->GetLayerName());
            if (mHexGridDrawer)
                mgr->Unregister(mHexGridDrawer->GetLayerName());
        }
        mShapesDrawer.reset();
        mLabelsDrawer.reset();
        mIntersectionsDrawer.reset();
        mAABBDrawer.reset();
        mBVHDrawer.reset();
        mQuadtreeDrawer.reset();
        mSpatialGridDrawer.reset();
        mHexGridDrawer.reset();
    }

    mBVH.reset();
    mQuadtree.reset();
    mSpatialGrid.reset();
    mHexGrid.reset();
#endif
}

void Geometry2DTestStageModule::SetupGallery()
{
    // Top band, right of console: 2 rows of 5, x∈[600,1320], row1 y≈820, row2 y≈720
    constexpr float kSpacing = 160.0f;
    constexpr float kRow1Y = 820.0f;
    constexpr float kRow2Y = 700.0f;
    constexpr float kStartX = 620.0f;

    // Row 1: Circle, AARect, OORect, Line, Ray
    mCircle = Dia::Geometry2D::Circle(45.0f, Dia::Maths::Vector2D(kStartX, kRow1Y));

    mAARect = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(kStartX + kSpacing - 40.0f, kRow1Y - 35.0f),
        Dia::Maths::Vector2D(kStartX + kSpacing + 40.0f, kRow1Y + 35.0f));

    {
        const float cx = kStartX + kSpacing * 2, cy = kRow1Y;
        const float hw = 40.0f, hh = 22.0f;
        const float cosA = 0.866f, sinA = 0.5f;
        Dia::Maths::Vector2D pt0(cx + hw * cosA - hh * sinA, cy + hw * sinA + hh * cosA);
        Dia::Maths::Vector2D pt1(cx - hw * cosA - hh * sinA, cy - hw * sinA + hh * cosA);
        Dia::Maths::Vector2D pt2(cx - hw * cosA + hh * sinA, cy - hw * sinA - hh * cosA);
        Dia::Maths::Vector2D pt3(cx + hw * cosA + hh * sinA, cy + hw * sinA - hh * cosA);
        mOORect = Dia::Geometry2D::OORect(pt0, pt1, pt2, pt3);
    }

    // Line: short segment (40px apart so it's visually contained despite infinite rendering)
    mLine = Dia::Geometry2D::Line(
        Dia::Maths::Vector2D(kStartX + kSpacing * 3 - 20.0f, kRow1Y - 20.0f),
        Dia::Maths::Vector2D(kStartX + kSpacing * 3 + 20.0f, kRow1Y + 20.0f));

    mRay = Dia::Geometry2D::Ray(
        Dia::Maths::Vector2D(kStartX + kSpacing * 4, kRow1Y),
        Dia::Maths::Vector2D(1.0f, 0.3f));

    // Row 2: Triangle, Capsule, ConvexPoly, Arc, Sector
    mTriangle = Dia::Geometry2D::Triangle(
        Dia::Maths::Vector2D(kStartX - 35.0f, kRow2Y - 30.0f),
        Dia::Maths::Vector2D(kStartX + 35.0f, kRow2Y - 30.0f),
        Dia::Maths::Vector2D(kStartX, kRow2Y + 40.0f));

    mCapsule = Dia::Geometry2D::Capsule(
        22.0f,
        Dia::Maths::Vector2D(kStartX + kSpacing - 20.0f, kRow2Y - 20.0f),
        Dia::Maths::Vector2D(kStartX + kSpacing + 20.0f, kRow2Y + 20.0f));

    {
        const float cx = kStartX + kSpacing * 2, cy = kRow2Y, r = 38.0f;
        Dia::Maths::Vector2D verts[5];
        verts[0] = Dia::Maths::Vector2D(cx + r *  0.000f, cy + r *  1.000f);
        verts[1] = Dia::Maths::Vector2D(cx + r * -0.951f, cy + r *  0.309f);
        verts[2] = Dia::Maths::Vector2D(cx + r * -0.588f, cy + r * -0.809f);
        verts[3] = Dia::Maths::Vector2D(cx + r *  0.588f, cy + r * -0.809f);
        verts[4] = Dia::Maths::Vector2D(cx + r *  0.951f, cy + r *  0.309f);
        mConvexPoly = Dia::Geometry2D::ConvexPolygon(verts, 5);
    }

    mArc = Dia::Geometry2D::Arc(
        38.0f,
        Dia::Maths::Angle::FromDegrees(120.0f),
        Dia::Maths::Vector2D(kStartX + kSpacing * 3, kRow2Y),
        Dia::Maths::Vector2D(0.0f, 1.0f));

    mSector = Dia::Geometry2D::Sector(
        Dia::Maths::Vector2D(kStartX + kSpacing * 4, kRow2Y),
        38.0f,
        Dia::Maths::Vector2D(0.0f, 1.0f),
        Dia::Maths::Angle::FromDegrees(60.0f));

    mShapeCount = 10;
}

#ifdef DIA_DEBUG
void Geometry2DTestStageModule::SetupIntersectionPairs()
{
    // Middle band: y≈510, x from 620 to 1280 (6 pairs, ~110px apart)
    constexpr float kY = 510.0f;
    constexpr float kStartX = 640.0f;
    constexpr float kSpacing = 120.0f;

    // Pair 0: Circle/Circle — HIT
    {
        IntersectionPair pair;
        pair.kind    = IntersectionPair::Kind::CircleCircle;
        pair.circleA = Dia::Geometry2D::Circle(18.0f, Dia::Maths::Vector2D(kStartX - 12.0f, kY));
        pair.circleB = Dia::Geometry2D::Circle(18.0f, Dia::Maths::Vector2D(kStartX + 12.0f, kY));
        pair.hit = pair.circleA.IsIntersecting(pair.circleB).IsIntersecting();
        mIntersectionPairs.Add(pair);
    }

    // Pair 1: Circle/AARect — MISS
    {
        IntersectionPair pair;
        pair.kind    = IntersectionPair::Kind::CircleAARect;
        const float px = kStartX + kSpacing;
        pair.circleA = Dia::Geometry2D::Circle(15.0f, Dia::Maths::Vector2D(px - 30.0f, kY));
        pair.aaRect  = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(px + 5.0f, kY - 18.0f),
            Dia::Maths::Vector2D(px + 40.0f, kY + 18.0f));
        pair.hit = pair.circleA.IsIntersecting(pair.aaRect).IsIntersecting();
        mIntersectionPairs.Add(pair);
    }

    // Pair 2: AARect/Triangle — HIT
    {
        IntersectionPair pair;
        pair.kind     = IntersectionPair::Kind::AARectTriangle;
        const float px = kStartX + kSpacing * 2;
        pair.aaRect   = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(px - 30.0f, kY - 25.0f),
            Dia::Maths::Vector2D(px + 30.0f, kY + 25.0f));
        pair.triangle = Dia::Geometry2D::Triangle(
            Dia::Maths::Vector2D(px - 15.0f, kY - 10.0f),
            Dia::Maths::Vector2D(px + 15.0f, kY - 10.0f),
            Dia::Maths::Vector2D(px, kY + 15.0f));
        bool anyVertexInside =
            pair.aaRect.IsIntersecting(pair.triangle.GetPt(Dia::Geometry2D::Triangle::kPt0)).IsIntersecting() ||
            pair.aaRect.IsIntersecting(pair.triangle.GetPt(Dia::Geometry2D::Triangle::kPt1)).IsIntersecting() ||
            pair.aaRect.IsIntersecting(pair.triangle.GetPt(Dia::Geometry2D::Triangle::kPt2)).IsIntersecting();
        pair.hit = anyVertexInside;
        mIntersectionPairs.Add(pair);
    }

    // Pair 3: Ray/Circle — HIT
    {
        IntersectionPair pair;
        pair.kind    = IntersectionPair::Kind::RayCircle;
        const float px = kStartX + kSpacing * 3;
        pair.ray     = Dia::Geometry2D::Ray(
            Dia::Maths::Vector2D(px - 30.0f, kY),
            Dia::Maths::Vector2D(1.0f, 0.0f));
        pair.circleA = Dia::Geometry2D::Circle(15.0f, Dia::Maths::Vector2D(px + 20.0f, kY));
        Dia::Geometry2D::RaycastHit hitInfo;
        pair.hit = Dia::Geometry2D::Raycast::CastCircle(pair.ray, pair.circleA, hitInfo);
        mIntersectionPairs.Add(pair);
    }

    // Pair 4: Line/AARect — MISS
    {
        IntersectionPair pair;
        pair.kind   = IntersectionPair::Kind::LineAARect;
        const float px = kStartX + kSpacing * 4;
        pair.line   = Dia::Geometry2D::Line(
            Dia::Maths::Vector2D(px - 35.0f, kY - 15.0f),
            Dia::Maths::Vector2D(px - 5.0f, kY + 15.0f));
        pair.aaRect = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(px + 10.0f, kY - 18.0f),
            Dia::Maths::Vector2D(px + 45.0f, kY + 18.0f));
        bool ep1Inside = pair.aaRect.IsIntersecting(pair.line.GetPt1()).IsIntersecting();
        bool ep2Inside = pair.aaRect.IsIntersecting(pair.line.GetPt2()).IsIntersecting();
        pair.hit = ep1Inside || ep2Inside;
        mIntersectionPairs.Add(pair);
    }

    // Pair 5: Circle/Triangle — MISS
    {
        IntersectionPair pair;
        pair.kind     = IntersectionPair::Kind::CircleTriangle;
        const float px = kStartX + kSpacing * 5;
        pair.circleA  = Dia::Geometry2D::Circle(15.0f, Dia::Maths::Vector2D(px - 25.0f, kY));
        pair.triangle = Dia::Geometry2D::Triangle(
            Dia::Maths::Vector2D(px + 10.0f, kY - 18.0f),
            Dia::Maths::Vector2D(px + 45.0f, kY - 18.0f),
            Dia::Maths::Vector2D(px + 28.0f, kY + 18.0f));
        pair.hit = pair.circleA.IsIntersecting(pair.triangle).IsIntersecting();
        mIntersectionPairs.Add(pair);
    }

    mIntersectionPairCount = static_cast<int>(mIntersectionPairs.Size());

    DIA_LOG_INFO("CluicheTest", "Geometry2DTestStageModule — %d shapes, %d intersection pairs",
        mShapeCount, mIntersectionPairCount);
}

void Geometry2DTestStageModule::SetupSpatialStructures()
{
    // Bottom band: 4 structures
    // BVH, Quadtree, SpatialGrid (right side x∈[600,1350])
    // HexGrid (bottom-left under console, x∈[50,300])

    constexpr float kBoundsW = 220.0f;
    constexpr float kBoundsH = 260.0f;
    constexpr float kY0 = 80.0f;
    constexpr float kY1 = kY0 + kBoundsH;
    constexpr float kGap = 30.0f;
    constexpr float kX0 = 620.0f;

    auto makeScatter = [](float baseX, float baseY, float w, float h,
                          Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::AARect, 8>& out)
    {
        constexpr float s = 25.0f;
        out.Add(Dia::Geometry2D::AARect(Dia::Maths::Vector2D(baseX + w*0.15f, baseY + h*0.2f),
                                        Dia::Maths::Vector2D(baseX + w*0.15f + s, baseY + h*0.2f + s)));
        out.Add(Dia::Geometry2D::AARect(Dia::Maths::Vector2D(baseX + w*0.5f,  baseY + h*0.3f),
                                        Dia::Maths::Vector2D(baseX + w*0.5f + s,  baseY + h*0.3f + s)));
        out.Add(Dia::Geometry2D::AARect(Dia::Maths::Vector2D(baseX + w*0.7f,  baseY + h*0.6f),
                                        Dia::Maths::Vector2D(baseX + w*0.7f + s,  baseY + h*0.6f + s)));
        out.Add(Dia::Geometry2D::AARect(Dia::Maths::Vector2D(baseX + w*0.2f,  baseY + h*0.7f),
                                        Dia::Maths::Vector2D(baseX + w*0.2f + s,  baseY + h*0.7f + s)));
        out.Add(Dia::Geometry2D::AARect(Dia::Maths::Vector2D(baseX + w*0.8f,  baseY + h*0.15f),
                                        Dia::Maths::Vector2D(baseX + w*0.8f + s,  baseY + h*0.15f + s)));
        out.Add(Dia::Geometry2D::AARect(Dia::Maths::Vector2D(baseX + w*0.4f,  baseY + h*0.8f),
                                        Dia::Maths::Vector2D(baseX + w*0.4f + s,  baseY + h*0.8f + s)));
    };

    // --- BVH ---
    {
        Dia::Geometry2D::BVH<SpatialElem, kSpatialMax>::Def def;
        def.maxLeafObjects = 2;
        mBVH = std::make_unique<Dia::Geometry2D::BVH<SpatialElem, kSpatialMax>>(def);

        Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::AARect, 8> bounds;
        makeScatter(kX0, kY0, kBoundsW, kBoundsH, bounds);

        Dia::Core::Containers::DynamicArrayC<
            typename Dia::Geometry2D::BVH<SpatialElem, kSpatialMax>::BuildEntry, kSpatialMax> entries;
        for (unsigned int i = 0; i < bounds.Size(); ++i)
        {
            typename Dia::Geometry2D::BVH<SpatialElem, kSpatialMax>::BuildEntry e;
            e.object = i;
            e.bounds = bounds[i];
            entries.Add(e);
        }
        mBVH->Build(entries);
    }

    // --- Quadtree (with one tiny object to force deeper subdivision) ---
    {
        const float qx = kX0 + kBoundsW + kGap;
        Dia::Geometry2D::Quadtree<SpatialElem, kSpatialMax>::Def def;
        def.worldBounds = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(qx, kY0),
            Dia::Maths::Vector2D(qx + kBoundsW, kY1));
        def.splitThreshold = 2;
        def.maxDepth = 5;
        mQuadtree = std::make_unique<Dia::Geometry2D::Quadtree<SpatialElem, kSpatialMax>>(def);

        Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::AARect, 8> bounds;
        makeScatter(qx, kY0, kBoundsW, kBoundsH, bounds);
        for (unsigned int i = 0; i < bounds.Size(); ++i)
            mQuadtree->Insert(i, bounds[i]);
        // Extra tiny object in top-left quadrant to force deeper split
        const float tinyS = 8.0f;
        mQuadtree->Insert(10u, Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(qx + 15.0f, kY1 - 30.0f),
            Dia::Maths::Vector2D(qx + 15.0f + tinyS, kY1 - 30.0f + tinyS)));
        mQuadtree->Insert(11u, Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(qx + 25.0f, kY1 - 50.0f),
            Dia::Maths::Vector2D(qx + 25.0f + tinyS, kY1 - 50.0f + tinyS)));
        mQuadtree->Insert(12u, Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(qx + 10.0f, kY1 - 45.0f),
            Dia::Maths::Vector2D(qx + 10.0f + tinyS, kY1 - 45.0f + tinyS)));
    }

    // --- SpatialGrid ---
    {
        const float gx = kX0 + (kBoundsW + kGap) * 2;
        Dia::Geometry2D::SpatialGrid<SpatialElem, kSpatialMax>::Def def;
        def.worldBounds = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(gx, kY0),
            Dia::Maths::Vector2D(gx + kBoundsW, kY1));
        def.cellSize = 55.0f;
        mSpatialGrid = std::make_unique<Dia::Geometry2D::SpatialGrid<SpatialElem, kSpatialMax>>(def);

        Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::AARect, 8> bounds;
        makeScatter(gx, kY0, kBoundsW, kBoundsH, bounds);
        for (unsigned int i = 0; i < bounds.Size(); ++i)
            mSpatialGrid->Insert(i, bounds[i]);
    }

    // --- HexGrid (bottom-left, under console) ---
    {
        constexpr float hx0 = 50.0f, hy0 = 50.0f;
        constexpr float hW = 450.0f, hH = 300.0f;
        Dia::Geometry2D::HexGrid<SpatialElem, kSpatialMax>::Def def;
        def.worldBounds = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(hx0, hy0),
            Dia::Maths::Vector2D(hx0 + hW, hy0 + hH));
        def.hexRadius = 28.0f;
        mHexGrid = std::make_unique<Dia::Geometry2D::HexGrid<SpatialElem, kSpatialMax>>(def);

        constexpr float s = 20.0f;
        mHexGrid->Insert(0u, Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(hx0 + 60.0f, hy0 + 80.0f),
            Dia::Maths::Vector2D(hx0 + 60.0f + s, hy0 + 80.0f + s)));
        mHexGrid->Insert(1u, Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(hx0 + 180.0f, hy0 + 120.0f),
            Dia::Maths::Vector2D(hx0 + 180.0f + s, hy0 + 120.0f + s)));
        mHexGrid->Insert(2u, Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(hx0 + 300.0f, hy0 + 60.0f),
            Dia::Maths::Vector2D(hx0 + 300.0f + s, hy0 + 60.0f + s)));
        mHexGrid->Insert(3u, Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(hx0 + 120.0f, hy0 + 200.0f),
            Dia::Maths::Vector2D(hx0 + 120.0f + s, hy0 + 200.0f + s)));
    }
}
#endif

} // namespace CluicheTest

namespace { using Geometry2DTestStageModule_ = CluicheTest::Geometry2DTestStageModule; }
DIA_MODULE(Geometry2DTestStageModule_);
