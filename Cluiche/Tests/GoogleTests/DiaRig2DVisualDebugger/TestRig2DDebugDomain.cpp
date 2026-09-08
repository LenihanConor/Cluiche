////////////////////////////////////////////////////////////////////////////////
// TestRig2DDebugDomain.cpp
// AC-15 mandatory test shapes for the Rig2D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (GetDebugScale changes output measurements)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaRig2DVisualDebugger/Rig2DDebugDomain.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cstring>

using namespace Dia::Rig2D;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;
using namespace Dia::Maths;

namespace
{

// ---------------------------------------------------------------------------
// Fixture: 3-bone chain skeleton. BoneLinesDrawer emits 2 lines.
// ---------------------------------------------------------------------------
struct DomainRig
{
    SkeletonDef                                                              def;
    Skeleton                                                                 skeleton;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>           worldTransforms;

    DomainRig()
        : def(Dia::Rig2D::Testing::MakeSimpleChain(3))
        , skeleton(def)
    {
        Pose pose(skeleton);
        pose.SetToBindPose(skeleton);
        const BoneTransform identity{
            Dia::Maths::Vector2D(0.0f, 0.0f), 0.0f, Dia::Maths::Vector2D(1.0f, 1.0f)
        };
        pose.ComputeWorldTransforms(skeleton, identity, worldTransforms);
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
// Identity
// ===========================================================================

TEST(Rig2DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    DomainRig dr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("Rig2D"));
    EXPECT_STREQ(domain.GetDisplayName(), "Rig2D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Animation"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(Rig2DDebugDomain_Identity, DescriptionWithin80Chars)
{
    DomainRig dr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(Rig2DDebugDomain_Identity, AccentIsAnimationGroupConstant)
{
    DomainRig dr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAnimation);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(Rig2DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), Rig2DDebugDomain::kDrawerCount);
    for (int i = 0; i < Rig2DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(domain.GetDrawer(Rig2DDebugDomain::kDrawerCount), nullptr);
}

TEST(Rig2DDebugDomain_Lifecycle, RegisterAddsAllFiveLayers)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Rig2DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kRigBones));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kRigJoints));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kRigArrows));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kRigLabels));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kRigRestPose));
}

TEST(Rig2DDebugDomain_Lifecycle, LayersCarryTheRig2DStageTag)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("Rig2D")));
}

TEST(Rig2DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kRigBones));
}

TEST(Rig2DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Rig2DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(Rig2DDebugDomain_DrawerGate, DisabledBoneLinesDrawerEmitsNoLines)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    // Disable everything except bone lines, verify we get lines.
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigJoints);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigArrows);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigLabels);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigRestPose);

    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    ASSERT_GE(Inspect(enabledFrame).LineCount(), 1) << "precondition: bone lines draw when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kRigBones);

    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(Inspect(disabledFrame).LineCount(), 0);
}

TEST(Rig2DDebugDomain_DrawerGate, DisablingEveryLayerEmitsNothing)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    for (int i = 0; i < Rig2DDebugDomain::kDrawerCount; ++i)
        mgr.DisableLayer(domain.GetDrawer(i)->GetLayerName());

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Inspect(fd).TotalCount(), 0);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(Rig2DDebugDomain_Primitives, BoneLinesDrawerEmitsLines)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kRigJoints);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigArrows);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigLabels);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigRestPose);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 2) << "3-bone chain -> 2 bone lines";
    EXPECT_EQ(v.CircleCount(), 0);
    EXPECT_EQ(v.RayCount(), 0);
}

TEST(Rig2DDebugDomain_Primitives, JointCirclesDrawerEmitsCircles)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kRigBones);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigArrows);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigLabels);
    mgr.DisableLayer(Dia::Debug::LayerNames::kRigRestPose);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.CircleCount(), 3) << "3-bone chain -> 3 joint circles";
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(Rig2DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(Rig2DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
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

TEST(Rig2DDebugDomain_JSONState, ReportsEveryDrawerAndAStatsObject)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(Rig2DDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());

    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        const Json::Value& entry = state["drawers"][i];
        EXPECT_TRUE(entry.isMember("name"));
        EXPECT_FALSE(entry["name"].asString().empty());
        ASSERT_TRUE(entry.isMember("enabled"));
        EXPECT_TRUE(entry["enabled"].asBool()) << "drawers start enabled";
    }
}

TEST(Rig2DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kRigBones);

    Json::Value state;
    domain.GetJSONState(state);
    ASSERT_EQ(state["drawers"].size(), 5u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "BoneLines");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(Rig2DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    DomainRig dr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);

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

TEST(Rig2DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    const Dia::Core::StringCRC bones = Dia::Debug::LayerNames::kRigBones;
    ASSERT_TRUE(mgr.IsLayerEnabled(bones));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("BoneLines"));
    EXPECT_FALSE(mgr.IsLayerEnabled(bones));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("BoneLines"));
    EXPECT_TRUE(mgr.IsLayerEnabled(bones));
}

TEST(Rig2DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("rig.joints"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kRigJoints));
}

TEST(Rig2DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Arrows"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kRigArrows));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kRigBones));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kRigJoints));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kRigLabels));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kRigRestPose));
}

TEST(Rig2DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < Rig2DDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(Rig2DDebugDomain_OnCommand, MalformedAndUnknownCommandsAreIgnored)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kRigBones));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(Rig2DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("BoneLines"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(Rig2DDebugDomain_OnCommand, SetScaleUpdatesSharedDebugScale)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(Rig2DDebugDomain_OnCommand, SetScaleWithUnknownKeyIsIgnored)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

// ===========================================================================
// Stats fields — GetJSONState() bone count assertions
// ===========================================================================

TEST(Rig2DDebugDomain_JSONState_Stats, BoneCountFieldPresent)
{
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    ASSERT_TRUE(stats.isObject());
    EXPECT_TRUE(stats.isMember("boneCount")) << "stats.boneCount missing";
    EXPECT_TRUE(stats["boneCount"].isInt());
}

TEST(Rig2DDebugDomain_JSONState_Stats, BoneCountMatchesThreeBoneSkeleton)
{
    // DomainRig uses MakeSimpleChain(3) — skeleton has 3 bones.
    DomainRig dr;
    Dia::Debug::DebugLayerManager mgr;
    Rig2DDebugDomain domain(dr.skeleton, dr.worldTransforms);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_EQ(state["stats"]["boneCount"].asInt(), 3);
}

#endif // DIA_DEBUG
