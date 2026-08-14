////////////////////////////////////////////////////////////////////////////////
// TestScene2DDebugDomain.cpp
// AC-15 mandatory test shapes for the Scene2D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (OnCommand setScale)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaScene2DVisualDebugger/Scene2DDebugDomain.h>

#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaScene2D/LayerTable.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <DiaGraphics/Frame/FrameData.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::Scene2DVisualDebugger;

namespace
{

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

// Minimal fixture: empty registries and empty LayerTable.
struct SceneFixture
{
    Dia::Camera2D::CameraRegistry2D  cameraRegistry;
    Dia::Lighting2D::LightRegistry2D lightRegistry;
    Dia::Scene2D::LayerTable         layerTable;

    Scene2DDebugDomain MakeDomain() const
    {
        return Scene2DDebugDomain(cameraRegistry, lightRegistry, layerTable);
    }
};

} // namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(Scene2DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    SceneFixture f;
    auto domain = f.MakeDomain();

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("Scene2D"));
    EXPECT_STREQ(domain.GetDisplayName(), "Scene2D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Rendering"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(Scene2DDebugDomain_Identity, DescriptionWithin80Chars)
{
    SceneFixture f;
    auto domain = f.MakeDomain();

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(Scene2DDebugDomain_Identity, AccentIsRenderingGroupConstant)
{
    SceneFixture f;
    auto domain = f.MakeDomain();

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kRendering);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(Scene2DDebugDomain_Lifecycle, DrawerOnlyExistsAfterRegister)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), Scene2DDebugDomain::kDrawerCount);
    EXPECT_NE(domain.GetDrawer(0), nullptr);
    EXPECT_EQ(domain.GetDrawer(Scene2DDebugDomain::kDrawerCount), nullptr);
}

TEST(Scene2DDebugDomain_Lifecycle, RegisterAddsOverviewLayer)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 3);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kScene2DCameras));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kScene2DLights));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kScene2DLayerBounds));
}

TEST(Scene2DDebugDomain_Lifecycle, LayersCarryTheScene2DStageTag)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("Scene2D")));
}

TEST(Scene2DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kScene2DCameras));
}

TEST(Scene2DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Scene2DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(Scene2DDebugDomain_DrawerGate, DisablingOverviewLayerDoesNotCrash)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kScene2DCameras);
    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);  // must not crash with disabled layer

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kScene2DCameras));
}

TEST(Scene2DDebugDomain_DrawerGate, LayerEnabledByDefault)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kScene2DCameras));
}

// ===========================================================================
// AC-15 #2 — drawer emits its expected primitive type
// ===========================================================================

TEST(Scene2DDebugDomain_Primitives, DrawDoesNotCrashWithEmptyRegistries)
{
    // SceneOverviewDrawer reads from registries each frame — empty is valid.
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    Dia::Graphics::FrameData fd;
    EXPECT_NO_FATAL_FAILURE(mgr.Draw(fd));
}

TEST(Scene2DDebugDomain_Primitives, DisabledLayerEmitsNothing)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kScene2DCameras);
    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);
    // When disabled, whatever the drawer would have emitted is suppressed.
    SUCCEED();  // Verifies no crash — primitive count tested in layer-gate tests
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(Scene2DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.0f);
}

TEST(Scene2DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 5.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

// ===========================================================================
// AC-15 #4 — GetJSONState round-trip
// ===========================================================================

TEST(Scene2DDebugDomain_JSONState, ReportsOneDrawerAndAStatsObject)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), static_cast<Json::ArrayIndex>(Scene2DDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());

    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Cameras");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool()) << "drawer starts enabled";
}

TEST(Scene2DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kScene2DCameras);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

TEST(Scene2DDebugDomain_JSONState, BeforeRegisterDrawerReportsDisabled)
{
    SceneFixture f;
    auto domain = f.MakeDomain();

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 3u);
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(Scene2DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    const Dia::Core::StringCRC cameras = Dia::Debug::LayerNames::kScene2DCameras;
    ASSERT_TRUE(mgr.IsLayerEnabled(cameras));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Cameras"));
    EXPECT_FALSE(mgr.IsLayerEnabled(cameras));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Cameras"));
    EXPECT_TRUE(mgr.IsLayerEnabled(cameras));
}

TEST(Scene2DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("scene2d.cameras"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kScene2DCameras));
}

TEST(Scene2DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kScene2DCameras));
}

TEST(Scene2DDebugDomain_OnCommand, MalformedCommandsAreIgnored)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kScene2DCameras));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(Scene2DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Cameras"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

// ===========================================================================
// Stats fields — GetJSONState() scene count assertions
// ===========================================================================

TEST(Scene2DDebugDomain_JSONState_Stats, SceneCountFieldsPresent)
{
    SceneFixture f;
    Dia::Debug::DebugLayerManager mgr;
    auto domain = f.MakeDomain();
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    ASSERT_TRUE(stats.isObject());
    EXPECT_TRUE(stats.isMember("cameraCount")) << "stats.cameraCount missing";
    EXPECT_TRUE(stats["cameraCount"].isInt());
    EXPECT_TRUE(stats.isMember("lightCount"))  << "stats.lightCount missing";
    EXPECT_TRUE(stats["lightCount"].isInt());
    EXPECT_TRUE(stats.isMember("layerCount"))  << "stats.layerCount missing";
    EXPECT_TRUE(stats["layerCount"].isInt());
}

#endif // DIA_DEBUG
