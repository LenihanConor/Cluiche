////////////////////////////////////////////////////////////////////////////////
// TestUtilityAIDebugDomain.cpp
// Panel-only IDebugDomain for Utility AI — 20 tests across 4 suites.
// System spec: docs/specs/applications/dia/systems/diautilityaivisualdebugger/diautilityaivisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaUtilityAIVisualDebugger/UtilityAIDebugDomain.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::UtilityAI;

namespace
{

Json::Value ParseJson(const char* src)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(src, root);
    return root;
}

// Two actions with linear scorers reading health.value.
// With health=80: Attack=0.8, Flee=0.2 (Attack wins).
const char* kTwoActionsJson = R"({
    "actions": [
        {
            "id": "Flee",
            "scorers": [
                { "slot": "health", "field": "value", "input_min": 0.0, "input_max": 100.0, "curve": { "shape": "linear", "invert": true } }
            ]
        },
        {
            "id": "Attack",
            "scorers": [
                { "slot": "health", "field": "value", "input_min": 0.0, "input_max": 100.0, "curve": { "shape": "linear" } }
            ]
        }
    ]
})";

// Populate a UtilitySet's last-frame scores: Attack=0.8, Flee=0.2.
void PopulateScores(UtilitySet& set)
{
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 80.0f);
    Dia::Rules::RuleActionRegistry registry;
    set.Evaluate(ctx, registry, nullptr, nullptr);
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

TEST(UtilityAIDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("UtilityAI"));
    EXPECT_STREQ(domain.GetDisplayName(), "UtilityAI");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("AIBehavior"));
    EXPECT_FALSE(domain.HasWorldDrawers());
}

TEST(UtilityAIDebugDomain_Identity, DescriptionWithin80Chars)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(UtilityAIDebugDomain_Identity, AccentIsAIBehaviorConstant)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

TEST(UtilityAIDebugDomain_Identity, GetDrawerCount_ReturnsZero)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
}

// ===========================================================================
// JSON state
// ===========================================================================

TEST(UtilityAIDebugDomain_JSONState, ReportsDrawerAndStats)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 1u);
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());
    EXPECT_TRUE(state.isMember("actions"));
}

TEST(UtilityAIDebugDomain_JSONState, DrawerName_IsScores)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_GE(state["drawers"].size(), 1u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Scores");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(UtilityAIDebugDomain_JSONState, EmptyUtilitySet_ZeroActionsAndWinnerScore)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_EQ(state["stats"]["actionCount"].asInt(), 0);
    EXPECT_FLOAT_EQ(state["stats"]["winnerScore"].asFloat(), 0.0f);
    EXPECT_TRUE(state["actions"].isArray());
    EXPECT_EQ(state["actions"].size(), 0u);
}

TEST(UtilityAIDebugDomain_JSONState, ActionsAtTopLevel_NotInsideStats)
{
    UtilitySet set = UtilitySet::LoadFromJson(ParseJson(kTwoActionsJson));
    PopulateScores(set);
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    // actions must be top-level, not nested under stats
    EXPECT_TRUE(state.isMember("actions"))           << "actions must be top-level";
    EXPECT_FALSE(state["stats"].isMember("actions")) << "actions must NOT be in stats";
    EXPECT_EQ(state["actions"].size(), 2u);
}

TEST(UtilityAIDebugDomain_JSONState, StatsAlwaysPresent_EvenWhenDrawerDisabled)
{
    UtilitySet set = UtilitySet::LoadFromJson(ParseJson(kTwoActionsJson));
    PopulateScores(set);
    UtilityAIDebugDomain domain(set);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Scores"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE(state.isMember("stats"));
    EXPECT_EQ(state["stats"]["actionCount"].asInt(), 2);
    EXPECT_GT(state["stats"]["winnerScore"].asFloat(), 0.0f);
    EXPECT_FALSE(state.isMember("actions")) << "actions must be absent when disabled";
}

TEST(UtilityAIDebugDomain_JSONState, DrawerEnabledFlag_MatchesToggleState)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Scores"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

TEST(UtilityAIDebugDomain_JSONState, BeforeEvaluate_EmptyActionsArray)
{
    UtilitySet set = UtilitySet::LoadFromJson(ParseJson(kTwoActionsJson));
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    // No Evaluate() called — GetLastFrameScores returns nothing
    EXPECT_EQ(state["stats"]["actionCount"].asInt(), 0);
    EXPECT_EQ(state["actions"].size(), 0u);
}

// ===========================================================================
// Actions array content
// ===========================================================================

TEST(UtilityAIDebugDomain_Actions, SortedDescendingByScore)
{
    UtilitySet set = UtilitySet::LoadFromJson(ParseJson(kTwoActionsJson));
    PopulateScores(set);
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_GE(state["actions"].size(), 2u);
    EXPECT_GE(state["actions"][0u]["score"].asFloat(),
              state["actions"][1u]["score"].asFloat())
        << "actions must be sorted descending by score";
}

TEST(UtilityAIDebugDomain_Actions, WinnerMarkedOnTopEntry)
{
    UtilitySet set = UtilitySet::LoadFromJson(ParseJson(kTwoActionsJson));
    PopulateScores(set);
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_GE(state["actions"].size(), 1u);
    EXPECT_TRUE(state["actions"][0u]["winner"].asBool())    << "first action must have winner=true";
    EXPECT_FALSE(state["actions"][1u]["winner"].asBool())   << "second action must have winner=false";
}

TEST(UtilityAIDebugDomain_Actions, WinnerScoreMatchesTopAction)
{
    UtilitySet set = UtilitySet::LoadFromJson(ParseJson(kTwoActionsJson));
    PopulateScores(set);
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_GE(state["actions"].size(), 1u);
    EXPECT_FLOAT_EQ(state["stats"]["winnerScore"].asFloat(),
                    state["actions"][0u]["score"].asFloat());
}

TEST(UtilityAIDebugDomain_Actions, ActionCountMatchesArraySize)
{
    UtilitySet set = UtilitySet::LoadFromJson(ParseJson(kTwoActionsJson));
    PopulateScores(set);
    UtilityAIDebugDomain domain(set);

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_EQ(static_cast<Json::ArrayIndex>(state["stats"]["actionCount"].asInt()),
              state["actions"].size());
}

// ===========================================================================
// OnCommand
// ===========================================================================

TEST(UtilityAIDebugDomain_OnCommand, Toggle_Scores_RemovesActionsKey)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Scores"));

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_FALSE(state.isMember("actions"));
}

TEST(UtilityAIDebugDomain_OnCommand, Toggle_Scores_Twice_Restores)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Scores"));
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Scores"));

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_TRUE(state.isMember("actions"));
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(UtilityAIDebugDomain_OnCommand, SetScale_NoOp_NoCrash)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("setScale"), args));
}

TEST(UtilityAIDebugDomain_OnCommand, UnknownCommand_NoOp)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(UtilityAIDebugDomain_OnCommand, MalformedToggle_NoOp)
{
    UtilitySet set;
    UtilityAIDebugDomain domain(set);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool()) << "drawer must remain enabled after malformed toggle";
}

#endif // DIA_DEBUG
