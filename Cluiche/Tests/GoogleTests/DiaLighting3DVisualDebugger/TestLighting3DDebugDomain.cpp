////////////////////////////////////////////////////////////////////////////////
// TestLighting3DDebugDomain.cpp
// AC-15 mandatory test shapes for the Lighting3D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (OnCommand setScale)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaLighting3DVisualDebugger/Lighting3DDebugDomain.h>

#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaLighting3D/Testing/LightBuilder3D.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::Lighting3D;

namespace
{

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

uint32_t Count3DPrimsOfType(const Dia::Graphics::FrameData& fd,
                             Dia::Graphics::DebugPrimitiveType t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < fd.GetDebug3DPrimitiveCount(); ++i)
        if (fd.GetDebug3DPrimitive(i).type == t) ++n;
    return n;
}

} // namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(Lighting3DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Lighting3DDebugDomain domain(builder.Registry());

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("Lighting3D"));
    EXPECT_STREQ(domain.GetDisplayName(), "Lighting3D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Rendering"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(Lighting3DDebugDomain_Identity, DescriptionWithin80Chars)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Lighting3DDebugDomain domain(builder.Registry());

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(Lighting3DDebugDomain_Identity, AccentIsRenderingGroupConstant)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Lighting3DDebugDomain domain(builder.Registry());

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kRendering);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(Lighting3DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), Lighting3DDebugDomain::kDrawerCount);
    for (int i = 0; i < Lighting3DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer " << i;
    EXPECT_EQ(domain.GetDrawer(Lighting3DDebugDomain::kDrawerCount), nullptr);
}

TEST(Lighting3DDebugDomain_Lifecycle, RegisterAddsAllThreeLayers)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Lighting3DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kLightWidgets));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kLightPathArc));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kLightRanges));
}

TEST(Lighting3DDebugDomain_Lifecycle, LayersCarryTheLighting3DStageTag)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("Lighting3D")));
}

TEST(Lighting3DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kLightWidgets));
}

TEST(Lighting3DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Lighting3DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(Lighting3DDebugDomain_DrawerGate, DisabledWidgetsDrawerEmitsNoSpheres)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("point0");
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    Dia::Graphics::FrameData enabled;
    mgr.Draw(enabled);
    ASSERT_GE(Count3DPrimsOfType(enabled, Dia::Graphics::DebugPrimitiveType::Sphere3D), 1u)
        << "precondition: widgets draw when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kLightWidgets);
    mgr.DisableLayer(Dia::Debug::LayerNames::kLightPathArc);
    mgr.DisableLayer(Dia::Debug::LayerNames::kLightRanges);

    Dia::Graphics::FrameData disabled;
    mgr.Draw(disabled);
    EXPECT_EQ(disabled.GetDebug3DPrimitiveCount(), 0u);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(Lighting3DDebugDomain_Primitives, WidgetsDrawerEmitsSphereForPointLight)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("p0");
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    // Isolate widgets drawer.
    mgr.DisableLayer(Dia::Debug::LayerNames::kLightPathArc);
    mgr.DisableLayer(Dia::Debug::LayerNames::kLightRanges);

    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Count3DPrimsOfType(fd, Dia::Graphics::DebugPrimitiveType::Sphere3D), 1u);
}

TEST(Lighting3DDebugDomain_Primitives, EmptyRegistryEmitsNoPrimitives)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;  // no lights
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(fd.GetDebug3DPrimitiveCount(), 0u);
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(Lighting3DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(Lighting3DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

// ===========================================================================
// AC-15 #4 — GetJSONState round-trip
// ===========================================================================

TEST(Lighting3DDebugDomain_JSONState, ReportsTwoDrawersAndAStatsObject)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(Lighting3DDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());

    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        EXPECT_FALSE(state["drawers"][i]["name"].asString().empty());
        EXPECT_TRUE(state["drawers"][i]["enabled"].asBool()) << "drawers start enabled";
    }
}

TEST(Lighting3DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kLightWidgets);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "PositionWidgets");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(Lighting3DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Lighting3DDebugDomain domain(builder.Registry());

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 3u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(Lighting3DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    const Dia::Core::StringCRC widgets = Dia::Debug::LayerNames::kLightWidgets;
    ASSERT_TRUE(mgr.IsLayerEnabled(widgets));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("PositionWidgets"));
    EXPECT_FALSE(mgr.IsLayerEnabled(widgets));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("PositionWidgets"));
    EXPECT_TRUE(mgr.IsLayerEnabled(widgets));
}

TEST(Lighting3DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("light3d.path_arc"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kLightPathArc));
}

TEST(Lighting3DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("PathArcs"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kLightPathArc));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kLightWidgets));
}

TEST(Lighting3DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < Lighting3DDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(Lighting3DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Widgets"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

// ===========================================================================
// Stats fields — GetJSONState() light count assertions
// ===========================================================================

TEST(Lighting3DDebugDomain_JSONState_Stats, LightCountFieldsPresent)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    ASSERT_TRUE(stats.isObject());
    EXPECT_TRUE(stats.isMember("pointLightCount"))       << "stats.pointLightCount missing";
    EXPECT_TRUE(stats["pointLightCount"].isInt());
    EXPECT_TRUE(stats.isMember("directionalLightCount")) << "stats.directionalLightCount missing";
    EXPECT_TRUE(stats["directionalLightCount"].isInt());
    EXPECT_TRUE(stats.isMember("spotLightCount"))        << "stats.spotLightCount missing";
    EXPECT_TRUE(stats["spotLightCount"].isInt());
}

TEST(Lighting3DDebugDomain_JSONState_Stats, LightCountsReflectRegistryPopulation)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("p0");
    builder.WithPoint("p1");
    Dia::Debug::DebugLayerManager             mgr;
    Lighting3DDebugDomain domain(builder.Registry());
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_EQ(state["stats"]["pointLightCount"].asInt(), 2);
    EXPECT_EQ(state["stats"]["directionalLightCount"].asInt(), 0);
    EXPECT_EQ(state["stats"]["spotLightCount"].asInt(), 0);
}

#endif // DIA_DEBUG
