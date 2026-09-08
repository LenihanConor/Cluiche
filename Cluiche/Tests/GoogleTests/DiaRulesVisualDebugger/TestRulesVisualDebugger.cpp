////////////////////////////////////////////////////////////////////////////////
// TestRulesVisualDebugger.cpp
// Panel-only IDebugDomain for Rules — 20 tests across 4 suites.
// System spec: docs/specs/applications/dia/systems/diarulesvisualdebugger/diarulesvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaRulesVisualDebugger/RulesVisualDebugger.h>
#include <DiaRules/RuleSetComponent.h>
#include <DiaRules/RuleSet.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::Rules;

namespace
{

Json::Value ParseJson(const char* src)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(src, root);
    return root;
}

// 3 rules: attack (fires with health=80), flee (doesn't fire), unnamed (fires with health=80)
const char* kThreeRulesJson = R"({
    "rules": [
        { "id": "attack", "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 }, "actions": ["do_attack"] },
        { "id": "flee",   "guard": { "op": "<",  "slot": "health", "field": "value", "value": 20.0 }, "actions": ["do_flee"] },
        {                 "guard": { "op": ">=", "slot": "health", "field": "value", "value": 60.0 }, "actions": ["do_special"] }
    ]
})";

// health=80 → attack fires, flee doesn't, unnamed fires (2 of 3 rules fired)
static void NoOp(void*) {}

struct RulesTestFixture
{
    RuleActionRegistry registry;
    RuleSetComponent   component;

    explicit RulesTestFixture(const char* json = kThreeRulesJson)
    {
        component.SetRuleSet(RuleSet::LoadFromJson(ParseJson(json)));
        registry.Register(Dia::Core::StringCRC("do_attack"),  NoOp);
        registry.Register(Dia::Core::StringCRC("do_flee"),    NoOp);
        registry.Register(Dia::Core::StringCRC("do_special"), NoOp);
        component.SetRegistry(&registry);
    }

    void Evaluate(float healthValue)
    {
        Dia::Condition::Testing::MockConditionContext ctx;
        ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), healthValue);
        component.Evaluate(ctx, nullptr);
    }
};

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

TEST(RulesVisualDebugger_Identity, DomainIdIsRules)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("rules"));
    EXPECT_STREQ(domain.GetDisplayName(), "Rules");
}

TEST(RulesVisualDebugger_Identity, GroupIsAIBehavior)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("AIBehavior"));
    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

TEST(RulesVisualDebugger_Identity, HasWorldDrawersFalse)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    EXPECT_FALSE(domain.HasWorldDrawers());
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
}

TEST(RulesVisualDebugger_Identity, DescriptionWithin80Chars)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

// ===========================================================================
// JSON state
// ===========================================================================

TEST(RulesVisualDebugger_JSONState, ReportsDrawersAndStats)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 1u);
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());
}

TEST(RulesVisualDebugger_JSONState, DrawerName_IsFireLog_EnabledTrue)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_GE(state["drawers"].size(), 1u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "FireLog");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(RulesVisualDebugger_JSONState, EmptyComponent_ZeroRulesAndFired)
{
    RuleSetComponent emptyComponent;
    RulesVisualDebugger domain(emptyComponent);

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_EQ(state["stats"]["ruleCount"].asInt(), 0);
    EXPECT_EQ(state["stats"]["fired"].asInt(), 0);
    EXPECT_TRUE(state["rules"].isArray());
    EXPECT_EQ(state["rules"].size(), 0u);
}

TEST(RulesVisualDebugger_JSONState, StatsAlwaysPresent_EvenWhenDrawerDisabled)
{
    RulesTestFixture f;
    f.Evaluate(80.0f);
    RulesVisualDebugger domain(f.component);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("FireLog"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE(state.isMember("stats"));
    EXPECT_EQ(state["stats"]["ruleCount"].asInt(), 3);
    EXPECT_EQ(state["stats"]["fired"].asInt(), 2);
    EXPECT_FALSE(state.isMember("rules")) << "rules must be absent when drawer disabled";
}

TEST(RulesVisualDebugger_JSONState, DrawerEnabledFlag_MatchesToggleState)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("FireLog"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

// ===========================================================================
// Rules array content
// ===========================================================================

TEST(RulesVisualDebugger_Rules, FiredRule_HasFiredTrue_WithActions)
{
    RulesTestFixture f;
    f.Evaluate(80.0f);
    RulesVisualDebugger domain(f.component);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state["rules"].isArray());
    ASSERT_GE(state["rules"].size(), 1u);

    // First rule is "attack" — should be fired with do_attack action
    EXPECT_STREQ(state["rules"][0u]["id"].asCString(), "attack");
    EXPECT_TRUE(state["rules"][0u]["fired"].asBool());
    ASSERT_GE(state["rules"][0u]["actions"].size(), 1u);
    EXPECT_STREQ(state["rules"][0u]["actions"][0u].asCString(), "do_attack");
}

TEST(RulesVisualDebugger_Rules, UnfiredRule_HasFiredFalse)
{
    RulesTestFixture f;
    f.Evaluate(80.0f);
    RulesVisualDebugger domain(f.component);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_GE(state["rules"].size(), 2u);
    // Second rule is "flee" — should not fire with health=80
    EXPECT_STREQ(state["rules"][1u]["id"].asCString(), "flee");
    EXPECT_FALSE(state["rules"][1u]["fired"].asBool());
    // Unfired rule still shows its definition-time actions
    EXPECT_GE(state["rules"][1u]["actions"].size(), 1u);
}

TEST(RulesVisualDebugger_Rules, UnnamedRule_EmittedAs_Unnamed)
{
    RulesTestFixture f;
    f.Evaluate(80.0f);
    RulesVisualDebugger domain(f.component);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["rules"].size(), 3u);
    // Third rule has no id — must appear as "(unnamed)"
    EXPECT_STREQ(state["rules"][2u]["id"].asCString(), "(unnamed)");
    EXPECT_TRUE(state["rules"][2u]["fired"].asBool()) << "unnamed rule fires with health=80 (guard >= 60)";
}

TEST(RulesVisualDebugger_Rules, AllRules_PresentInArray)
{
    RulesTestFixture f;
    f.Evaluate(80.0f);
    RulesVisualDebugger domain(f.component);

    Json::Value state;
    domain.GetJSONState(state);

    // All 3 rules must appear regardless of fired status
    EXPECT_EQ(state["rules"].size(), 3u);
}

TEST(RulesVisualDebugger_Rules, Stats_RuleCount_MatchesTotal)
{
    RulesTestFixture f;
    f.Evaluate(80.0f);
    RulesVisualDebugger domain(f.component);

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_EQ(state["stats"]["ruleCount"].asInt(), 3);
    EXPECT_EQ(static_cast<Json::ArrayIndex>(state["stats"]["ruleCount"].asInt()),
              state["rules"].size());
}

TEST(RulesVisualDebugger_Rules, Stats_FiredCount_MatchesFiredRules)
{
    RulesTestFixture f;
    f.Evaluate(80.0f);
    RulesVisualDebugger domain(f.component);

    Json::Value state;
    domain.GetJSONState(state);

    // attack + unnamed fire; flee doesn't
    EXPECT_EQ(state["stats"]["fired"].asInt(), 2);

    // Count manually from the rules array
    int manualFiredCount = 0;
    for (Json::ArrayIndex i = 0; i < state["rules"].size(); ++i)
        if (state["rules"][i]["fired"].asBool()) ++manualFiredCount;

    EXPECT_EQ(state["stats"]["fired"].asInt(), manualFiredCount);
}

// ===========================================================================
// OnCommand
// ===========================================================================

TEST(RulesVisualDebugger_OnCommand, Toggle_FireLog_RemovesRulesKey)
{
    RulesTestFixture f;
    f.Evaluate(80.0f);
    RulesVisualDebugger domain(f.component);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("FireLog"));

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_FALSE(state.isMember("rules")) << "rules key must be absent when FireLog disabled";
}

TEST(RulesVisualDebugger_OnCommand, Toggle_FireLog_Twice_Restores)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("FireLog"));
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("FireLog"));

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_TRUE(state.isMember("rules"));
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(RulesVisualDebugger_OnCommand, SetScale_NoOp_NoCrash)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("setScale"), args));
}

TEST(RulesVisualDebugger_OnCommand, UnknownCommand_NoOp)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(RulesVisualDebugger_OnCommand, MalformedToggle_NoOp)
{
    RulesTestFixture f;
    RulesVisualDebugger domain(f.component);

    // No drawer key
    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);

    // Wrong type for drawer
    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool()) << "drawer must remain enabled after malformed toggle";
}

#endif // DIA_DEBUG
