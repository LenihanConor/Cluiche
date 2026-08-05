#include "Modules/TestStages/EntitySpatialTestStageModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaCore/Json/external/json/json.h>
#include <cmath>

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
void EntitySpatialTestStageModule::MoveAgents() {}
void EntitySpatialTestStageModule::RunQueries() {}
void EntitySpatialTestStageModule::UpdateMetrics() {}
void EntitySpatialTestStageModule::CheckCheckpoints() {}

} // namespace CluicheTest

namespace { using EntitySpatialTestStageModule_ = CluicheTest::EntitySpatialTestStageModule; }
DIA_MODULE(EntitySpatialTestStageModule_);
DIA_DESCRIBE(EntitySpatialTestStageModule_::kTypeId, "Entity spatial queries: 5 query shapes, layer masks, dirty-flag re-index, destroy sweep");
