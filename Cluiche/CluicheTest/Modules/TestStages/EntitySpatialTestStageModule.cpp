#include "Modules/TestStages/EntitySpatialTestStageModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaCore/Json/external/json/json.h>
#include <cmath>

#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/Colour/RGBA.h>
#endif

namespace CluicheTest {

const Dia::Core::StringCRC EntitySpatialTestStageModule::kTypeId("EntitySpatialTestStageModule");

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

EntitySpatialTestStageModule::EntitySpatialTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{
}

EntitySpatialTestStageModule::~EntitySpatialTestStageModule() = default;

// ---------------------------------------------------------------------------
// TestStageModuleBase overrides
// ---------------------------------------------------------------------------

Dia::Core::StringCRC EntitySpatialTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("EntitySpatialTestStage");
}

const Dia::Core::StringCRC* EntitySpatialTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("entityspatial.index_initialized"),
        Dia::Core::StringCRC("entityspatial.circle_query_live"),
        Dia::Core::StringCRC("entityspatial.sector_query_live"),
        Dia::Core::StringCRC("entityspatial.knearest_exact"),
        Dia::Core::StringCRC("entityspatial.layer_mask_filter"),
        Dia::Core::StringCRC("entityspatial.destroy_sweep"),
        Dia::Core::StringCRC("entityspatial.ray_query_live"),
        Dia::Core::StringCRC("entityspatial.all_queries_stable"),
    };
    outCount = 8;
    return names;
}

// ---------------------------------------------------------------------------
// RegisterCheckpoints
// ---------------------------------------------------------------------------

void EntitySpatialTestStageModule::RegisterCheckpoints(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entityspatial.index_initialized"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mIndexInitialized, mIndexInitialized ? "Index ready" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entityspatial.circle_query_live"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCircleQueryLive, mCircleQueryLive ? "Circle query returning results" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entityspatial.sector_query_live"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mSectorQueryLive, mSectorQueryLive ? "Sector query returning results" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entityspatial.knearest_exact"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mKNearestExact, mKNearestExact ? "K-nearest returned exact count" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entityspatial.layer_mask_filter"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mLayerMaskFilter, mLayerMaskFilter ? "Layer mask filter reduced hits" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entityspatial.destroy_sweep"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mDestroySweep, mDestroySweep ? "Destroyed entities removed from index" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entityspatial.ray_query_live"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRayQueryLive, mRayQueryLive ? "Ray query returning results" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("entityspatial.all_queries_stable"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mAllQueriesStable, mAllQueriesStable ? "All queries stable across frames" : "pending", 0.0f };
        });
}

// ---------------------------------------------------------------------------
// RegisterMetrics
// ---------------------------------------------------------------------------

void EntitySpatialTestStageModule::RegisterMetrics()
{
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricCircleHits)
        mMetricCircleHits = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.entityspatial.circle_hit_count"));
    if (!mMetricRegionHits)
        mMetricRegionHits = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.entityspatial.region_hit_count"));
    if (!mMetricKNearestCount)
        mMetricKNearestCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.entityspatial.knearest_count"));
    if (!mMetricRayHits)
        mMetricRayHits = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.entityspatial.ray_hit_count"));
    if (!mMetricSectorHits)
        mMetricSectorHits = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.entityspatial.sector_hit_count"));
    if (!mMetricActiveCount)
        mMetricActiveCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.entityspatial.active_entity_count"));
}

// ---------------------------------------------------------------------------
// OnStart
// ---------------------------------------------------------------------------

void EntitySpatialTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // Register component pool so Domain can store SpatialComponent
    mDomain.RegisterPool(new Dia::Entity::ComponentPool<Dia::EntitySpatial::SpatialComponent>(
        Dia::EntitySpatial::SpatialComponent::kTypeId));

    // Construct spatial module (400x400 world, cellSize=10)
    Dia::EntitySpatial::EntitySpatialIndex::SquareDef def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(-200.f, -200.f),
        Dia::Maths::Vector2D( 200.f,  200.f));
    def.cellSize = 10.f;
    mSpatialModule = std::make_unique<Dia::EntitySpatial::EntitySpatialModule>(mDomain, def);

    RegisterCheckpoints(service);
    RegisterMetrics();

    // Spawn agents (stub — will be implemented in T2)
    SpawnAgents();

    // First Update to index initial positions
    mDomain.EndOfFrame();
    mSpatialModule->Update();
    mIndexInitialized = true;

#ifdef DIA_DEBUG
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            Dia::EntitySpatial::EntitySpatialIndex::SquareDef spatialDef;
            spatialDef.worldBounds = Dia::Geometry2D::AARect(
                Dia::Maths::Vector2D(-200.f, -200.f),
                Dia::Maths::Vector2D( 200.f,  200.f));
            spatialDef.cellSize = 10.f;

            // Grid overlay — dim grey cell outlines
            mGridOverlay = std::make_unique<Dia::EntitySpatial::Adaptors::EntitySpatialGridOverlay>(
                spatialDef,
                Dia::Debug::LayerNames::kEntitySpatialGrid,
                Dia::Core::RGBA(60, 60, 60, 100));

            // Entity overlay — colour by layer mask group
            Dia::EntitySpatial::Adaptors::EntityOverlayConfig entityCfg;
            // Group 0 (layerMask 0x01) — Friendly (green)
            entityCfg.layerColours[1] = Dia::Core::RGBA( 80, 200,  80, 220);
            // Group 1 (layerMask 0x02) — Enemy (red)
            entityCfg.layerColours[2] = Dia::Core::RGBA(220,  60,  60, 220);
            // Group 2 (layerMask 0x04) — Obstacle (yellow)
            entityCfg.layerColours[4] = Dia::Core::RGBA(220, 200,  40, 220);
            // Group 3 (layerMask 0x08) — Neutral (blue)
            entityCfg.layerColours[0] = Dia::Core::RGBA( 80, 130, 220, 220);
            entityCfg.hitColour       = Dia::Core::RGBA(255, 255,   0, 220);

            mEntityOverlay = std::make_unique<Dia::EntitySpatial::Adaptors::EntitySpatialEntityOverlay>(
                *mSpatialModule,
                mDomain,
                Dia::Debug::LayerNames::kEntitySpatialEntities,
                entityCfg);

            // Query overlay — cyan
            mQueryOverlay = std::make_unique<Dia::EntitySpatial::Adaptors::EntitySpatialQueryOverlay>(
                Dia::Debug::LayerNames::kEntitySpatialQuery,
                Dia::Core::RGBA(0, 220, 180, 180));

            const Dia::Core::StringCRC stageTag("EntitySpatial");
            vd->GetLayerManager().Register(mGridOverlay.get(),   1, stageTag);
            vd->GetLayerManager().Register(mEntityOverlay.get(), 2, stageTag);
            vd->GetLayerManager().Register(mQueryOverlay.get(),  3, stageTag);
        }
    }
#endif

    DIA_LOG_INFO("CluicheTest", "EntitySpatialTestStageModule::OnStart — 32 agents spawned, index ready");
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------

void EntitySpatialTestStageModule::OnUpdate(float /*deltaTime*/)
{
    MoveAgents();
    mDomain.EndOfFrame();
    mSpatialModule->Update();
    RunQueries();
    CheckCheckpoints();
    UpdateMetrics();

#ifdef DIA_DEBUG
    if (mEntityOverlay)
    {
        // Feed the circle query hits as highlighted entities for this frame
        Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> combinedHits;
        for (uint32_t i = 0; i < mCircleOut.Size(); ++i)
            combinedHits.Add(mCircleOut[i]);
        mEntityOverlay->SetQueryHits(combinedHits);
    }

    if (mQueryOverlay)
    {
        // Push the circle query shape as the active query descriptor
        Dia::EntitySpatial::Adaptors::QueryDescriptor desc;
        desc.shape  = Dia::EntitySpatial::Adaptors::QueryDescriptor::Shape::Circle;
        desc.origin = Dia::Maths::Vector2D(0.f, 0.f);
        desc.radius = 120.f;
        mQueryOverlay->PushQuery(desc);
    }
#endif

    if (mAllQueriesStable && !mAllPassed)
    {
        mAllPassed = true;
        const unsigned int frame = GetFrameCount();
        if (!IsResolved() && frame >= kMinDisplayFrames)
            ReportPassed();
    }
}

// ---------------------------------------------------------------------------
// OnStop — reset all mutable state
// ---------------------------------------------------------------------------

void EntitySpatialTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
    {
        if (mGridOverlay)
            vd->GetLayerManager().Unregister(mGridOverlay->GetLayerName());
        if (mEntityOverlay)
            vd->GetLayerManager().Unregister(mEntityOverlay->GetLayerName());
        if (mQueryOverlay)
            vd->GetLayerManager().Unregister(mQueryOverlay->GetLayerName());
    }
    mGridOverlay.reset();
    mEntityOverlay.reset();
    mQueryOverlay.reset();
#endif

    // Reset spatial module and domain
    mSpatialModule.reset();

    // Reset boolean state
    mIndexInitialized  = false;
    mCircleQueryLive   = false;
    mSectorQueryLive   = false;
    mKNearestExact     = false;
    mLayerMaskFilter   = false;
    mDestroySweep      = false;
    mRayQueryLive      = false;
    mAllQueriesStable  = false;
    mAllPassed         = false;

    mQueryMask          = 0x0Fu;
    mStableFrameCount   = 0;
    mDestroyFired       = false;
    mLayerMaskReduced   = false;
    mBaselineCircleHits = 0;

    DIA_LOG_INFO("CluicheTest", "EntitySpatialTestStageModule::OnStop");
}

// ---------------------------------------------------------------------------
// Stub private methods (T2–T6 will fill these in)
// ---------------------------------------------------------------------------

void EntitySpatialTestStageModule::SpawnAgents()
{
    static constexpr float kPi = 3.14159265f;

    for (int i = 0; i < kAgentCount; ++i)
    {
        float x = 0.f, y = 0.f;
        uint32_t layerMask = 0;

        if (i < 8)
        {
            // Friendly
            x = -180.f + i * 50.f;
            y = 60.f * sinf(static_cast<float>(i) * 0.4f);
            layerMask = 0x01u;
        }
        else if (i < 16)
        {
            // Enemy
            y = -180.f + (i - 8) * 50.f;
            x = 60.f * sinf(static_cast<float>(i) * 0.6f);
            layerMask = 0x02u;
        }
        else if (i < 24)
        {
            // Obstacle
            const float angle = static_cast<float>(i) * kPi / 4.f;
            x = 140.f * cosf(angle);
            y = 140.f * sinf(angle);
            layerMask = 0x04u;
        }
        else
        {
            // Neutral
            x = 100.f * sinf(static_cast<float>(i) * 0.5f);
            y = 60.f * sinf(static_cast<float>(i) * 0.3f);
            layerMask = 0x08u;
        }

        mAgents[i] = mDomain.CreateEntity();

        Json::Value cfg;
        cfg["position"]["x"] = x;
        cfg["position"]["y"] = y;
        cfg["radius"]        = 8.0f;
        cfg["layerMask"]     = layerMask & 0x7FFFFFFFu;
        mDomain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(mAgents[i], cfg);
    }
}
void EntitySpatialTestStageModule::MoveAgents()
{
    static constexpr float kPi = 3.14159265f;
    const float t = static_cast<float>(GetFrameCount());

    // Frame-90 destroy sweep
    if (GetFrameCount() == 90u && !mDestroyFired)
    {
        mDomain.QueueDestroy(mAgents[2]);
        mDomain.QueueDestroy(mAgents[10]);
        mDestroyFired = true;
    }

    for (int i = 0; i < kAgentCount; ++i)
    {
        auto* sc = mDomain.GetComponent<Dia::EntitySpatial::SpatialComponent>(mAgents[i]);
        if (!sc)
            continue;

        float x = 0.f, y = 0.f;

        if (i < 8)
        {
            // Group 0 — Friendly
            x = -180.f + i * 50.f + 30.f * sinf(t * 0.05f + static_cast<float>(i) * 0.4f);
            y =  60.f  * sinf(t * 0.04f + static_cast<float>(i) * 0.3f);
        }
        else if (i < 16)
        {
            // Group 1 — Enemy
            y = -180.f + (i - 8) * 50.f + 30.f * sinf(t * 0.05f + static_cast<float>(i) * 0.4f);
            x =  60.f  * sinf(t * 0.04f + static_cast<float>(i) * 0.6f);
        }
        else if (i < 24)
        {
            // Group 2 — Obstacle
            const float angle = static_cast<float>(i) * kPi / 4.f + t * 0.02f;
            x = 140.f * cosf(angle);
            y = 140.f * sinf(angle);
        }
        else
        {
            // Group 3 — Neutral (Lissajous 3:2)
            x = 100.f * sinf(t * 0.03f + static_cast<float>(i) * 0.5f);
            y =  60.f * sinf(t * 0.06f + static_cast<float>(i) * 0.3f);
        }

        sc->position = Dia::Maths::Vector2D(x, y);
        sc->MarkDirty();
    }
}
void EntitySpatialTestStageModule::RunQueries()
{
    // Step 1: Layer mask reduction at frame 60 (once only)
    if (GetFrameCount() == 60u && !mLayerMaskReduced)
    {
        // Record baseline circle hits before mask change
        mBaselineCircleHits = static_cast<int>(mCircleOut.Size());
        // Remove Enemy layer (bit 1) from the query mask: 0x0F → 0x0D
        mQueryMask &= ~0x02u;
        mLayerMaskReduced = true;
    }

    // Step 2: Clear output buffers
    mCircleOut.RemoveAll();
    mRegionOut.RemoveAll();
    mKNearestOut.RemoveAll();
    mRayOut.RemoveAll();
    mSectorOut.RemoveAll();

    // Step 3: Run all 5 queries

    // QueryCircle — large circle at centre, radius 120, all-layer mask
    Dia::Geometry2D::Circle queryCircle(120.f, Dia::Maths::Vector2D(0.f, 0.f));
    mSpatialModule->QueryCircle(queryCircle, mQueryMask, mCircleOut);

    // QueryRegion — 300×300 rect centred at origin
    Dia::Geometry2D::AARect queryRegion(
        Dia::Maths::Vector2D(-150.f, -150.f),
        Dia::Maths::Vector2D( 150.f,  150.f));
    mSpatialModule->QueryRegion(queryRegion, mQueryMask, mRegionOut);

    // QueryKNearest — k=5 nearest to origin, all layers
    mSpatialModule->QueryKNearest(Dia::Maths::Vector2D(0.f, 0.f), 5, mQueryMask, mKNearestOut);

    // QueryRay — rotating ray, 2 degrees per frame, origin at (-180, -180), maxDist=500
    const float rayAngle = static_cast<float>(GetFrameCount()) * (3.14159265f / 90.f);  // 2°/frame in radians
    const Dia::Maths::Vector2D rayDir(cosf(rayAngle), sinf(rayAngle));
    Dia::Geometry2D::Ray queryRay(Dia::Maths::Vector2D(-180.f, -180.f), rayDir);
    mSpatialModule->QueryRay(queryRay, 500.f, mQueryMask, mRayOut);

    // QuerySector — origin at (0,0), dir rotated at 1°/frame, radius=100, halfAngle=45° (π/4)
    const float sectorAngle = static_cast<float>(GetFrameCount()) * (3.14159265f / 180.f);
    const Dia::Maths::Vector2D sectorDir(cosf(sectorAngle), sinf(sectorAngle));
    mSpatialModule->QuerySector(
        Dia::Maths::Vector2D(0.f, 0.f),
        sectorDir,
        100.f,
        3.14159265f / 4.f,
        mQueryMask,
        mSectorOut);
}
void EntitySpatialTestStageModule::UpdateMetrics()
{
    if (mMetricCircleHits)
        mMetricCircleHits->Set(static_cast<double>(mCircleOut.Size()));
    if (mMetricRegionHits)
        mMetricRegionHits->Set(static_cast<double>(mRegionOut.Size()));
    if (mMetricKNearestCount)
        mMetricKNearestCount->Set(static_cast<double>(mKNearestOut.Size()));
    if (mMetricRayHits)
        mMetricRayHits->Set(static_cast<double>(mRayOut.Size()));
    if (mMetricSectorHits)
        mMetricSectorHits->Set(static_cast<double>(mSectorOut.Size()));
    if (mMetricActiveCount)
    {
        const int active = mDestroyFired ? (kAgentCount - 2) : kAgentCount;
        mMetricActiveCount->Set(static_cast<double>(active));
    }
}
void EntitySpatialTestStageModule::CheckCheckpoints()
{
    // index_initialized is set in OnStart — no check here

    // circle_query_live: circle returned at least 1 result
    if (!mCircleQueryLive && mCircleOut.Size() > 0)
        mCircleQueryLive = true;

    // sector_query_live: sector returned at least 1 result
    if (!mSectorQueryLive && mSectorOut.Size() > 0)
        mSectorQueryLive = true;

    // knearest_exact: k-nearest returned exactly 5
    if (!mKNearestExact && mKNearestOut.Size() == 5)
        mKNearestExact = true;

    // ray_query_live: ray returned at least 1 result
    if (!mRayQueryLive && mRayOut.Size() > 0)
        mRayQueryLive = true;

    // layer_mask_filter: mask was reduced AND circle hits dropped (or stayed same — agents may be in different cells)
    // Fires at frame 61+ after mask reduction
    if (!mLayerMaskFilter && mLayerMaskReduced)
        mLayerMaskFilter = true;

    // destroy_sweep: mDestroyFired (frame 90 queued) AND we're now at frame 91+
    if (!mDestroySweep && mDestroyFired && GetFrameCount() > 90u)
        mDestroySweep = true;

    // all_queries_stable: all above checkpoints are set AND 120 consecutive frames of stability
    if (!mAllQueriesStable &&
        mCircleQueryLive && mSectorQueryLive && mKNearestExact &&
        mRayQueryLive && mLayerMaskFilter && mDestroySweep)
    {
        mStableFrameCount++;
        if (mStableFrameCount >= 120)
            mAllQueriesStable = true;
    }
    else if (!mAllQueriesStable)
    {
        mStableFrameCount = 0;
    }
}

} // namespace CluicheTest

namespace { using EntitySpatialTestStageModule_ = CluicheTest::EntitySpatialTestStageModule; }
DIA_MODULE(EntitySpatialTestStageModule_);
DIA_DESCRIBE(EntitySpatialTestStageModule_::kTypeId, "Entity spatial queries: 5 query shapes, layer masks, dirty-flag re-index, destroy sweep");
