////////////////////////////////////////////////////////////////////////////////
// TestSoftBody2DDebugDomain.cpp
// AC-15 mandatory test shapes for the SoftBody2D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (GetDebugScale changes output measurements)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaSoftBody2DVisualDebugger/SoftBody2DDebugDomain.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaSoftBody2D/Rope.h>
#include <DiaSoftBody2D/Testing/SoftBodyWorldBuilder.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cstring>
#include <memory>

using namespace Dia::SoftBody2D;
using namespace Dia::SoftBody2D::Testing;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;
using namespace Dia::Maths;

namespace
{

// ---------------------------------------------------------------------------
// Fixture: 3-particle rope — SoftParticlesDrawer draws 3 circles.
// ---------------------------------------------------------------------------
struct DomainWorld
{
    std::unique_ptr<SoftBodyWorld> world;

    DomainWorld()
    {
        world.reset(SoftBodyWorldBuilder().WithNoGravity().Build());
        RopeDef def;
        def.id            = Dia::Core::StringCRC("rope");
        def.startPoint    = Vector2D(0.0f, 0.0f);
        def.endPoint      = Vector2D(2.0f, 0.0f);
        def.particleCount = 3;
        def.mass          = 1.0f;
        def.stiffness     = 1.0f;
        def.particleRadius = 0.1f;
        def.startAnchor   = nullptr;
        def.endAnchor     = nullptr;
        world->AddRope(def);
    }
};

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
// Identity (AC-5: description <=80 chars, AC-6: accent from DebugGroupAccents)
// ===========================================================================

TEST(SoftBody2DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    DomainWorld dw;
    SoftBody2DDebugDomain domain(*dw.world);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("SoftBody2D"));
    EXPECT_STREQ(domain.GetDisplayName(), "SoftBody2D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Physics"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(SoftBody2DDebugDomain_Identity, DescriptionWithin80Chars)
{
    DomainWorld dw;
    SoftBody2DDebugDomain domain(*dw.world);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(SoftBody2DDebugDomain_Identity, AccentIsPhysicsGroupConstant)
{
    DomainWorld dw;
    SoftBody2DDebugDomain domain(*dw.world);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kPhysics);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(SoftBody2DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), SoftBody2DDebugDomain::kDrawerCount);
    for (int i = 0; i < SoftBody2DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(domain.GetDrawer(SoftBody2DDebugDomain::kDrawerCount), nullptr);
}

TEST(SoftBody2DDebugDomain_Lifecycle, RegisterAddsAllFourLayers)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), SoftBody2DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kSoftParticles));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kSoftConstraints));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kSoftAnchors));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kSoftVelocity));
}

TEST(SoftBody2DDebugDomain_Lifecycle, LayersCarryTheSoftBody2DStageTag)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("SoftBody2D")));
}

TEST(SoftBody2DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kSoftParticles));
}

TEST(SoftBody2DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), SoftBody2DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(SoftBody2DDebugDomain_DrawerGate, DisabledParticlesDrawerEmitsNoCircles)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    ASSERT_GE(Inspect(enabledFrame).CircleCount(), 1) << "precondition: particles draw when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kSoftParticles);

    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(Inspect(disabledFrame).CircleCount(), 0);
}

TEST(SoftBody2DDebugDomain_DrawerGate, DisablingEveryLayerEmitsNothing)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    for (int i = 0; i < SoftBody2DDebugDomain::kDrawerCount; ++i)
        mgr.DisableLayer(domain.GetDrawer(i)->GetLayerName());

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Inspect(fd).TotalCount(), 0);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(SoftBody2DDebugDomain_Primitives, ParticlesDrawerEmitsCircles)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    // Disable all but the particles drawer so we isolate the type.
    mgr.DisableLayer(Dia::Debug::LayerNames::kSoftConstraints);
    mgr.DisableLayer(Dia::Debug::LayerNames::kSoftAnchors);
    mgr.DisableLayer(Dia::Debug::LayerNames::kSoftVelocity);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.CircleCount(), 3) << "3-particle rope -> 3 circles from particles drawer";
    EXPECT_EQ(v.LineCount(), 0);
}

TEST(SoftBody2DDebugDomain_Primitives, ConstraintsDrawerEmitsLines)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    // Isolate the constraints drawer.
    mgr.DisableLayer(Dia::Debug::LayerNames::kSoftParticles);
    mgr.DisableLayer(Dia::Debug::LayerNames::kSoftAnchors);
    mgr.DisableLayer(Dia::Debug::LayerNames::kSoftVelocity);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_GE(v.LineCount(), 1) << "rope constraints -> at least 1 line";
    EXPECT_EQ(v.CircleCount(), 0);
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(SoftBody2DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(SoftBody2DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
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

TEST(SoftBody2DDebugDomain_JSONState, ReportsEveryDrawerAndAStatsObject)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(SoftBody2DDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());

    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        const Json::Value& entry = state["drawers"][i];
        EXPECT_TRUE(entry.isMember("name"));
        EXPECT_TRUE(entry["name"].isString());
        EXPECT_FALSE(entry["name"].asString().empty());
        ASSERT_TRUE(entry.isMember("enabled"));
        EXPECT_TRUE(entry["enabled"].asBool()) << "drawers start enabled";
    }
}

TEST(SoftBody2DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kSoftParticles);

    Json::Value state;
    domain.GetJSONState(state);
    ASSERT_EQ(state["drawers"].size(), 4u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Particles");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(SoftBody2DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    DomainWorld dw;
    SoftBody2DDebugDomain domain(*dw.world);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 4u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(SoftBody2DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    const Dia::Core::StringCRC particles = Dia::Debug::LayerNames::kSoftParticles;
    ASSERT_TRUE(mgr.IsLayerEnabled(particles));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Particles"));
    EXPECT_FALSE(mgr.IsLayerEnabled(particles));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Particles"));
    EXPECT_TRUE(mgr.IsLayerEnabled(particles));
}

TEST(SoftBody2DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("soft.constraints"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kSoftConstraints));
}

TEST(SoftBody2DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Anchors"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kSoftAnchors));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kSoftParticles));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kSoftConstraints));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kSoftVelocity));
}

TEST(SoftBody2DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < SoftBody2DDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(SoftBody2DDebugDomain_OnCommand, MalformedAndUnknownCommandsAreIgnored)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kSoftParticles));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(SoftBody2DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Particles"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(SoftBody2DDebugDomain_OnCommand, SetScaleUpdatesSharedDebugScale)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(SoftBody2DDebugDomain_OnCommand, SetScaleWithUnknownKeyIsIgnored)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    SoftBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

#endif // DIA_DEBUG
