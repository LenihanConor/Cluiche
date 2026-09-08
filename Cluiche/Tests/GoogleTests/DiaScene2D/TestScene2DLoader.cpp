#include <gtest/gtest.h>

#include <DiaScene2D/SceneLoader2D.h>
#include <DiaScene2D/SceneLoadContext.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaEntity/Domain.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Testing/MockSink.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <cstdio>
#include <cstring>

using namespace Dia::Scene2D;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void WriteFile(const char* filename, const char* content)
{
    FILE* f = nullptr;
#ifdef _MSC_VER
    fopen_s(&f, filename, "wb");
#else
    f = fopen(filename, "wb");
#endif
    if (!f) return;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

// RAII MockSink registration — also registers thread buffer so FlushSync drains correctly.
struct ScopedMockSink
{
    Dia::Observation::Log::MockSink sink;

    ScopedMockSink()
    {
        Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
        Dia::Observation::Log::Logger::Instance().RegisterSink(&sink);
    }
    ~ScopedMockSink()
    {
        Dia::Observation::Log::Logger::Instance().UnregisterSink(&sink);
        Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
    }

    bool HasLevel(Dia::Observation::Log::LogLevel level) const
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
        for (unsigned int i = 0; i < sink.Count(); ++i)
            if (sink.entries[i].entry.level == level) return true;
        return false;
    }
};

// ---------------------------------------------------------------------------
// Scene fixtures
// ---------------------------------------------------------------------------

static const char* kMinimalScene = R"({
  "scene2d": {
    "cameras": [
      { "id": {"value": "main"}, "active": true, "blueprint": {"value": "basic_cam"} }
    ]
  }
})";

static const char* kFullScene = R"({
  "scene2d": {
    "layers": [
      { "id": {"value": "background"}, "sort_order": -10, "sort_policy": {"value": "insertion"}, "enabled": true },
      { "id": {"value": "foreground"}, "sort_order":  10, "sort_policy": {"value": "insertion"}, "enabled": true }
    ],
    "cameras": [
      {
        "id": {"value": "gameplay"},
        "active": true,
        "blueprint": {"value": "follow_cam"},
        "instance_data": {
          "Camera2D.position": [200.0, 150.0],
          "Camera2D.zoom": 1.5,
          "Camera2D.rotation": 30.0
        }
      }
    ],
    "lights": [
      {
        "id": {"value": "torch"},
        "enabled": true,
        "blueprint": {"value": "warm_light"},
        "affects_layers": [ {"value": "foreground"} ],
        "instance_data": {
          "PointLight2D.position": [100.0, 50.0],
          "PointLight2D.radius": 80.0,
          "PointLight2D.intensity": 0.8,
          "PointLight2D.colour": [1.0, 0.8, 0.3, 1.0]
        }
      },
      {
        "id": {"value": "ambient"},
        "enabled": false,
        "blueprint": {"value": "cool_light"},
        "affects_layers": []
      }
    ],
    "entities": [
      { "id": {"value": "player"}, "blueprint": {"value": "hero"}, "enabled": true },
      { "id": {"value": "crate"},  "blueprint": {"value": "prop"}, "enabled": true },
      { "id": {"value": "dormant"}, "blueprint": {"value": "ghost"}, "enabled": false }
    ]
  }
})";

static const char* kNoActiveCameraScene = R"({
  "scene2d": {
    "cameras": [
      { "id": {"value": "cam1"}, "active": false, "blueprint": {"value": "b"} }
    ]
  }
})";

static const char* kTwoActiveCamerasScene = R"({
  "scene2d": {
    "cameras": [
      { "id": {"value": "cam1"}, "active": true, "blueprint": {"value": "b"} },
      { "id": {"value": "cam2"}, "active": true, "blueprint": {"value": "b"} }
    ]
  }
})";

static const char* kMalformedJson = R"({ "scene2d": { NOT VALID JSON )";

static const char* kMissingScene2DKey = R"({ "scene3d": {} })";

static const char* kEmptyAffectsLayersScene = R"({
  "scene2d": {
    "cameras": [
      { "id": {"value": "cam"}, "active": true, "blueprint": {"value": "b"} }
    ],
    "lights": [
      {
        "id": {"value": "global_light"},
        "enabled": true,
        "blueprint": {"value": "b"},
        "affects_layers": []
      }
    ]
  }
})";

// ---------------------------------------------------------------------------
// Happy path
// ---------------------------------------------------------------------------

TEST(DiaScene2D_Loader, Load_MinimalScene_Succeeds)
{
    WriteFile("_test_minimal.diascene", kMinimalScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable; SceneLoadErrors errors;

    SceneLoader2D loader;
    bool ok = loader.Load("_test_minimal.diascene", ctx, layerTable, &errors);

    EXPECT_TRUE(ok);
    EXPECT_FALSE(errors.hasErrors);
    EXPECT_TRUE(cameras.Has(StringCRC("main")));
    EXPECT_EQ(cameras.GetActiveId(), StringCRC("main"));
    remove("_test_minimal.diascene");
}

TEST(DiaScene2D_Loader, Load_FullScene_CameraHydrated_AllFields)
{
    WriteFile("_test_cam.diascene", kFullScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    EXPECT_TRUE(loader.Load("_test_cam.diascene", ctx, layerTable, nullptr));

    const Dia::Camera2D::Camera2D& cam = cameras.Get(StringCRC("gameplay"));
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 200.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 150.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 1.5f);
    EXPECT_FLOAT_EQ(cam.GetRotation(), 30.0f);
    EXPECT_EQ(cameras.GetActiveId(), StringCRC("gameplay"));
    remove("_test_cam.diascene");
}

TEST(DiaScene2D_Loader, Load_FullScene_LightHydrated_AllFields)
{
    WriteFile("_test_light.diascene", kFullScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    EXPECT_TRUE(loader.Load("_test_light.diascene", ctx, layerTable, nullptr));

    const Dia::Lighting2D::PointLight2D& torch = lights.Get(StringCRC("torch"));
    EXPECT_FLOAT_EQ(torch.position.x, 100.0f);
    EXPECT_FLOAT_EQ(torch.position.y,  50.0f);
    EXPECT_FLOAT_EQ(torch.radius, 80.0f);
    EXPECT_FLOAT_EQ(torch.intensity, 0.8f);
    EXPECT_FLOAT_EQ(torch.colour[0], 1.0f);
    EXPECT_FLOAT_EQ(torch.colour[1], 0.8f);
    EXPECT_FLOAT_EQ(torch.colour[2], 0.3f);
    EXPECT_FLOAT_EQ(torch.colour[3], 1.0f);
    EXPECT_TRUE(torch.enabled);

    unsigned int foreBit = layerTable.GetBitIndex(StringCRC("foreground"));
    unsigned int backBit = layerTable.GetBitIndex(StringCRC("background"));
    EXPECT_TRUE (torch.layerMask & (1u << foreBit));
    EXPECT_FALSE(torch.layerMask & (1u << backBit));
    remove("_test_light.diascene");
}

TEST(DiaScene2D_Loader, Load_FullScene_DisabledLightRegistered_EnabledFalse)
{
    WriteFile("_test_disabled_light.diascene", kFullScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    EXPECT_TRUE(loader.Load("_test_disabled_light.diascene", ctx, layerTable, nullptr));

    EXPECT_TRUE(lights.Has(StringCRC("ambient")));
    EXPECT_FALSE(lights.Get(StringCRC("ambient")).enabled);
    remove("_test_disabled_light.diascene");
}

TEST(DiaScene2D_Loader, Load_FullScene_DisabledEntityNotSpawned)
{
    WriteFile("_test_disabled_entity.diascene", kFullScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    EXPECT_TRUE(loader.Load("_test_disabled_entity.diascene", ctx, layerTable, nullptr));

    // 2 enabled entities (player + crate), 1 disabled (dormant) → should not be spawned
    EXPECT_EQ(domain.GetEntityCount(), 2u);
    remove("_test_disabled_entity.diascene");
}

TEST(DiaScene2D_Loader, Load_FullScene_LayerTableBuilt)
{
    WriteFile("_test_layers.diascene", kFullScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_test_layers.diascene", ctx, layerTable, nullptr);

    EXPECT_EQ(layerTable.GetCount(), 3u);  // default + background + foreground
    EXPECT_TRUE(layerTable.Has(StringCRC("default")));
    EXPECT_TRUE(layerTable.Has(StringCRC("background")));
    EXPECT_TRUE(layerTable.Has(StringCRC("foreground")));
    remove("_test_layers.diascene");
}

TEST(DiaScene2D_Loader, Load_EmptyAffectsLayers_ZeroMask)
{
    WriteFile("_test_zero_mask.diascene", kEmptyAffectsLayersScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    EXPECT_TRUE(loader.Load("_test_zero_mask.diascene", ctx, layerTable, nullptr));

    EXPECT_TRUE(lights.Has(StringCRC("global_light")));
    EXPECT_EQ(lights.Get(StringCRC("global_light")).layerMask, 0u);
    remove("_test_zero_mask.diascene");
}

// ---------------------------------------------------------------------------
// Unload
// ---------------------------------------------------------------------------

TEST(DiaScene2D_Loader, Unload_ClearsAllRegistries)
{
    WriteFile("_test_unload.diascene", kFullScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_test_unload.diascene", ctx, layerTable, nullptr);

    EXPECT_TRUE(cameras.Has(StringCRC("gameplay")));
    EXPECT_TRUE(lights.Has(StringCRC("torch")));
    EXPECT_EQ(domain.GetEntityCount(), 2u);

    loader.Unload(ctx);

    EXPECT_FALSE(cameras.Has(StringCRC("gameplay")));
    EXPECT_FALSE(lights.Has(StringCRC("torch")));
    EXPECT_FALSE(lights.Has(StringCRC("ambient")));
    EXPECT_EQ(domain.GetEntityCount(), 0u);
    remove("_test_unload.diascene");
}

TEST(DiaScene2D_Loader, UnloadThenReload_WorksCleanly)
{
    WriteFile("_test_reload.diascene", kMinimalScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    EXPECT_TRUE(loader.Load("_test_reload.diascene", ctx, layerTable, nullptr));
    loader.Unload(ctx);
    EXPECT_FALSE(cameras.Has(StringCRC("main")));

    LayerTable layerTable2;
    EXPECT_TRUE(loader.Load("_test_reload.diascene", ctx, layerTable2, nullptr));
    EXPECT_TRUE(cameras.Has(StringCRC("main")));
    loader.Unload(ctx);
    remove("_test_reload.diascene");
}

// ---------------------------------------------------------------------------
// Validation errors
// ---------------------------------------------------------------------------

TEST(DiaScene2D_Loader, Load_RejectsZeroActiveCameras)
{
    WriteFile("_test_noactive.diascene", kNoActiveCameraScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable; SceneLoadErrors errors;

    SceneLoader2D loader;
    EXPECT_FALSE(loader.Load("_test_noactive.diascene", ctx, layerTable, &errors));
    EXPECT_TRUE(errors.hasErrors);
    remove("_test_noactive.diascene");
}

TEST(DiaScene2D_Loader, Load_RejectsTwoActiveCameras)
{
    WriteFile("_test_twoactive.diascene", kTwoActiveCamerasScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable; SceneLoadErrors errors;

    SceneLoader2D loader;
    EXPECT_FALSE(loader.Load("_test_twoactive.diascene", ctx, layerTable, &errors));
    EXPECT_TRUE(errors.hasErrors);
    remove("_test_twoactive.diascene");
}

TEST(DiaScene2D_Loader, Load_MissingFile_ReturnsFalse)
{
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable; SceneLoadErrors errors;

    SceneLoader2D loader;
    EXPECT_FALSE(loader.Load("_nonexistent.diascene", ctx, layerTable, &errors));
    EXPECT_TRUE(errors.hasErrors);
}

TEST(DiaScene2D_Loader, Load_MalformedJson_ReturnsFalse)
{
    WriteFile("_test_malformed.diascene", kMalformedJson);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable; SceneLoadErrors errors;

    SceneLoader2D loader;
    EXPECT_FALSE(loader.Load("_test_malformed.diascene", ctx, layerTable, &errors));
    EXPECT_TRUE(errors.hasErrors);
    remove("_test_malformed.diascene");
}

TEST(DiaScene2D_Loader, Load_MissingScene2DKey_ReturnsFalse)
{
    WriteFile("_test_nokey.diascene", kMissingScene2DKey);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable; SceneLoadErrors errors;

    SceneLoader2D loader;
    EXPECT_FALSE(loader.Load("_test_nokey.diascene", ctx, layerTable, &errors));
    EXPECT_TRUE(errors.hasErrors);
    remove("_test_nokey.diascene");
}

// ---------------------------------------------------------------------------
// Observability — logging
// ---------------------------------------------------------------------------

TEST(DiaScene2D_Loader_Observability, Load_Success_LogsInfo)
{
    WriteFile("_test_obs_ok.diascene", kMinimalScene);
    ScopedMockSink mock;

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    EXPECT_TRUE(loader.Load("_test_obs_ok.diascene", ctx, layerTable, nullptr));
    EXPECT_TRUE(mock.HasLevel(Dia::Observation::Log::LogLevel::kInfo));
    remove("_test_obs_ok.diascene");
}

TEST(DiaScene2D_Loader_Observability, Load_MissingFile_LogsError)
{
    ScopedMockSink mock;
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_missing_obs.diascene", ctx, layerTable, nullptr);
    EXPECT_TRUE(mock.HasLevel(Dia::Observation::Log::LogLevel::kError));
}

TEST(DiaScene2D_Loader_Observability, Load_CameraValidationFail_LogsError)
{
    WriteFile("_test_obs_camfail.diascene", kTwoActiveCamerasScene);
    ScopedMockSink mock;

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_test_obs_camfail.diascene", ctx, layerTable, nullptr);
    EXPECT_TRUE(mock.HasLevel(Dia::Observation::Log::LogLevel::kError));
    remove("_test_obs_camfail.diascene");
}

TEST(DiaScene2D_Loader_Observability, Load_MalformedJson_LogsError)
{
    WriteFile("_test_obs_bad.diascene", kMalformedJson);
    ScopedMockSink mock;

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_test_obs_bad.diascene", ctx, layerTable, nullptr);
    EXPECT_TRUE(mock.HasLevel(Dia::Observation::Log::LogLevel::kError));
    remove("_test_obs_bad.diascene");
}

// ---------------------------------------------------------------------------
// Observability — metrics
// ---------------------------------------------------------------------------

TEST(DiaScene2D_Loader_Observability, Metrics_LoadsTotal_Increments)
{
    WriteFile("_test_metric_ok.diascene", kMinimalScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_test_metric_ok.diascene", ctx, layerTable, nullptr);

    auto* counter = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(
        Dia::Core::StringCRC("dia.scene2d.loads_total"));
    ASSERT_NE(counter, nullptr);
    EXPECT_GE(counter->Value(), 1u);
    remove("_test_metric_ok.diascene");
}

TEST(DiaScene2D_Loader_Observability, Metrics_LoadFailures_IncrementOnError)
{
    SceneLoader2D loader;
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    auto* failCounter = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(
        Dia::Core::StringCRC("dia.scene2d.load_failures"));
    uint64_t before = failCounter ? failCounter->Value() : 0u;

    loader.Load("_no_such_file.diascene", ctx, layerTable, nullptr);

    ASSERT_NE(failCounter, nullptr);
    EXPECT_GT(failCounter->Value(), before);
}

TEST(DiaScene2D_Loader_Observability, Metrics_CamerasLoaded_ReflectsCount)
{
    WriteFile("_test_metric_cam.diascene", kMinimalScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_test_metric_cam.diascene", ctx, layerTable, nullptr);

    auto* gauge = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(
        Dia::Core::StringCRC("dia.scene2d.cameras_loaded"));
    ASSERT_NE(gauge, nullptr);
    EXPECT_DOUBLE_EQ(gauge->Value(), 1.0);
    remove("_test_metric_cam.diascene");
}

TEST(DiaScene2D_Loader_Observability, Metrics_Unload_ResetsGauges)
{
    WriteFile("_test_metric_unload.diascene", kMinimalScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_test_metric_unload.diascene", ctx, layerTable, nullptr);
    loader.Unload(ctx);

    auto* camGauge = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(
        Dia::Core::StringCRC("dia.scene2d.cameras_loaded"));
    ASSERT_NE(camGauge, nullptr);
    EXPECT_DOUBLE_EQ(camGauge->Value(), 0.0);
    remove("_test_metric_unload.diascene");
}

// ---------------------------------------------------------------------------
// Observability — health
// ---------------------------------------------------------------------------

TEST(DiaScene2D_Loader_Observability, Health_AfterSuccessfulLoad_IsOK)
{
    WriteFile("_test_health_ok.diascene", kMinimalScene);
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    SceneLoader2D loader;
    loader.Load("_test_health_ok.diascene", ctx, layerTable, nullptr);

    auto health = loader.Report();
    EXPECT_EQ(health.status, Dia::Observation::Health::HealthStatus::kOK);
    remove("_test_health_ok.diascene");
}

TEST(DiaScene2D_Loader_Observability, Health_AfterFailedLoad_IsFailing)
{
    SceneLoader2D loader;
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable layerTable;

    loader.Load("_missing_health_test.diascene", ctx, layerTable, nullptr);

    auto health = loader.Report();
    EXPECT_EQ(health.status, Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_GT(health.errors, 0u);
}

TEST(DiaScene2D_Loader_Observability, Health_ReporterName)
{
    SceneLoader2D loader;
    EXPECT_EQ(loader.GetReporterName(), Dia::Core::StringCRC("dia.scene2d.loader"));
}
