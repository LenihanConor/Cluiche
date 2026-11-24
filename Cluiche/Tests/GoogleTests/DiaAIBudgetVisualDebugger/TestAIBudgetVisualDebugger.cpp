////////////////////////////////////////////////////////////////////////////////
// TestAIBudgetVisualDebugger.cpp
// Tests for AIBudgetVisualDebugger IDebugDomain — mandatory AC-15 shapes:
//   - Identity:         HasWorldDrawers returns false; GetDrawerCount returns 0
//   - DrawerGate:       disabling BudgetBar removes stats.fillPct from JSON
//   - JSONRoundTrip:    drawers array entries match enabled state
//   - ScaleSensitivity: OnCommand("setScale", ...) is no-op, does not crash
//   - OnCommandRoundTrip: toggle BudgetBar off then on; fillPct absent then present
//
// System spec: docs/specs/applications/dia/systems/diaaibudgetvisualdebugger/diaaibudgetvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaAIBudgetVisualDebugger/AIBudgetVisualDebugger.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <string>

using Dia::AIBudget::AIBudgetScheduler;
using Dia::AIBudget::AIBudgetResult;
using Dia::AIBudget::AIBudgetVisualDebugger;
using Dia::Core::StringCRC;

// ===========================================================================
// Helpers
// ===========================================================================

namespace
{

Json::Value GetState(AIBudgetVisualDebugger& d)
{
    Json::Value out;
    d.GetJSONState(out);
    return out;
}

} // namespace

// ===========================================================================
// Suite: AIBudgetVisualDebugger_Identity
// ===========================================================================

TEST(AIBudgetVisualDebugger_Identity, PrimitiveType_NoWorldDrawers)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 2.5f;

    AIBudgetVisualDebugger debugger(scheduler, result);

    EXPECT_FALSE(debugger.HasWorldDrawers());
    EXPECT_EQ(debugger.GetDrawerCount(), 0);
}

// ===========================================================================
// Suite: AIBudgetVisualDebugger_JSONState
// ===========================================================================

TEST(AIBudgetVisualDebugger_JSONState, DrawerGate_BudgetBarOff_RemovesFillPct)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 2.5f;

    AIBudgetVisualDebugger debugger(scheduler, result);

    // Toggle BudgetBar off
    Json::Value toggleArgs(Json::objectValue);
    toggleArgs["drawer"] = "BudgetBar";
    debugger.OnCommand(StringCRC("toggle"), toggleArgs);

    Json::Value state = GetState(debugger);
    EXPECT_TRUE(state["stats"].isMember("usedMs"));    // stats still present
    EXPECT_FALSE(state["stats"].isMember("fillPct"));  // fillPct removed
}

TEST(AIBudgetVisualDebugger_JSONState, JSONRoundTrip_DrawerEntriesMatchEnabledState)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 2.5f;

    AIBudgetVisualDebugger debugger(scheduler, result);

    Json::Value state = GetState(debugger);
    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_EQ(state["drawers"].size(), 2u);

    // Both drawers enabled by default
    bool foundBudgetBar = false, foundSystemTimings = false;
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        const auto& d = state["drawers"][i];
        if (std::string(d["name"].asCString()) == "BudgetBar")
        {
            foundBudgetBar = true;
            EXPECT_TRUE(d["enabled"].asBool());
        }
        if (std::string(d["name"].asCString()) == "SystemTimings")
        {
            foundSystemTimings = true;
            EXPECT_TRUE(d["enabled"].asBool());
        }
    }
    EXPECT_TRUE(foundBudgetBar);
    EXPECT_TRUE(foundSystemTimings);
}

// ===========================================================================
// Suite: AIBudgetVisualDebugger_OnCommand
// ===========================================================================

TEST(AIBudgetVisualDebugger_OnCommand, ScaleSensitivity_SetScaleNoOp_NoCrash)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 0;
    result.systemsDeferred = 0;
    result.usedMs          = 0.0f;

    AIBudgetVisualDebugger debugger(scheduler, result);

    Json::Value scaleArgs(Json::objectValue);
    scaleArgs["key"]   = "someScale";
    scaleArgs["value"] = 1.5f;
    EXPECT_NO_FATAL_FAILURE(debugger.OnCommand(StringCRC("setScale"), scaleArgs));
}

TEST(AIBudgetVisualDebugger_OnCommand, OnCommandRoundTrip_ToggleBudgetBarOffThenOn)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 2.5f;

    AIBudgetVisualDebugger debugger(scheduler, result);

    Json::Value toggleArgs(Json::objectValue);
    toggleArgs["drawer"] = "BudgetBar";

    // First toggle: disable
    debugger.OnCommand(StringCRC("toggle"), toggleArgs);
    Json::Value stateOff = GetState(debugger);
    EXPECT_FALSE(stateOff["stats"].isMember("fillPct"));

    // Second toggle: re-enable
    debugger.OnCommand(StringCRC("toggle"), toggleArgs);
    Json::Value stateOn = GetState(debugger);
    EXPECT_TRUE(stateOn["stats"].isMember("fillPct"));
}

// ===========================================================================
// Suite: AIBudgetVisualDebugger_Stats
// ===========================================================================

TEST(AIBudgetVisualDebugger_Stats, Stats_UsedMs_MatchesResult)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 3;
    result.systemsDeferred = 1;
    result.usedMs          = 2.1f;

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

    EXPECT_FLOAT_EQ(state["stats"]["usedMs"].asFloat(), result.usedMs);
}

TEST(AIBudgetVisualDebugger_Stats, Stats_BudgetMs_MatchesScheduler)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 3;
    result.systemsDeferred = 1;
    result.usedMs          = 2.1f;

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

    EXPECT_FLOAT_EQ(state["stats"]["budgetMs"].asFloat(), scheduler.GetLastBudgetMs());
}

TEST(AIBudgetVisualDebugger_Stats, Stats_FillPct_Computed)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);   // GetLastBudgetMs() returns 5.0f

    AIBudgetResult result{};
    result.systemsRun      = 3;
    result.systemsDeferred = 1;
    result.usedMs          = 2.1f;

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

    // 2.1 / 5.0 * 100 + 0.5 = 42.5 → static_cast<int> = 42
    EXPECT_NEAR(state["stats"]["fillPct"].asInt(), 42, 1);
}

TEST(AIBudgetVisualDebugger_Stats, Stats_FillPct_ClampedAt100)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 1;
    result.systemsDeferred = 0;
    result.usedMs          = 7.0f;   // exceeds budget

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

    EXPECT_EQ(state["stats"]["fillPct"].asInt(), 100);
}

TEST(AIBudgetVisualDebugger_Stats, Stats_SystemsRun_Matches)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 3;
    result.systemsDeferred = 1;
    result.usedMs          = 2.1f;

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

    EXPECT_EQ(state["stats"]["systemsRun"].asInt(), result.systemsRun);
}

TEST(AIBudgetVisualDebugger_Stats, Stats_SystemsDeferred_Matches)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 3;
    result.systemsDeferred = 1;
    result.usedMs          = 2.1f;

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

    EXPECT_EQ(state["stats"]["systemsDeferred"].asInt(), result.systemsDeferred);
}

TEST(AIBudgetVisualDebugger_Stats, BudgetZero_NoAssert)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(0.0f);   // budgetMs = 0 → fillPct guard fires → fillPct = 0

    AIBudgetResult result{};
    result.systemsRun      = 0;
    result.systemsDeferred = 0;
    result.usedMs          = 0.0f;

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state;
    EXPECT_NO_FATAL_FAILURE(state = GetState(debugger));
    EXPECT_EQ(state["stats"]["fillPct"].asInt(), 0);
}

// ===========================================================================
// Suite: AIBudgetVisualDebugger_Systems
// ===========================================================================

TEST(AIBudgetVisualDebugger_Systems, Systems_CountMatchesPerSystem)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 1.7f;

#ifdef DIA_DEBUG
    {
        Dia::AIBudget::SystemTimingEntry e1;
        e1.systemId = StringCRC("UtilityAI");
        e1.timeMs   = 0.8f;
        e1.ran      = true;
        result.perSystem.Add(e1);

        Dia::AIBudget::SystemTimingEntry e2;
        e2.systemId = StringCRC("HTNPlanner");
        e2.timeMs   = 0.9f;
        e2.ran      = true;
        result.perSystem.Add(e2);

        Dia::AIBudget::SystemTimingEntry e3;
        e3.systemId = StringCRC("StateMachine");
        e3.timeMs   = 0.0f;
        e3.ran      = false;
        result.perSystem.Add(e3);
    }
#endif

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

#ifdef DIA_DEBUG
    ASSERT_TRUE(state.isMember("systems"));
    EXPECT_EQ(state["systems"].size(), static_cast<unsigned int>(result.perSystem.Size()));
#endif
}

TEST(AIBudgetVisualDebugger_Systems, Systems_RanFlag_Accurate)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 1.7f;

#ifdef DIA_DEBUG
    {
        Dia::AIBudget::SystemTimingEntry e1;
        e1.systemId = StringCRC("UtilityAI");
        e1.timeMs   = 0.8f;
        e1.ran      = true;
        result.perSystem.Add(e1);

        Dia::AIBudget::SystemTimingEntry e2;
        e2.systemId = StringCRC("HTNPlanner");
        e2.timeMs   = 0.9f;
        e2.ran      = true;
        result.perSystem.Add(e2);

        Dia::AIBudget::SystemTimingEntry e3;
        e3.systemId = StringCRC("StateMachine");
        e3.timeMs   = 0.0f;
        e3.ran      = false;
        result.perSystem.Add(e3);
    }
#endif

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

#ifdef DIA_DEBUG
    ASSERT_TRUE(state.isMember("systems"));
    ASSERT_EQ(state["systems"].size(), 3u);
    EXPECT_TRUE(state["systems"][0]["ran"].asBool());
    EXPECT_TRUE(state["systems"][1]["ran"].asBool());
    EXPECT_FALSE(state["systems"][2]["ran"].asBool());
#endif
}

TEST(AIBudgetVisualDebugger_Systems, Systems_TimeMs_Accurate)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 1.7f;

#ifdef DIA_DEBUG
    {
        Dia::AIBudget::SystemTimingEntry e1;
        e1.systemId = StringCRC("UtilityAI");
        e1.timeMs   = 0.8f;
        e1.ran      = true;
        result.perSystem.Add(e1);

        Dia::AIBudget::SystemTimingEntry e2;
        e2.systemId = StringCRC("HTNPlanner");
        e2.timeMs   = 0.9f;
        e2.ran      = true;
        result.perSystem.Add(e2);

        Dia::AIBudget::SystemTimingEntry e3;
        e3.systemId = StringCRC("StateMachine");
        e3.timeMs   = 0.0f;
        e3.ran      = false;
        result.perSystem.Add(e3);
    }
#endif

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state = GetState(debugger);

#ifdef DIA_DEBUG
    ASSERT_TRUE(state.isMember("systems"));
    ASSERT_EQ(state["systems"].size(), 3u);
    EXPECT_NEAR(state["systems"][0]["timeMs"].asFloat(), 0.8f, 0.001f);
    EXPECT_NEAR(state["systems"][1]["timeMs"].asFloat(), 0.9f, 0.001f);
    EXPECT_NEAR(state["systems"][2]["timeMs"].asFloat(), 0.0f, 0.001f);
#endif
}

TEST(AIBudgetVisualDebugger_Systems, DrawerGate_SystemTimings)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 1.7f;

#ifdef DIA_DEBUG
    {
        Dia::AIBudget::SystemTimingEntry e1;
        e1.systemId = StringCRC("UtilityAI");
        e1.timeMs   = 0.8f;
        e1.ran      = true;
        result.perSystem.Add(e1);
    }
#endif

    AIBudgetVisualDebugger debugger(scheduler, result);

    // Toggle SystemTimings off
    Json::Value toggleArgs(Json::objectValue);
    toggleArgs["drawer"] = "SystemTimings";
    debugger.OnCommand(StringCRC("toggle"), toggleArgs);

    Json::Value state = GetState(debugger);

#ifdef DIA_DEBUG
    EXPECT_FALSE(state.isMember("systems"));
#endif
}

TEST(AIBudgetVisualDebugger_Systems, Toggle_SystemTimings)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 2;
    result.systemsDeferred = 1;
    result.usedMs          = 1.7f;

#ifdef DIA_DEBUG
    {
        Dia::AIBudget::SystemTimingEntry e1;
        e1.systemId = StringCRC("UtilityAI");
        e1.timeMs   = 0.8f;
        e1.ran      = true;
        result.perSystem.Add(e1);
    }
#endif

    AIBudgetVisualDebugger debugger(scheduler, result);

    Json::Value toggleArgs(Json::objectValue);
    toggleArgs["drawer"] = "SystemTimings";

    // First toggle: disable
    debugger.OnCommand(StringCRC("toggle"), toggleArgs);
    Json::Value stateOff = GetState(debugger);

    // Second toggle: re-enable
    debugger.OnCommand(StringCRC("toggle"), toggleArgs);
    Json::Value stateOn = GetState(debugger);

#ifdef DIA_DEBUG
    EXPECT_FALSE(stateOff.isMember("systems"));
    EXPECT_TRUE(stateOn.isMember("systems"));
#endif
}

TEST(AIBudgetVisualDebugger_Systems, NoSystems_EmptyArray_NoAssert)
{
    AIBudgetScheduler scheduler;
    scheduler.Update(5.0f);

    AIBudgetResult result{};
    result.systemsRun      = 0;
    result.systemsDeferred = 0;
    result.usedMs          = 0.0f;
    // perSystem left empty

    AIBudgetVisualDebugger debugger(scheduler, result);
    Json::Value state;
    EXPECT_NO_FATAL_FAILURE(state = GetState(debugger));

#ifdef DIA_DEBUG
    ASSERT_TRUE(state.isMember("systems"));
    EXPECT_EQ(state["systems"].size(), 0u);
#endif
}

#endif // DIA_DEBUG
