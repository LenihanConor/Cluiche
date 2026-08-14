////////////////////////////////////////////////////////////////////////////////
// TestAnimation2DDebugDomain.cpp
// AC-15 mandatory test shapes for the Animation2D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (GetDebugScale changes output measurements)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaAnimation2DVisualDebugger/Animation2DDebugDomain.h>

#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
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

using namespace Dia::Animation2D;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;
using namespace Dia::Maths;

namespace
{

// ---------------------------------------------------------------------------
// Fixture: 3-bone skeleton with one registered clip player so that
// AnimBlendWeightsDrawer emits at least one text label.
// ---------------------------------------------------------------------------
struct DomainAnim
{
    Dia::Rig2D::SkeletonDef                                                        def;
    Dia::Rig2D::Skeleton                                                           skeleton;
    Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>           worldTransforms;
    std::unique_ptr<Dia::Animation2D::AnimationEvaluator>                          evaluator;

    DomainAnim()
        : def(Dia::Rig2D::Testing::MakeSimpleChain(3))
        , skeleton(def)
    {
        Dia::Rig2D::Pose pose(skeleton);
        pose.SetToBindPose(skeleton);
        const Dia::Rig2D::BoneTransform identity{
            Dia::Maths::Vector2D(0.0f, 0.0f), 0.0f, Dia::Maths::Vector2D(1.0f, 1.0f)
        };
        pose.ComputeWorldTransforms(skeleton, identity, worldTransforms);

        evaluator = std::make_unique<Dia::Animation2D::AnimationEvaluator>(skeleton);
        evaluator->RegisterClipPlayer(Dia::Core::StringCRC("test_clip"), 0);
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

TEST(Animation2DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    DomainAnim da;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("Animation2D"));
    EXPECT_STREQ(domain.GetDisplayName(), "Animation2D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Animation"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(Animation2DDebugDomain_Identity, DescriptionWithin80Chars)
{
    DomainAnim da;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(Animation2DDebugDomain_Identity, AccentIsAnimationGroupConstant)
{
    DomainAnim da;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAnimation);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(Animation2DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), Animation2DDebugDomain::kDrawerCount);
    for (int i = 0; i < Animation2DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(domain.GetDrawer(Animation2DDebugDomain::kDrawerCount), nullptr);
}

TEST(Animation2DDebugDomain_Lifecycle, RegisterAddsAllThreeLayers)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Animation2DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kAnimBlendWeights));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kAnimClipCursor));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kAnimSpring));
}

TEST(Animation2DDebugDomain_Lifecycle, LayersCarryTheAnimation2DStageTag)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("Animation2D")));
}

TEST(Animation2DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kAnimBlendWeights));
}

TEST(Animation2DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Animation2DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(Animation2DDebugDomain_DrawerGate, DisabledBlendWeightsDrawerEmitsNoText)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    // Disable all but blend weights, then disable blend weights too.
    mgr.DisableLayer(Dia::Debug::LayerNames::kAnimClipCursor);
    mgr.DisableLayer(Dia::Debug::LayerNames::kAnimSpring);

    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    // With one clip player registered, blend weights should emit at least 1 text label.
    ASSERT_GE(Inspect(enabledFrame).TextCount(), 1) << "precondition: blend weights draws when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kAnimBlendWeights);

    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(Inspect(disabledFrame).TextCount(), 0);
}

TEST(Animation2DDebugDomain_DrawerGate, DisablingEveryLayerEmitsNothing)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    for (int i = 0; i < Animation2DDebugDomain::kDrawerCount; ++i)
        mgr.DisableLayer(domain.GetDrawer(i)->GetLayerName());

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(Inspect(fd).TotalCount(), 0);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(Animation2DDebugDomain_Primitives, BlendWeightsDrawerEmitsTextLabels)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kAnimClipCursor);
    mgr.DisableLayer(Dia::Debug::LayerNames::kAnimSpring);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_GE(v.TextCount(), 1) << "one registered source -> at least 1 text label";
    EXPECT_EQ(v.CircleCount(), 0);
    EXPECT_EQ(v.LineCount(), 0);
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(Animation2DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(Animation2DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
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

TEST(Animation2DDebugDomain_JSONState, ReportsEveryDrawerAndAStatsObject)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(Animation2DDebugDomain::kDrawerCount));
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

TEST(Animation2DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kAnimBlendWeights);

    Json::Value state;
    domain.GetJSONState(state);
    ASSERT_EQ(state["drawers"].size(), 3u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "BlendWeights");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(Animation2DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    DomainAnim da;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 3u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(Animation2DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    const Dia::Core::StringCRC blendWeights = Dia::Debug::LayerNames::kAnimBlendWeights;
    ASSERT_TRUE(mgr.IsLayerEnabled(blendWeights));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("BlendWeights"));
    EXPECT_FALSE(mgr.IsLayerEnabled(blendWeights));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("BlendWeights"));
    EXPECT_TRUE(mgr.IsLayerEnabled(blendWeights));
}

TEST(Animation2DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("anim.clip_cursor"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kAnimClipCursor));
}

TEST(Animation2DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Spring"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kAnimSpring));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kAnimBlendWeights));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kAnimClipCursor));
}

TEST(Animation2DDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < Animation2DDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(Animation2DDebugDomain_OnCommand, MalformedAndUnknownCommandsAreIgnored)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kAnimBlendWeights));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(Animation2DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("BlendWeights"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(Animation2DDebugDomain_OnCommand, SetScaleUpdatesSharedDebugScale)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(Animation2DDebugDomain_OnCommand, SetScaleWithUnknownKeyIsIgnored)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

// ===========================================================================
// Stats fields — GetJSONState() animation state assertions
// ===========================================================================

TEST(Animation2DDebugDomain_JSONState_Stats, AnimationStatFieldsPresent)
{
    DomainAnim da;
    Dia::Debug::DebugLayerManager mgr;
    Animation2DDebugDomain domain(*da.evaluator, da.skeleton, da.worldTransforms);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    ASSERT_TRUE(stats.isObject());
    EXPECT_TRUE(stats.isMember("layerCount"))     << "stats.layerCount missing";
    EXPECT_TRUE(stats["layerCount"].isInt());
    EXPECT_TRUE(stats.isMember("activeClip"))     << "stats.activeClip missing";
    EXPECT_TRUE(stats["activeClip"].isString());
    EXPECT_TRUE(stats.isMember("normalizedTime")) << "stats.normalizedTime missing";
    EXPECT_TRUE(stats["normalizedTime"].isNumeric());
    EXPECT_TRUE(stats.isMember("layers"))         << "stats.layers missing";
    EXPECT_TRUE(stats["layers"].isArray());
    EXPECT_TRUE(stats.isMember("springs"))        << "stats.springs missing";
    EXPECT_TRUE(stats["springs"].isObject());
    EXPECT_TRUE(stats["springs"].isMember("chainCount"))         << "stats.springs.chainCount missing";
    EXPECT_TRUE(stats["springs"].isMember("maxAngularVelocity")) << "stats.springs.maxAngularVelocity missing";
}

#endif // DIA_DEBUG
