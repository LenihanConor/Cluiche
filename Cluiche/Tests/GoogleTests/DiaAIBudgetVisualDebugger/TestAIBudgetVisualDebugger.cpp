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

#endif // DIA_DEBUG
