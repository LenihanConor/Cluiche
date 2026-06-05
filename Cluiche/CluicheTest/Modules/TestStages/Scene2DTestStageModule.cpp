#include "Modules/TestStages/Scene2DTestStageModule.h"
#include "Modules/TestStages/Entity/TransformComponent.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

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
    DIA_LOG_INFO("CluicheTest", "Scene2DTestStageModule::OnStart");

    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricEntityCount)
        mMetricEntityCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scene2d.entity_count"));
    if (!mMetricLayerCount)
        mMetricLayerCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scene2d.layer_count"));

    auto* scene    = mSceneRef.Get();
    auto* entities = mEntityRef.Get();

    if (scene && scene->IsLoaded() && entities)
    {
        // Register TransformComponent pool and apply test positions.
        // This is test-specific setup — the generic Scene2DModule has no knowledge
        // of TransformComponent; the test stage owns this post-load wiring.
        auto& domain = entities->GetDomain();
        domain.RegisterPool(new Dia::Entity::ComponentPool<TransformComponent>(
            TransformComponent::kTypeId));

        for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
        {
            Dia::Entity::Entity e = domain.GetAliveEntity(i);
            if (e.IsValid())
            {
                Json::Value emptyCfg;
                domain.QueueAddComponentByTypeId(e, TransformComponent::kTypeId, emptyCfg);
            }
        }
        domain.EndOfFrame();

        static const struct { float x; float y; } kPositions[] = {
            { 200.0f, 300.0f },
            { 600.0f, 400.0f },
            { 1000.0f, 500.0f }
        };
        unsigned int idx = 0;
        for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain && idx < 3; ++i)
        {
            Dia::Entity::Entity e = domain.GetAliveEntity(i);
            if (e.IsValid())
            {
                Json::Value xVal(kPositions[idx].x);
                Json::Value yVal(kPositions[idx].y);
                domain.WriteField(e, TransformComponent::kTypeId, "x", xVal);
                domain.WriteField(e, TransformComponent::kTypeId, "y", yVal);
                ++idx;
            }
        }

        if (mMetricEntityCount)
            mMetricEntityCount->Set(static_cast<double>(domain.GetEntityCount()));
        if (mMetricLayerCount)
            mMetricLayerCount->Set(static_cast<double>(scene->GetLayerTable().GetCount()));
    }

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scene.loaded"),
        [this]() -> Dia::Automation::CheckpointResult {
            auto* s = mSceneRef.Get();
            bool ok = s && s->IsLoaded();
            return { ok, ok ? "loaded" : "load_failed", 0.0f };
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
            auto* scene    = mSceneRef.Get();
            auto* entities = mEntityRef.Get();
            auto* cameras  = mCameraRef.Get();
            auto* lights   = mLightRef.Get();

            if (scene && scene->IsLoaded() && entities && cameras && lights)
            {
                auto& mgr = vd->GetLayerManager();
                const Dia::Core::StringCRC stageTag("Scene2D");

                mDrawer = std::make_unique<Scene2DTestDrawer>(
                    entities->GetDomain(),
                    cameras->GetRegistry(),
                    lights->GetRegistry(),
                    scene->GetLayerTable(),
                    mgr);
                mgr.Register(mDrawer.get(), 20, stageTag);
            }
        }
    }
#endif

    if (!IsResolved())
    {
        auto* scene = mSceneRef.Get();
        if (scene && scene->IsLoaded() && ValidateCameras() && ValidateLights() && ValidateEntities() && ValidateLayers())
            ReportPassed();
        else if (GetFrameCount() > 5)
            ReportFailed();
    }
}

void Scene2DTestStageModule::OnStop()
{
    DIA_LOG_INFO("CluicheTest", "Scene2DTestStageModule::OnStop");

#ifdef DIA_DEBUG
    if (mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->GetLayerManager().Unregister(mDrawer->GetLayerName());
        mDrawer.reset();
    }
#endif

    mMetricEntityCount = nullptr;
    mMetricLayerCount  = nullptr;
}

bool Scene2DTestStageModule::ValidateCameras() const
{
    auto* cameras = mCameraRef.Get();
    if (!cameras) return false;
    auto* scene = mSceneRef.Get();
    if (!scene || !scene->IsLoaded()) return false;

    const auto& reg = cameras->GetRegistry();
    // Registry contains Camera2DModule's default camera plus test_camera loaded from scene.
    if (reg.GetCount() != 2)
        return false;
    if (!reg.Has(Dia::Core::StringCRC("test_camera")))
        return false;
    return true;
}

bool Scene2DTestStageModule::ValidateLights() const
{
    auto* lights = mLightRef.Get();
    if (!lights) return false;
    auto* scene = mSceneRef.Get();
    if (!scene || !scene->IsLoaded()) return false;

    const auto& reg = lights->GetRegistry();
    if (reg.GetCount() != 2)
        return false;
    if (!reg.Has(Dia::Core::StringCRC("light_a")))
        return false;
    if (!reg.Has(Dia::Core::StringCRC("light_b")))
        return false;
    return true;
}

bool Scene2DTestStageModule::ValidateEntities() const
{
    auto* entities = mEntityRef.Get();
    if (!entities) return false;
    auto* scene = mSceneRef.Get();
    if (!scene || !scene->IsLoaded()) return false;

    const auto& domain = entities->GetDomain();
    if (domain.GetEntityCount() != 3)
        return false;

    static const struct { float x; float y; } kExpected[] = {
        { 200.0f, 300.0f },
        { 600.0f, 400.0f },
        { 1000.0f, 500.0f }
    };
    unsigned int idx = 0;
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain && idx < 3; ++i)
    {
        Dia::Entity::Entity e = domain.GetAliveEntity(i);
        if (!e.IsValid()) continue;
        auto* tc = domain.GetComponent<TransformComponent>(e);
        if (!tc) return false;
        if (tc->x != kExpected[idx].x || tc->y != kExpected[idx].y) return false;
        ++idx;
    }
    return idx == 3;
}

bool Scene2DTestStageModule::ValidateLayers() const
{
    auto* scene = mSceneRef.Get();
    if (!scene || !scene->IsLoaded())
        return false;
    const auto& layers = scene->GetLayerTable();
    if (layers.GetCount() != 3)
        return false;
    if (!layers.Has(Dia::Core::StringCRC("background")))
        return false;
    if (!layers.Has(Dia::Core::StringCRC("midground")))
        return false;
    if (!layers.Has(Dia::Core::StringCRC("foreground")))
        return false;
    return true;
}

} // namespace CluicheTest

namespace { using Scene2DTestStageModule_ = CluicheTest::Scene2DTestStageModule; }
DIA_MODULE(Scene2DTestStageModule_);
DIA_DESCRIBE(Scene2DTestStageModule_::kTypeId, "Test stage for the 2D scene system: loading levels, layers, and entity placement.");
