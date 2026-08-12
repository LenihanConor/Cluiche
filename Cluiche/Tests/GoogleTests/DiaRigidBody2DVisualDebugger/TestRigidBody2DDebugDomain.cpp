////////////////////////////////////////////////////////////////////////////////
// TestRigidBody2DDebugDomain.cpp
// AC-15 mandatory test shapes for the RigidBody2D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (GetDebugScale changes output measurements)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaRigidBody2DVisualDebugger/RigidBody2DDebugDomain.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Testing/PhysicsWorldBuilder.h>
#include <DiaRigidBody2D/Constraints/PinJoint.h>
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

using namespace Dia::RigidBody2D;
using namespace Dia::RigidBody2D::Testing;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;
using namespace Dia::Maths;

namespace
{

// ---------------------------------------------------------------------------
// Fixture: world with one dynamic (moving) body, one static body, and one
// constraint between them. Exercises every drawer except contacts (which needs
// a simulation step) in a single Draw() pass.
// ---------------------------------------------------------------------------
struct DomainWorld
{
    std::unique_ptr<PhysicsWorld> world;
    std::unique_ptr<BodyFactory>  factory;
    RigidBody2D*                  dynamicBody = nullptr;
    RigidBody2D*                  staticBody  = nullptr;

    DomainWorld()
    {
        world.reset(PhysicsWorldBuilder().WithNoGravity().Build());
        factory.reset(new BodyFactory(*world));
        dynamicBody = factory->MakeDynamic(-1.0f, 0.0f);
        dynamicBody->SetVelocity(Vector2D(2.0f, 0.0f));
        staticBody = factory->MakeStatic(1.0f, 0.0f);
        world->AddConstraint(new PinJoint(dynamicBody, Vector2D::Zero(),
                                          staticBody,  Vector2D::Zero()));
    }
};

RecordingDebugVisitor Inspect(const FrameData& fd)
{
    RecordingDebugVisitor v;
    static_cast<const Dia::Graphics::DebugFrameData&>(fd).AcceptVisitor(v);
    return v;
}

struct PrimitiveCapture : public DebugFrameDataVisitor
{
    static constexpr int kMax = 64;
    mutable DebugPrimitive prims[kMax] = {};
    mutable int            count = 0;

    void Visit(const DebugPrimitive& p) const override
    {
        if (count < kMax) prims[count++] = p;
    }
    void Visit(const DebugFrameData&) const override {}
};

// Length of the single velocity ray in the frame, or -1 if absent.
float FirstRayLength(const FrameData& fd)
{
    PrimitiveCapture cap;
    static_cast<const Dia::Graphics::DebugFrameData&>(fd).AcceptVisitor(cap);
    for (int i = 0; i < cap.count; ++i)
    {
        if (cap.prims[i].type == DebugPrimitiveType::Ray2D)
            return cap.prims[i].ray2D.length;
    }
    return -1.0f;
}

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

} // namespace

// ===========================================================================
// Identity (AC-5: description ≤80 chars, AC-6: accent from DebugGroupAccents)
// ===========================================================================

TEST(RigidBody2DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    DomainWorld dw;
    RigidBody2DDebugDomain domain(*dw.world);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("RigidBody2D"));
    EXPECT_STREQ(domain.GetDisplayName(), "RigidBody2D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Physics"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(RigidBody2DDebugDomain_Identity, DescriptionWithin80Chars)
{
    DomainWorld dw;
    RigidBody2DDebugDomain domain(*dw.world);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(RigidBody2DDebugDomain_Identity, AccentIsPhysicsGroupConstant)
{
    DomainWorld dw;
    RigidBody2DDebugDomain domain(*dw.world);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kPhysics);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(RigidBody2DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), RigidBody2DDebugDomain::kDrawerCount);
    for (int i = 0; i < RigidBody2DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(domain.GetDrawer(RigidBody2DDebugDomain::kDrawerCount), nullptr);
}

TEST(RigidBody2DDebugDomain_Lifecycle, RegisterAddsAllFiveLayers)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), RigidBody2DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kPhysicsShapes));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kPhysicsVelocity));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kPhysicsContacts));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kPhysicsAABB));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kPhysicsConstraints));
}

TEST(RigidBody2DDebugDomain_Lifecycle, LayersCarryTheRigidBody2DStageTag)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("RigidBody2D")));
}

TEST(RigidBody2DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kPhysicsShapes));
}

TEST(RigidBody2DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);
    domain.Register(mgr);   // must not double-register (would DIA_ASSERT)

    EXPECT_EQ(mgr.GetLayerCount(), RigidBody2DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(RigidBody2DDebugDomain_DrawerGate, DisabledShapesDrawerEmitsNoCircles)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    ASSERT_GE(Inspect(enabledFrame).CircleCount(), 1) << "precondition: shapes draw when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kPhysicsShapes);

    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(Inspect(disabledFrame).CircleCount(), 0);
}

TEST(RigidBody2DDebugDomain_DrawerGate, DisablingEveryLayerEmitsNothing)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    for (int i = 0; i < RigidBody2DDebugDomain::kDrawerCount; ++i)
        mgr.DisableLayer(domain.GetDrawer(i)->GetLayerName());

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Inspect(fd).TotalCount(), 0);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(RigidBody2DDebugDomain_Primitives, AllEnabledDrawersEmitExpectedTypes)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);

    EXPECT_EQ(v.CircleCount(), 2) << "shapes: dynamic + static circle";
    EXPECT_EQ(v.RectCount(),   2) << "aabb: one rect per rigid body";
    EXPECT_EQ(v.RayCount(),    1) << "velocity: one arrow on the moving body";
    EXPECT_EQ(v.LineCount(),   1) << "constraints: one anchor-to-anchor line";
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(RigidBody2DDebugDomain_Scale, DoublingDebugScaleDoublesVelocityArrowLength)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    // Isolate the velocity ray — other drawers do not emit Ray2D primitives.
    mgr.SetDebugScale(1.0f);
    FrameData frame1;
    mgr.Draw(frame1);
    const float length1 = FirstRayLength(frame1);
    ASSERT_GT(length1, 0.0f);

    mgr.SetDebugScale(2.0f);
    FrameData frame2;
    mgr.Draw(frame2);
    const float length2 = FirstRayLength(frame2);

    EXPECT_NEAR(length2, length1 * 2.0f, 1e-4f);
}

// ===========================================================================
// AC-15 #4 — GetJSONState round-trip
// ===========================================================================

TEST(RigidBody2DDebugDomain_JSONState, ReportsEveryDrawerAndAStatsObject)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(RigidBody2DDebugDomain::kDrawerCount));
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

TEST(RigidBody2DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kPhysicsShapes);

    Json::Value state;
    domain.GetJSONState(state);
    ASSERT_EQ(state["drawers"].size(), 5u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Shapes");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(RigidBody2DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    DomainWorld dw;
    RigidBody2DDebugDomain domain(*dw.world);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 5u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(RigidBody2DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    const Dia::Core::StringCRC shapes = Dia::Debug::LayerNames::kPhysicsShapes;
    ASSERT_TRUE(mgr.IsLayerEnabled(shapes));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Shapes"));
    EXPECT_FALSE(mgr.IsLayerEnabled(shapes));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Shapes"));
    EXPECT_TRUE(mgr.IsLayerEnabled(shapes));
}

TEST(RigidBody2DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("physics.velocity"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kPhysicsVelocity));
}

TEST(RigidBody2DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("AABB"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kPhysicsAABB));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kPhysicsShapes));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kPhysicsVelocity));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kPhysicsContacts));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kPhysicsConstraints));
}

TEST(RigidBody2DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < RigidBody2DDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(RigidBody2DDebugDomain_OnCommand, MalformedAndUnknownCommandsAreIgnored)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);          // no "drawer"
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kPhysicsShapes));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(RigidBody2DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);

    // No Register() — must not crash or touch the manager.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Shapes"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(RigidBody2DDebugDomain_OnCommand, SetScaleUpdatesSharedDebugScale)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(RigidBody2DDebugDomain_OnCommand, SetScaleWithUnknownKeyIsIgnored)
{
    DomainWorld dw;
    Dia::Debug::DebugLayerManager mgr;
    RigidBody2DDebugDomain domain(*dw.world);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

#endif // DIA_DEBUG
