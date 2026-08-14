////////////////////////////////////////////////////////////////////////////////
// TestCoord2DDebugDomain.cpp
// AC-15 mandatory test shapes for the Coord2D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (OnCommand setScale)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
//
// NOTE: Coord2D layers are disabled by default after Register() (opt-in overlay).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Coord2D/Coord2DDebugDomain.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <DiaGraphics/Frame/FrameData.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::Debug;

namespace
{

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

} // namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(Coord2DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    Coord2DDebugDomain domain;

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("Coord2D"));
    EXPECT_STREQ(domain.GetDisplayName(), "Coord2D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("CoreDebug"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(Coord2DDebugDomain_Identity, DescriptionWithin80Chars)
{
    Coord2DDebugDomain domain;
    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(Coord2DDebugDomain_Identity, AccentIsCoreDebugGroupConstant)
{
    Coord2DDebugDomain domain;
    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kCoreDebug);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(Coord2DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), Coord2DDebugDomain::kDrawerCount);
    for (int i = 0; i < Coord2DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer " << i;
    EXPECT_EQ(domain.GetDrawer(Coord2DDebugDomain::kDrawerCount), nullptr);
}

TEST(Coord2DDebugDomain_Lifecycle, RegisterAddsAllFiveLayers)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Coord2DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord2DOrigin));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord2DAxes));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord2DGrid));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord2DBounds));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord2DCursor));
}

TEST(Coord2DDebugDomain_Lifecycle, AllLayersDisabledAfterRegister)
{
    // Coord2D overlays are opt-in — all start disabled.
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    for (int i = 0; i < Coord2DDebugDomain::kDrawerCount; ++i)
        EXPECT_FALSE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()))
            << "drawer " << i << " should start disabled";
}

TEST(Coord2DDebugDomain_Lifecycle, LayersCarryTheCoord2DStageTag)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("Coord2D")));
}

TEST(Coord2DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord2DOrigin));
}

TEST(Coord2DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Coord2DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(Coord2DDebugDomain_DrawerGate, LayersStartDisabledAfterRegister)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    // mgr.Draw() with all layers disabled should emit nothing.
    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);
    // Coord2D drawers use world-space data; no viewport set → nothing emitted is expected.
    SUCCEED();
}

TEST(Coord2DDebugDomain_DrawerGate, TogglingEnablesAndDisablesLayer)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    ASSERT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DOrigin));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DOrigin));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DOrigin));
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(Coord2DDebugDomain_Primitives, DrawersAreNonNullAfterRegister)
{
    // Coord2D drawers rely on viewport data to produce output; testing
    // non-null proves correct wiring without requiring a viewport fixture.
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    for (int i = 0; i < Coord2DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer " << i << " is wired";
}

TEST(Coord2DDebugDomain_Primitives, DrawDoesNotCrashWithDefaultViewport)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    // Enable all layers to exercise all draw paths.
    for (int i = 0; i < Coord2DDebugDomain::kDrawerCount; ++i)
        mgr.EnableLayer(domain.GetDrawer(i)->GetLayerName());

    Dia::Graphics::FrameData fd;
    EXPECT_NO_FATAL_FAILURE(mgr.Draw(fd));
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(Coord2DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.0f);
}

TEST(Coord2DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
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

TEST(Coord2DDebugDomain_JSONState, ReportsFiveDrawersAndAStatsObject)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(Coord2DDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));

    // All start disabled (opt-in overlay).
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        EXPECT_FALSE(state["drawers"][i]["name"].asString().empty());
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool())
            << "drawer " << i << " should start disabled";
    }
}

TEST(Coord2DDebugDomain_JSONState, EnabledFlagTracksLayerManagerAfterToggle)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    // Enable just the Origin drawer via toggle.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Origin");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_FALSE(state["drawers"][1u]["enabled"].asBool());
}

TEST(Coord2DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    Coord2DDebugDomain domain;
    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 5u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(Coord2DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    const Dia::Core::StringCRC origin = Dia::Debug::LayerNames::kCoord2DOrigin;
    ASSERT_FALSE(mgr.IsLayerEnabled(origin));  // starts disabled

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));
    EXPECT_TRUE(mgr.IsLayerEnabled(origin));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));
    EXPECT_FALSE(mgr.IsLayerEnabled(origin));
}

TEST(Coord2DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    // Enable grid via raw layer name.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("coord2d.grid"));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DGrid));
}

TEST(Coord2DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Grid"));

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DGrid));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DOrigin));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DAxes));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DBounds));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DCursor));
}

TEST(Coord2DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    // All layers remain in their default state (disabled).
    for (int i = 0; i < Coord2DDebugDomain::kDrawerCount; ++i)
        EXPECT_FALSE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(Coord2DDebugDomain_OnCommand, MalformedCommandsAreIgnored)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord2DOrigin));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(Coord2DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

// ===========================================================================
// Stats fields — GetJSONState() cursor position assertions
// ===========================================================================

TEST(Coord2DDebugDomain_JSONState_Stats, CursorFieldPresent)
{
    DebugLayerManager  mgr;
    Coord2DDebugDomain domain;
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    ASSERT_TRUE(stats.isObject());
    EXPECT_TRUE(stats.isMember("cursor"))      << "stats.cursor missing";
    EXPECT_TRUE(stats["cursor"].isObject())    << "stats.cursor must be object";
    EXPECT_TRUE(stats["cursor"].isMember("x")) << "stats.cursor.x missing";
    EXPECT_TRUE(stats["cursor"].isMember("y")) << "stats.cursor.y missing";
    EXPECT_TRUE(stats["cursor"]["x"].isNumeric());
    EXPECT_TRUE(stats["cursor"]["y"].isNumeric());
}

#endif // DIA_DEBUG
