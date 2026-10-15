////////////////////////////////////////////////////////////////////////////////
// TestIK2DDebugDomain.cpp
// AC-15 mandatory test shapes for the IK2D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (GetDebugScale changes output measurements)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaIK2DVisualDebugger/IK2DDebugDomain.h>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/IKChainDef.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::IK2D;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;

namespace
{

// ---------------------------------------------------------------------------
// Fixture: 3-bone limb skeleton with one IK chain registered.
// IKChainBonesDrawer emits 2 lines for the 3-bone chain.
// ---------------------------------------------------------------------------
struct DomainIK
{
    Dia::Rig2D::SkeletonDef def;
    Dia::Rig2D::Skeleton    skeleton;
    Dia::Rig2D::Pose        pose;
    IKSolver                solver;

    DomainIK()
        : def(Dia::IK2D::Testing::BuildLimbSkeletonDef(3, 1.0f))
        , skeleton(def)
        , pose(skeleton)
        , solver(skeleton, pose)
    {
        Dia::Rig2D::BoneTransform identity;
        solver.SetRootTransform(identity);

        IKChainDef chain;
        chain.id          = Dia::Core::StringCRC("arm");
        chain.startBoneId = Dia::Core::StringCRC("bone0");
        chain.endBoneId   = Dia::Core::StringCRC("bone2");
        solver.RegisterChain(chain);
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

TEST(IK2DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    DomainIK di;
    IK2DDebugDomain domain(di.solver, di.skeleton);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("IK2D"));
    EXPECT_STREQ(domain.GetDisplayName(), "IK2D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Animation"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(IK2DDebugDomain_Identity, DescriptionWithin80Chars)
{
    DomainIK di;
    IK2DDebugDomain domain(di.solver, di.skeleton);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(IK2DDebugDomain_Identity, AccentIsAnimationGroupConstant)
{
    DomainIK di;
    IK2DDebugDomain domain(di.solver, di.skeleton);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAnimation);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(IK2DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), IK2DDebugDomain::kDrawerCount);
    for (int i = 0; i < IK2DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(domain.GetDrawer(IK2DDebugDomain::kDrawerCount), nullptr);
}

TEST(IK2DDebugDomain_Lifecycle, RegisterAddsAllFourLayers)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), IK2DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kIKBones));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kIKJoints));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kIKArrows));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kIKReach));
}

TEST(IK2DDebugDomain_Lifecycle, LayersCarryTheIK2DStageTag)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("IK2D")));
}

TEST(IK2DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kIKBones));
}

TEST(IK2DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), IK2DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(IK2DDebugDomain_DrawerGate, DisabledChainBonesDrawerEmitsNoLines)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kIKJoints);
    mgr.DisableLayer(Dia::Debug::LayerNames::kIKArrows);
    mgr.DisableLayer(Dia::Debug::LayerNames::kIKReach);

    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    ASSERT_GE(Inspect(enabledFrame).LineCount(), 1) << "precondition: chain bones draw when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kIKBones);

    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(Inspect(disabledFrame).LineCount(), 0);
}

TEST(IK2DDebugDomain_DrawerGate, DisablingEveryLayerEmitsNothing)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    for (int i = 0; i < IK2DDebugDomain::kDrawerCount; ++i)
        mgr.DisableLayer(domain.GetDrawer(i)->GetLayerName());

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Inspect(fd).TotalCount(), 0);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(IK2DDebugDomain_Primitives, ChainBonesDrawerEmitsLines)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kIKJoints);
    mgr.DisableLayer(Dia::Debug::LayerNames::kIKArrows);
    mgr.DisableLayer(Dia::Debug::LayerNames::kIKReach);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 2) << "3-bone chain -> 2 chain bone lines";
    EXPECT_EQ(v.CircleCount(), 0);
}

TEST(IK2DDebugDomain_Primitives, ReachCirclesDrawerEmitsCircles)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kIKBones);
    mgr.DisableLayer(Dia::Debug::LayerNames::kIKJoints);
    mgr.DisableLayer(Dia::Debug::LayerNames::kIKArrows);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_GE(v.CircleCount(), 1) << "one chain -> at least 1 reach circle";
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(IK2DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(IK2DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
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

TEST(IK2DDebugDomain_JSONState, ReportsEveryDrawerAndAStatsObject)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(IK2DDebugDomain::kDrawerCount));
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

TEST(IK2DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kIKBones);

    Json::Value state;
    domain.GetJSONState(state);
    ASSERT_EQ(state["drawers"].size(), 4u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "ChainBones");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(IK2DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    DomainIK di;
    IK2DDebugDomain domain(di.solver, di.skeleton);

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

TEST(IK2DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    const Dia::Core::StringCRC bones = Dia::Debug::LayerNames::kIKBones;
    ASSERT_TRUE(mgr.IsLayerEnabled(bones));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ChainBones"));
    EXPECT_FALSE(mgr.IsLayerEnabled(bones));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ChainBones"));
    EXPECT_TRUE(mgr.IsLayerEnabled(bones));
}

TEST(IK2DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ik.joints"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kIKJoints));
}

TEST(IK2DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ReachCircles"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kIKReach));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kIKBones));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kIKJoints));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kIKArrows));
}

TEST(IK2DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < IK2DDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(IK2DDebugDomain_OnCommand, MalformedAndUnknownCommandsAreIgnored)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kIKBones));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(IK2DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ChainBones"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(IK2DDebugDomain_OnCommand, SetScaleUpdatesSharedDebugScale)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(IK2DDebugDomain_OnCommand, SetScaleWithUnknownKeyIsIgnored)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

// ===========================================================================
// Stats fields — GetJSONState() IK chain assertions
// ===========================================================================

TEST(IK2DDebugDomain_JSONState_Stats, ChainCountFieldPresent)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    ASSERT_TRUE(stats.isObject());
    EXPECT_TRUE(stats.isMember("chainCount")) << "stats.chainCount missing";
    EXPECT_TRUE(stats["chainCount"].isInt());
    EXPECT_TRUE(stats.isMember("chains"))     << "stats.chains missing";
    EXPECT_TRUE(stats["chains"].isArray());
    EXPECT_EQ(stats["chains"].size(),
              static_cast<Json::ArrayIndex>(stats["chainCount"].asInt()));
}

TEST(IK2DDebugDomain_JSONState_Stats, DrawerNamesAreChainBonesToReachCircles)
{
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), static_cast<Json::ArrayIndex>(IK2DDebugDomain::kDrawerCount));
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "ChainBones");
    EXPECT_STREQ(state["drawers"][1u]["name"].asCString(), "ChainJoints");
    EXPECT_STREQ(state["drawers"][2u]["name"].asCString(), "ChainArrows");
    EXPECT_STREQ(state["drawers"][3u]["name"].asCString(), "ReachCircles");
}

TEST(IK2DDebugDomain_JSONState_Stats, ChainEntryHasAllSubfields)
{
    // DomainIK registers one chain "arm" — chains[0] must have all 5 sub-fields.
    DomainIK di;
    Dia::Debug::DebugLayerManager mgr;
    IK2DDebugDomain domain(di.solver, di.skeleton);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["stats"]["chainCount"].asInt(), 1);
    const Json::Value& chain0 = state["stats"]["chains"][0u];
    EXPECT_EQ(chain0["index"].asInt(), 0);
    EXPECT_STREQ(chain0["id"].asCString(), "arm");
    EXPECT_TRUE(chain0.isMember("solved"))           << "chains[0].solved missing";
    EXPECT_TRUE(chain0.isMember("iterations"))       << "chains[0].iterations missing";
    EXPECT_TRUE(chain0.isMember("endEffectorError")) << "chains[0].endEffectorError missing";
}

#endif // DIA_DEBUG
