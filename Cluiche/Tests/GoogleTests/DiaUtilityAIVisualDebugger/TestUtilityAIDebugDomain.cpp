////////////////////////////////////////////////////////////////////////////////
// TestUtilityAIDebugDomain.cpp
// AC-15 mandatory test shapes for the UtilityAI IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (GetDebugScale changes output measurements)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
//
// Note: UtilityScoreDrawer is ImGui-only — Draw() is a no-op.
//       Tests 1 and 2 therefore verify 0 world primitives are emitted.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaUtilityAIVisualDebugger/UtilityAIDebugDomain.h>

#include <DiaUtilityAI/UtilitySet.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::UtilityAI;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;

namespace
{

// ---------------------------------------------------------------------------
// Fixture: empty UtilitySet — UtilityScoreDrawer.Draw() is a no-op.
// The domain takes const UtilitySet&, so we hold one instance.
// ---------------------------------------------------------------------------
struct DomainUtilityAI
{
    Dia::UtilityAI::UtilitySet utilitySet;
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

TEST(UtilityAIDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    DomainUtilityAI du;
    UtilityAIDebugDomain domain(du.utilitySet);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("UtilityAI"));
    EXPECT_STREQ(domain.GetDisplayName(), "UtilityAI");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("AIBehavior"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(UtilityAIDebugDomain_Identity, DescriptionWithin80Chars)
{
    DomainUtilityAI du;
    UtilityAIDebugDomain domain(du.utilitySet);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(UtilityAIDebugDomain_Identity, AccentIsAIBehaviorGroupConstant)
{
    DomainUtilityAI du;
    UtilityAIDebugDomain domain(du.utilitySet);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(UtilityAIDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), UtilityAIDebugDomain::kDrawerCount);
    for (int i = 0; i < UtilityAIDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(domain.GetDrawer(UtilityAIDebugDomain::kDrawerCount), nullptr);
}

TEST(UtilityAIDebugDomain_Lifecycle, RegisterAddsScoreTableLayer)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), UtilityAIDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kUtilityAIScores));
}

TEST(UtilityAIDebugDomain_Lifecycle, LayersCarryTheUtilityAIStageTag)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("UtilityAI")));
}

TEST(UtilityAIDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kUtilityAIScores));
}

TEST(UtilityAIDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), UtilityAIDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// (UtilityScoreDrawer is ImGui-only: both enabled and disabled yield 0 world
//  primitives. The test confirms the gate does not crash and emits nothing.)
// ===========================================================================

TEST(UtilityAIDebugDomain_DrawerGate, EnabledDrawerEmitsNoWorldPrimitives)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    EXPECT_EQ(Inspect(enabledFrame).TotalCount(), 0)
        << "UtilityScoreDrawer is ImGui-only: no world primitives even when enabled";
}

TEST(UtilityAIDebugDomain_DrawerGate, DisabledDrawerEmitsNoWorldPrimitives)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kUtilityAIScores);

    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(Inspect(disabledFrame).TotalCount(), 0);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// (ImGui-only: expected type count is 0 for all world-space primitive types.)
// ===========================================================================

TEST(UtilityAIDebugDomain_Primitives, ScoreTableDrawerEmitsNoWorldPrimitives)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);

    EXPECT_EQ(v.CircleCount(), 0) << "UtilityScoreDrawer has no world-space circles";
    EXPECT_EQ(v.LineCount(),   0) << "UtilityScoreDrawer has no world-space lines";
    EXPECT_EQ(v.RectCount(),   0) << "UtilityScoreDrawer has no world-space rects";
    EXPECT_EQ(v.RayCount(),    0) << "UtilityScoreDrawer has no world-space rays";
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// (No geometric output to measure; verify setScale is forwarded without crash.)
// ===========================================================================

TEST(UtilityAIDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(UtilityAIDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
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

TEST(UtilityAIDebugDomain_JSONState, ReportsScoreTableDrawerAndStatsObject)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(UtilityAIDebugDomain::kDrawerCount));
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

TEST(UtilityAIDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kUtilityAIScores);

    Json::Value state;
    domain.GetJSONState(state);
    ASSERT_EQ(state["drawers"].size(), 1u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "ScoreTable");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

TEST(UtilityAIDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    DomainUtilityAI du;
    UtilityAIDebugDomain domain(du.utilitySet);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 1u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(UtilityAIDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    const Dia::Core::StringCRC scores = Dia::Debug::LayerNames::kUtilityAIScores;
    ASSERT_TRUE(mgr.IsLayerEnabled(scores));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ScoreTable"));
    EXPECT_FALSE(mgr.IsLayerEnabled(scores));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ScoreTable"));
    EXPECT_TRUE(mgr.IsLayerEnabled(scores));
}

TEST(UtilityAIDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("utility_ai.scores"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kUtilityAIScores));
}

TEST(UtilityAIDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < UtilityAIDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(UtilityAIDebugDomain_OnCommand, MalformedAndUnknownCommandsAreIgnored)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kUtilityAIScores));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(UtilityAIDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ScoreTable"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(UtilityAIDebugDomain_OnCommand, SetScaleUpdatesSharedDebugScale)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(UtilityAIDebugDomain_OnCommand, SetScaleWithUnknownKeyIsIgnored)
{
    DomainUtilityAI du;
    Dia::Debug::DebugLayerManager mgr;
    UtilityAIDebugDomain domain(du.utilitySet);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

#endif // DIA_DEBUG
