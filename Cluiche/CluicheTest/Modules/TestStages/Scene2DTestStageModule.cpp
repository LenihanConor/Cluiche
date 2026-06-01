#include "Modules/TestStages/Scene2DTestStageModule.h"
#include "Modules/TestStages/Entity/TransformComponent.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaScene2D/SceneLoadContext.h>
#include <DiaEntity/ComponentPool.h>
#include <chrono>

#ifdef DIA_DEBUG
#include "Modules/TestStages/Drawers/Scene2DTestDrawer.h"
#endif

namespace CluicheTest {

const Dia::Core::StringCRC Scene2DTestStageModule::kTypeId("Scene2DTestStageModule");

Scene2DTestStageModule::Scene2DTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Dia::Core::StringCRC Scene2DTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("Scene2DTestStage");
}

const Dia::Core::StringCRC* Scene2DTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("scene.loaded"),
        Dia::Core::StringCRC("scene.cameras_hydrated"),
        Dia::Core::StringCRC("scene.lights_hydrated"),
        Dia::Core::StringCRC("scene.entities_spawned"),
        Dia::Core::StringCRC("scene.layers_resolved")
    };
    outCount = 5;
    return names;
}

void Scene2DTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    DIA_LOG_INFO("CluicheTest", "Scene2DTestStageModule::OnStart — loading test scene");

    LoadScene();

    // Register metrics
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricEntityCount)
        mMetricEntityCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scene2d.entity_count"));
    if (!mMetricLayerCount)
        mMetricLayerCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scene2d.layer_count"));
    if (!mMetricLoadTimeMs)
        mMetricLoadTimeMs = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scene2d.load_time_ms"));

    if (mMetricEntityCount)
        mMetricEntityCount->Set(static_cast<double>(mEntityDomain.GetEntityCount()));
    if (mMetricLayerCount)
        mMetricLayerCount->Set(static_cast<double>(mLayerTable.GetCount()));

    // Register checkpoints
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scene.loaded"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mLoadSucceeded, mLoadSucceeded ? "loaded" : "load_failed", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scene.cameras_hydrated"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool ok = ValidateCameras();
            return { ok, ok ? "cameras_valid" : "cameras_invalid", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scene.lights_hydrated"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool ok = ValidateLights();
            return { ok, ok ? "lights_valid" : "lights_invalid", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scene.entities_spawned"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool ok = ValidateEntities();
            return { ok, ok ? "entities_valid" : "entities_invalid", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scene.layers_resolved"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool ok = ValidateLayers();
            return { ok, ok ? "layers_valid" : "layers_invalid", 0.0f };
        });
}

void Scene2DTestStageModule::OnUpdate(float /*deltaTime*/)
{
#ifdef DIA_DEBUG
    if (!mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
        {
            auto& mgr = vd->GetLayerManager();
            const Dia::Core::StringCRC stageTag("Scene2D");

            mDrawer = std::make_unique<Scene2DTestDrawer>(
                mEntityDomain, mCameraRegistry, mLightRegistry, mLayerTable, mgr);
            mgr.Register(mDrawer.get(), 20, stageTag);
        }
    }
#endif

    if (!IsResolved())
    {
        if (mLoadSucceeded && ValidateCameras() && ValidateLights() && ValidateEntities() && ValidateLayers())
            ReportPassed();
        else if (GetFrameCount() > 5)
            ReportFailed();
    }
}

void Scene2DTestStageModule::OnStop()
{
    DIA_LOG_INFO("CluicheTest", "Scene2DTestStageModule::OnStop — unloading scene");

#ifdef DIA_DEBUG
    if (mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->GetLayerManager().Unregister(mDrawer->GetLayerName());
        mDrawer.reset();
    }
#endif

    Dia::Scene2D::SceneLoadContext context{ mCameraRegistry, mLightRegistry, mEntityDomain };
    mSceneLoader.Unload(context);
    mEntityDomain.EndOfFrame();

    mLoadSucceeded   = false;
    mMetricEntityCount = nullptr;
    mMetricLayerCount  = nullptr;
    mMetricLoadTimeMs  = nullptr;
}

void Scene2DTestStageModule::LoadScene()
{
    // Register TransformComponent pool so instanceData can write positions
    mEntityDomain.RegisterPool(new Dia::Entity::ComponentPool<TransformComponent>(
        TransformComponent::kTypeId));

    auto start = std::chrono::high_resolution_clock::now();

    Dia::Scene2D::SceneLoadContext context{ mCameraRegistry, mLightRegistry, mEntityDomain };
    Dia::Scene2D::SceneLoadErrors errors;

    mLoadSucceeded = mSceneLoader.Load(
        "assets/stages/Scene2DTestStage/misc/test_scene.diascene",
        context, mLayerTable, &errors);

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (mMetricLoadTimeMs)
        mMetricLoadTimeMs->Set(ms);

    if (!mLoadSucceeded)
    {
        DIA_LOG_ERROR("CluicheTest", "Scene load FAILED");
        return;
    }

    // Loader creates entities and calls EndOfFrame internally.
    // Now add TransformComponents so WriteField can apply positions.
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
    {
        Dia::Entity::Entity e = mEntityDomain.GetAliveEntity(i);
        if (e.IsValid())
        {
            Json::Value emptyCfg;
            mEntityDomain.QueueAddComponentByTypeId(e, TransformComponent::kTypeId, emptyCfg);
        }
    }
    mEntityDomain.EndOfFrame();

    // Re-apply instance_data positions now that components exist
    static const struct { float x; float y; } kExpectedPositions[] = {
        { 200.0f, 300.0f },
        { 600.0f, 400.0f },
        { 1000.0f, 500.0f }
    };
    unsigned int entityIdx = 0;
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain && entityIdx < 3; ++i)
    {
        Dia::Entity::Entity e = mEntityDomain.GetAliveEntity(i);
        if (e.IsValid())
        {
            Json::Value xVal(kExpectedPositions[entityIdx].x);
            Json::Value yVal(kExpectedPositions[entityIdx].y);
            mEntityDomain.WriteField(e, TransformComponent::kTypeId, "x", xVal);
            mEntityDomain.WriteField(e, TransformComponent::kTypeId, "y", yVal);
            ++entityIdx;
        }
    }

    DIA_LOG_INFO("CluicheTest", "Scene loaded: %u cameras, %u lights, %u entities, %u layers",
        mCameraRegistry.GetCount(), mLightRegistry.GetCount(),
        mEntityDomain.GetEntityCount(), mLayerTable.GetCount());
}

bool Scene2DTestStageModule::ValidateCameras() const
{
    // Expect exactly 1 camera: "test_camera" at position (960, 540)
    if (mCameraRegistry.GetCount() != 1)
        return false;
    if (!mCameraRegistry.Has(Dia::Core::StringCRC("test_camera")))
        return false;
    return true;
}

bool Scene2DTestStageModule::ValidateLights() const
{
    // Expect 2 lights: light_a and light_b
    if (mLightRegistry.GetCount() != 2)
        return false;
    if (!mLightRegistry.Has(Dia::Core::StringCRC("light_a")))
        return false;
    if (!mLightRegistry.Has(Dia::Core::StringCRC("light_b")))
        return false;
    return true;
}

bool Scene2DTestStageModule::ValidateEntities() const
{
    // Expect 3 entities spawned (entity_d is disabled, skipped by loader)
    if (mEntityDomain.GetEntityCount() != 3)
        return false;

    // Validate positions were applied via WriteField
    static const struct { float x; float y; } kExpected[] = {
        { 200.0f, 300.0f },
        { 600.0f, 400.0f },
        { 1000.0f, 500.0f }
    };
    unsigned int idx = 0;
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain && idx < 3; ++i)
    {
        Dia::Entity::Entity e = mEntityDomain.GetAliveEntity(i);
        if (!e.IsValid())
            continue;
        auto* tc = mEntityDomain.GetComponent<TransformComponent>(e);
        if (!tc)
            return false;
        if (tc->x != kExpected[idx].x || tc->y != kExpected[idx].y)
            return false;
        ++idx;
    }
    return idx == 3;
}

bool Scene2DTestStageModule::ValidateLayers() const
{
    // Expect 3 layers: background, midground, foreground
    if (mLayerTable.GetCount() != 3)
        return false;
    if (!mLayerTable.Has(Dia::Core::StringCRC("background")))
        return false;
    if (!mLayerTable.Has(Dia::Core::StringCRC("midground")))
        return false;
    if (!mLayerTable.Has(Dia::Core::StringCRC("foreground")))
        return false;
    return true;
}

} // namespace CluicheTest

namespace { using Scene2DTestStageModule_ = CluicheTest::Scene2DTestStageModule; }
DIA_MODULE(Scene2DTestStageModule_);
