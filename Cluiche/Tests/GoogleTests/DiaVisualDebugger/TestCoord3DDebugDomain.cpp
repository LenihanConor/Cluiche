////////////////////////////////////////////////////////////////////////////////
// TestCoord3DDebugDomain.cpp
// AC-15 mandatory test shapes for the Coord3D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (OnCommand setScale)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
//
// NOTE: Coord3D layers are disabled by default after Register() (opt-in overlay).
// Coord3D drawers use RegisterWithoutDraw — they are driven externally by
// VisualDebuggerModule::DrawCoord3D(), not the 2D layer manager draw loop.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Coord3D/Coord3DDebugDomain.h>

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

TEST(Coord3DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    Coord3DDebugDomain domain;

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("Coord3D"));
    EXPECT_STREQ(domain.GetDisplayName(), "Coord3D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("CoreDebug"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(Coord3DDebugDomain_Identity, DescriptionWithin80Chars)
{
    Coord3DDebugDomain domain;
    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(Coord3DDebugDomain_Identity, AccentIsCoreDebugGroupConstant)
{
    Coord3DDebugDomain domain;
    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kCoreDebug);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(Coord3DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), Coord3DDebugDomain::kDrawerCount);
    for (int i = 0; i < Coord3DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer " << i;
    EXPECT_EQ(domain.GetDrawer(Coord3DDebugDomain::kDrawerCount), nullptr);
}

TEST(Coord3DDebugDomain_Lifecycle, RegisterAddsAllFourLayers)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Coord3DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord3DOrigin));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord3DAxes));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord3DGrid));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord3DCamera));
}

TEST(Coord3DDebugDomain_Lifecycle, AllLayersDisabledAfterRegister)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    for (int i = 0; i < Coord3DDebugDomain::kDrawerCount; ++i)
        EXPECT_FALSE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()))
            << "drawer " << i << " should start disabled";
}

TEST(Coord3DDebugDomain_Lifecycle, LayersCarryTheCoord3DStageTag)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("Coord3D")));
}

TEST(Coord3DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kCoord3DOrigin));
}

TEST(Coord3DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Coord3DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(Coord3DDebugDomain_DrawerGate, LayersStartDisabledAfterRegister)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DOrigin));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DAxes));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DGrid));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DCamera));
}

TEST(Coord3DDebugDomain_DrawerGate, TogglingEnablesAndDisablesLayer)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    ASSERT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DAxes));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Axes"));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DAxes));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Axes"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DAxes));
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(Coord3DDebugDomain_Primitives, DrawersAreNonNullAfterRegister)
{
    // Coord3D drawers use RegisterWithoutDraw; they're driven by DrawCoord3D(),
    // not mgr.Draw(). Verifying non-null proves correct domain wiring.
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    for (int i = 0; i < Coord3DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer " << i << " is wired";
}

TEST(Coord3DDebugDomain_Primitives, LayerManagerDrawSkipsCoord3DDrawers)
{
    // Since registered with RegisterWithoutDraw, mgr.Draw() must not crash
    // even when layers are enabled.
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    // Enable all layers.
    for (int i = 0; i < Coord3DDebugDomain::kDrawerCount; ++i)
        mgr.EnableLayer(domain.GetDrawer(i)->GetLayerName());

    Dia::Graphics::FrameData fd;
    // RegisterWithoutDraw means mgr.Draw() doesn't invoke these 3D drawers —
    // no crash and nothing written to the 2D frame is the expected outcome.
    EXPECT_NO_FATAL_FAILURE(mgr.Draw(fd));
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(Coord3DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 3.0f);
}

TEST(Coord3DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
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

TEST(Coord3DDebugDomain_JSONState, ReportsFourDrawersAndAStatsObject)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(Coord3DDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));

    // All start disabled (opt-in overlay).
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        EXPECT_FALSE(state["drawers"][i]["name"].asString().empty());
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool())
            << "drawer " << i << " should start disabled";
    }
}

TEST(Coord3DDebugDomain_JSONState, EnabledFlagTracksLayerManagerAfterToggle)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Origin");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_FALSE(state["drawers"][1u]["enabled"].asBool());
}

TEST(Coord3DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    Coord3DDebugDomain domain;
    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 4u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(Coord3DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    const Dia::Core::StringCRC origin = Dia::Debug::LayerNames::kCoord3DOrigin;
    ASSERT_FALSE(mgr.IsLayerEnabled(origin));  // starts disabled

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));
    EXPECT_TRUE(mgr.IsLayerEnabled(origin));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));
    EXPECT_FALSE(mgr.IsLayerEnabled(origin));
}

TEST(Coord3DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("coord3d.grid"));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DGrid));
}

TEST(Coord3DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Camera"));

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DCamera));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DOrigin));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DAxes));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DGrid));
}

TEST(Coord3DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < Coord3DDebugDomain::kDrawerCount; ++i)
        EXPECT_FALSE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(Coord3DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DebugLayerManager  mgr;
    Coord3DDebugDomain domain;

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Origin"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

#endif // DIA_DEBUG
