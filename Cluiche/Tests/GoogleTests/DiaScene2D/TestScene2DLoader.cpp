#include <gtest/gtest.h>

#include <DiaScene2D/SceneLoader2D.h>
#include <DiaScene2D/SceneLoadContext.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaEntity/Domain.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdio>
#include <cstring>

using namespace Dia::Scene2D;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// Helper — write a temp .diascene file and return its path
// ---------------------------------------------------------------------------
static const char* WriteTempScene(const char* filename, const char* content)
{
    FILE* f = fopen(filename, "wb");
    if (!f) return nullptr;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    return filename;
}

// ---------------------------------------------------------------------------
// Minimal valid scene — one active camera
// ---------------------------------------------------------------------------
static const char* kMinimalScene = R"({
  "scene2d": {
    "cameras": [
      { "id": {"value": "main"}, "active": true, "blueprint": {"value": "basic_cam"} }
    ]
  }
})";

// ---------------------------------------------------------------------------
// Full scene — layers, camera, light, entities
// ---------------------------------------------------------------------------
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
        "instance_data": { "Camera2D.position": [200.0, 150.0], "Camera2D.zoom": 1.5 }
      }
    ],
    "lights": [
      {
        "id": {"value": "torch"},
        "enabled": true,
        "blueprint": {"value": "warm_light"},
        "affects_layers": [ {"value": "foreground"} ],
        "instance_data": { "PointLight2D.position": [100.0, 50.0], "PointLight2D.radius": 80.0 }
      }
    ],
    "entities": [
      { "id": {"value": "player"}, "blueprint": {"value": "hero"}, "enabled": true },
      { "id": {"value": "crate"},  "blueprint": {"value": "prop"}, "enabled": true }
    ]
  }
})";

// ---------------------------------------------------------------------------
// Invalid scene — zero active cameras
// ---------------------------------------------------------------------------
static const char* kNoActiveCameraScene = R"({
  "scene2d": {
    "cameras": [
      { "id": {"value": "cam1"}, "active": false, "blueprint": {"value": "b"} }
    ]
  }
})";

// ---------------------------------------------------------------------------
// Invalid scene — two active cameras
// ---------------------------------------------------------------------------
static const char* kTwoActiveCamerasScene = R"({
  "scene2d": {
    "cameras": [
      { "id": {"value": "cam1"}, "active": true, "blueprint": {"value": "b"} },
      { "id": {"value": "cam2"}, "active": true, "blueprint": {"value": "b"} }
    ]
  }
})";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(DiaScene2D_Loader, Load_MinimalScene_Succeeds)
{
    const char* path = WriteTempScene("_test_minimal.diascene", kMinimalScene);
    ASSERT_NE(path, nullptr);

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;
    SceneLoadErrors  errors;

    SceneLoader2D loader;
    bool ok = loader.Load(path, ctx, layerTable, &errors);

    EXPECT_TRUE(ok);
    EXPECT_FALSE(errors.hasErrors);
    EXPECT_TRUE(cameras.Has(StringCRC("main")));
    EXPECT_EQ(cameras.GetActiveId(), StringCRC("main"));

    remove(path);
}

TEST(DiaScene2D_Loader, Load_FullScene_CameraHydrated)
{
    const char* path = WriteTempScene("_test_full.diascene", kFullScene);
    ASSERT_NE(path, nullptr);

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;

    SceneLoader2D loader;
    bool ok = loader.Load(path, ctx, layerTable, nullptr);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(cameras.Has(StringCRC("gameplay")));
    EXPECT_EQ(cameras.GetActiveId(), StringCRC("gameplay"));

    const Dia::Camera2D::Camera2D& cam = cameras.Get(StringCRC("gameplay"));
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 200.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 150.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 1.5f);

    remove(path);
}

TEST(DiaScene2D_Loader, Load_FullScene_LightHydrated)
{
    const char* path = WriteTempScene("_test_light.diascene", kFullScene);
    ASSERT_NE(path, nullptr);

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;

    SceneLoader2D loader;
    bool ok = loader.Load(path, ctx, layerTable, nullptr);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(lights.Has(StringCRC("torch")));

    const Dia::Lighting2D::PointLight2D& light = lights.Get(StringCRC("torch"));
    EXPECT_FLOAT_EQ(light.position.x, 100.0f);
    EXPECT_FLOAT_EQ(light.position.y,  50.0f);
    EXPECT_FLOAT_EQ(light.radius, 80.0f);
    EXPECT_TRUE(light.enabled);

    // "foreground" layer mask should be set; "background" should not
    unsigned int foreBit = layerTable.GetBitIndex(StringCRC("foreground"));
    unsigned int backBit = layerTable.GetBitIndex(StringCRC("background"));
    EXPECT_TRUE(light.layerMask & (1u << foreBit));
    EXPECT_FALSE(light.layerMask & (1u << backBit));

    remove(path);
}

TEST(DiaScene2D_Loader, Load_FullScene_EntitiesSpawned)
{
    const char* path = WriteTempScene("_test_entities.diascene", kFullScene);
    ASSERT_NE(path, nullptr);

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;

    SceneLoader2D loader;
    bool ok = loader.Load(path, ctx, layerTable, nullptr);

    EXPECT_TRUE(ok);
    EXPECT_EQ(domain.GetEntityCount(), 2u);

    remove(path);
}

TEST(DiaScene2D_Loader, Load_FullScene_LayerTableBuilt)
{
    const char* path = WriteTempScene("_test_layers.diascene", kFullScene);
    ASSERT_NE(path, nullptr);

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;

    SceneLoader2D loader;
    loader.Load(path, ctx, layerTable, nullptr);

    // "default" injected + background + foreground = 3 layers
    EXPECT_EQ(layerTable.GetCount(), 3u);
    EXPECT_TRUE(layerTable.Has(StringCRC("background")));
    EXPECT_TRUE(layerTable.Has(StringCRC("foreground")));
    EXPECT_TRUE(layerTable.Has(StringCRC("default")));

    remove(path);
}

TEST(DiaScene2D_Loader, Load_RejectsZeroActiveCameras)
{
    const char* path = WriteTempScene("_test_noactive.diascene", kNoActiveCameraScene);
    ASSERT_NE(path, nullptr);

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;
    SceneLoadErrors  errors;

    SceneLoader2D loader;
    bool ok = loader.Load(path, ctx, layerTable, &errors);

    EXPECT_FALSE(ok);
    EXPECT_TRUE(errors.hasErrors);

    remove(path);
}

TEST(DiaScene2D_Loader, Load_RejectsTwoActiveCameras)
{
    const char* path = WriteTempScene("_test_twoactive.diascene", kTwoActiveCamerasScene);
    ASSERT_NE(path, nullptr);

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;
    SceneLoadErrors  errors;

    SceneLoader2D loader;
    bool ok = loader.Load(path, ctx, layerTable, &errors);

    EXPECT_FALSE(ok);
    EXPECT_TRUE(errors.hasErrors);

    remove(path);
}

TEST(DiaScene2D_Loader, Unload_ClearsCameraAndLightRegistries)
{
    const char* path = WriteTempScene("_test_unload.diascene", kFullScene);
    ASSERT_NE(path, nullptr);

    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;

    SceneLoader2D loader;
    loader.Load(path, ctx, layerTable, nullptr);

    EXPECT_TRUE(cameras.Has(StringCRC("gameplay")));
    EXPECT_TRUE(lights.Has(StringCRC("torch")));

    loader.Unload(ctx);

    EXPECT_FALSE(cameras.Has(StringCRC("gameplay")));
    EXPECT_FALSE(lights.Has(StringCRC("torch")));
    EXPECT_EQ(domain.GetEntityCount(), 0u);

    remove(path);
}

TEST(DiaScene2D_Loader, Load_MissingFile_ReturnsFalse)
{
    Dia::Camera2D::CameraRegistry2D  cameras;
    Dia::Lighting2D::LightRegistry2D lights;
    Dia::Entity::Domain              domain;
    SceneLoadContext ctx{cameras, lights, domain};
    LayerTable       layerTable;
    SceneLoadErrors  errors;

    SceneLoader2D loader;
    bool ok = loader.Load("nonexistent_scene.diascene", ctx, layerTable, &errors);

    EXPECT_FALSE(ok);
    EXPECT_TRUE(errors.hasErrors);
}
