////////////////////////////////////////////////////////////////////////////////
// TestGeometry2DDebugDomain.cpp
// AC-15 mandatory test shapes for the Geometry2D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type (submit-per-frame)
//   3. scale sensitivity (OnCommand setScale)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaGeometry2DVisualDebugger/Geometry2DDebugDomain.h>
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2DVisualDebugger/AABBOverlayDrawer.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>

#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cstring>

using namespace Dia::Geometry2DVisualDebugger;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;
using namespace Dia::Maths;

namespace
{

RecordingDebugVisitor Inspect(const FrameData& fd)
{
    RecordingDebugVisitor v;
    static_cast<const Dia::Graphics::DebugFrameData&>(fd).AcceptVisitor(v);
    return v;
}

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

TEST(Geometry2DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    Geometry2DDebugDomain domain;

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("Geometry2D"));
    EXPECT_STREQ(domain.GetDisplayName(), "Geometry2D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Spatial"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(Geometry2DDebugDomain_Identity, DescriptionWithin80Chars)
{
    Geometry2DDebugDomain domain;
    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(Geometry2DDebugDomain_Identity, AccentIsSpatialGroupConstant)
{
    Geometry2DDebugDomain domain;
    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kSpatial);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(Geometry2DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), Geometry2DDebugDomain::kDrawerCount);
    for (int i = 0; i < Geometry2DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(domain.GetDrawer(Geometry2DDebugDomain::kDrawerCount), nullptr);
}

TEST(Geometry2DDebugDomain_Lifecycle, RegisterAddsAllThreeLayers)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Geometry2DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kGeoShapes));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kGeoLabels));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kGeoAABB));
}

TEST(Geometry2DDebugDomain_Lifecycle, LayersCarryTheGeometry2DStageTag)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("Geometry2D")));
}

TEST(Geometry2DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kGeoShapes));
}

TEST(Geometry2DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);
    domain.Register(mgr);   // must not double-register

    EXPECT_EQ(mgr.GetLayerCount(), Geometry2DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(Geometry2DDebugDomain_DrawerGate, DisabledShapesDrawerEmitsNoCircles)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    // Submit a circle and draw — should appear since layer is enabled.
    auto* sd = static_cast<ShapeDrawer*>(domain.GetDrawer(0));
    sd->SubmitCircle(Dia::Geometry2D::Circle(1.0f, Vector2D::Zero()), Dia::Graphics::RGBA(255,255,255,255));
    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    ASSERT_GE(Inspect(enabledFrame).CircleCount(), 1) << "precondition: shapes draw when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kGeoShapes);

    sd->SubmitCircle(Dia::Geometry2D::Circle(1.0f, Vector2D::Zero()), Dia::Graphics::RGBA(255,255,255,255));
    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(Inspect(disabledFrame).CircleCount(), 0);
}

TEST(Geometry2DDebugDomain_DrawerGate, DisablingEveryLayerEmitsNothing)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    for (int i = 0; i < Geometry2DDebugDomain::kDrawerCount; ++i)
        mgr.DisableLayer(domain.GetDrawer(i)->GetLayerName());

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Inspect(fd).TotalCount(), 0);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(Geometry2DDebugDomain_Primitives, ShapesDrawerEmitsCircleWhenSubmitted)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    // Disable Labels and AABB so only Shapes drawer is active.
    mgr.DisableLayer(Dia::Debug::LayerNames::kGeoLabels);
    mgr.DisableLayer(Dia::Debug::LayerNames::kGeoAABB);

    auto* sd = static_cast<ShapeDrawer*>(domain.GetDrawer(0));
    sd->SubmitCircle(Dia::Geometry2D::Circle(1.0f, Vector2D::Zero()), Dia::Graphics::RGBA(255,255,255,255));

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Inspect(fd).CircleCount(), 1) << "ShapeDrawer emits 1 circle when submitted";
}

TEST(Geometry2DDebugDomain_Primitives, NoSubmitMeansNoOutput)
{
    // Submit-per-frame: Draw() with no shapes submitted emits nothing.
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Inspect(fd).TotalCount(), 0) << "submit-per-frame: no Submit means no output";
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(Geometry2DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 3.0f);
}

TEST(Geometry2DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
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

TEST(Geometry2DDebugDomain_JSONState, ReportsEveryDrawerAndAStatsObject)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(Geometry2DDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());

    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        EXPECT_TRUE(state["drawers"][i].isMember("name"));
        EXPECT_FALSE(state["drawers"][i]["name"].asString().empty());
        EXPECT_TRUE(state["drawers"][i].isMember("enabled"));
        EXPECT_TRUE(state["drawers"][i]["enabled"].asBool()) << "drawers start enabled";
    }
}

TEST(Geometry2DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kGeoShapes);

    Json::Value state;
    domain.GetJSONState(state);
    ASSERT_EQ(state["drawers"].size(), 3u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Shapes");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(Geometry2DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    Geometry2DDebugDomain domain;
    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 3u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(Geometry2DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    const Dia::Core::StringCRC shapes = Dia::Debug::LayerNames::kGeoShapes;
    ASSERT_TRUE(mgr.IsLayerEnabled(shapes));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Shapes"));
    EXPECT_FALSE(mgr.IsLayerEnabled(shapes));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Shapes"));
    EXPECT_TRUE(mgr.IsLayerEnabled(shapes));
}

TEST(Geometry2DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("geometry.labels"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kGeoLabels));
}

TEST(Geometry2DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("AABB"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kGeoAABB));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kGeoShapes));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kGeoLabels));
}

TEST(Geometry2DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < Geometry2DDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(Geometry2DDebugDomain_OnCommand, MalformedCommandsAreIgnored)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kGeoShapes));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(Geometry2DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    Dia::Debug::DebugLayerManager mgr;
    Geometry2DDebugDomain domain;

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Shapes"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

#endif // DIA_DEBUG
