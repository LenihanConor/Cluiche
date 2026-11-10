////////////////////////////////////////////////////////////////////////////////
// TestStateMachineVisualDebugger.cpp
// Test shapes for the StateMachineVisualDebugger IDebugDomain:
//
//   Suite StateMachineVisualDebugger_Identity    — identity methods, group, accent, no-drawers
//   Suite StateMachineVisualDebugger_JSONState   — JSON schema, stats, gates, guard results
//   Suite StateMachineVisualDebugger_Listener    — OnTransition / OnTransitionFailed callbacks
//   Suite StateMachineVisualDebugger_OnCommand   — toggle, setScale no-op, unknown/malformed
//
// Uses real FlatStateMachine (not a mock) per task spec.
// Feature spec: docs/specs/applications/dia/systems/diastatemachinevisualdebugger/diastatemachinevisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaStateMachineVisualDebugger/StateMachineVisualDebugger.h>

#include <DiaStateMachine/FlatStateMachine.h>
#include <DiaStateMachine/StateMachineBuilder.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

struct NullCtx {};

// Guard predicates for tests
static bool GuardAlwaysTrue(const void*)  { return true;  }
static bool GuardAlwaysFalse(const void*) { return false; }

// Build a simple two-state machine: idle -> patrol (trigger: start_patrol)
Dia::StateMachine::StateMachineDefinition BuildSimpleMachine()
{
    return Dia::StateMachine::StateMachineBuilder()
        .State(Dia::Core::StringCRC("idle"))
        .InitialState(Dia::Core::StringCRC("idle"))
        .Transition(Dia::Core::StringCRC("patrol"), Dia::Core::StringCRC("start_patrol"))
        .State(Dia::Core::StringCRC("patrol"))
        .Transition(Dia::Core::StringCRC("idle"), Dia::Core::StringCRC("stop_patrol"))
        .Build();
}

// Build a machine where the transition from idle to patrol has a guard
Dia::StateMachine::StateMachineDefinition BuildGuardedMachine(bool(*guardFn)(const void*))
{
    return Dia::StateMachine::StateMachineBuilder()
        .State(Dia::Core::StringCRC("idle"))
        .InitialState(Dia::Core::StringCRC("idle"))
        .Transition(Dia::Core::StringCRC("patrol"), Dia::Core::StringCRC("start_patrol"))
        .Guard(guardFn, Dia::Core::StringCRC("health_ok"))
        .State(Dia::Core::StringCRC("patrol"))
        .Transition(Dia::Core::StringCRC("idle"), Dia::Core::StringCRC("stop_patrol"))
        .Build();
}

Json::Value GetState(Dia::StateMachine::StateMachineVisualDebugger& domain)
{
    Json::Value out;
    domain.GetJSONState(out);
    return out;
}

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args(Json::objectValue);
    args["drawer"] = drawerName;
    return args;
}

} // anonymous namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(StateMachineVisualDebugger_Identity, IdsAndGroupAreCanonical)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    EXPECT_EQ(domain.GetDomainId(),    Dia::Core::StringCRC("statemachine"));
    EXPECT_STREQ(domain.GetDisplayName(), "State Machine");
    EXPECT_EQ(domain.GetGroup(),       Dia::Core::StringCRC("AIBehavior"));
    EXPECT_FALSE(domain.HasWorldDrawers());
}

TEST(StateMachineVisualDebugger_Identity, DescriptionWithin80Chars)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(StateMachineVisualDebugger_Identity, AccentIsAIBehaviorConstant)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

TEST(StateMachineVisualDebugger_Identity, GetDrawerCount_ReturnsZero)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
}

// ===========================================================================
// JSON state
// ===========================================================================

TEST(StateMachineVisualDebugger_JSONState, ReportsDrawersAndStats)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);
    const Json::Value state = GetState(domain);

    // drawers array with 2 entries, both enabled
    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    ASSERT_EQ(state["drawers"].size(), 2u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "StateList");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_STREQ(state["drawers"][1u]["name"].asCString(), "TransitionHistory");
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());

    // stats object present
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());
}

TEST(StateMachineVisualDebugger_JSONState, StatesArrayMatchesGetAllStates)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    Dia::Core::Containers::DynamicArrayC<Dia::StateMachine::StateInfo, 64> allStates;
    machine.GetAllStates(allStates);

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("states"));
    EXPECT_EQ(state["states"].size(), static_cast<Json::ArrayIndex>(allStates.Size()));
}

TEST(StateMachineVisualDebugger_JSONState, CurrentState_MarkedActive)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    const Dia::Core::StringCRC currentId = machine.GetCurrentStateId();
    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("states"));
    bool foundActive = false;
    for (Json::ArrayIndex i = 0; i < state["states"].size(); ++i)
    {
        const Json::Value& s = state["states"][i];
        const bool active = s["active"].asBool();
        if (Dia::Core::StringCRC(s["id"].asCString()) == currentId)
        {
            EXPECT_TRUE(active) << "current state must be active";
            foundActive = true;
        }
        else
        {
            EXPECT_FALSE(active) << "non-current state must not be active";
        }
    }
    EXPECT_TRUE(foundActive) << "current state must appear in states array";
}

TEST(StateMachineVisualDebugger_JSONState, HistoryArray_MatchesGetTransitionHistory)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    machine.Fire(Dia::Core::StringCRC("start_patrol"));

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    Dia::Core::Containers::DynamicArrayC<Dia::StateMachine::TransitionRecord, 32> hist;
    machine.GetTransitionHistory(hist);

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("history"));
    EXPECT_EQ(state["history"].size(), static_cast<Json::ArrayIndex>(hist.Size()));
}

TEST(StateMachineVisualDebugger_JSONState, GuardResults_Empty_BeforeAnyTransition)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);
    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("lastGuardResults"));
    EXPECT_TRUE(state["lastGuardResults"].isArray());
    EXPECT_EQ(state["lastGuardResults"].size(), 0u);
}

TEST(StateMachineVisualDebugger_JSONState, LastTransitionFailed_False_Initially)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);
    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("lastTransitionFailed"));
    EXPECT_FALSE(state["lastTransitionFailed"].asBool());
}

// ===========================================================================
// ITransitionListener
// ===========================================================================

TEST(StateMachineVisualDebugger_Listener, GuardResults_CapturedOnTransition)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildGuardedMachine(GuardAlwaysTrue), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    const bool fired = machine.Fire(Dia::Core::StringCRC("start_patrol"));
    ASSERT_TRUE(fired) << "guard should pass — transition must succeed";

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("lastGuardResults"));
    ASSERT_EQ(state["lastGuardResults"].size(), 1u)
        << "one guard was evaluated";
    EXPECT_STREQ(state["lastGuardResults"][0u]["guard"].asCString(), "health_ok");
    EXPECT_TRUE(state["lastGuardResults"][0u]["passed"].asBool());
}

TEST(StateMachineVisualDebugger_Listener, LastTransitionFailed_SetOnFailure)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    // Fire a trigger that has no matching transition from idle
    machine.Fire(Dia::Core::StringCRC("stop_patrol"));

    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state["lastTransitionFailed"].asBool());
}

TEST(StateMachineVisualDebugger_Listener, LastTransitionFailed_ClearedOnSuccess)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    // Fail first
    machine.Fire(Dia::Core::StringCRC("stop_patrol"));
    {
        const Json::Value s = GetState(domain);
        ASSERT_TRUE(s["lastTransitionFailed"].asBool());
    }

    // Then succeed
    machine.Fire(Dia::Core::StringCRC("start_patrol"));
    {
        const Json::Value s = GetState(domain);
        EXPECT_FALSE(s["lastTransitionFailed"].asBool());
    }
}

TEST(StateMachineVisualDebugger_Listener, Destructor_ClearsListener)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    {
        Dia::StateMachine::StateMachineVisualDebugger domain(machine);
        // domain registers itself as listener
    }
    // domain destroyed — listener cleared

    // Firing must not crash (null listener is guarded in FlatStateMachine)
    EXPECT_NO_FATAL_FAILURE(machine.Fire(Dia::Core::StringCRC("start_patrol")));
}

// ===========================================================================
// OnCommand
// ===========================================================================

TEST(StateMachineVisualDebugger_OnCommand, Toggle_StateList_Removes_StatesKey)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("StateList"));

    const Json::Value state = GetState(domain);
    EXPECT_FALSE(state.isMember("states")) << "states key must be absent when StateList disabled";
}

TEST(StateMachineVisualDebugger_OnCommand, Toggle_StateList_Twice_Restores)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("StateList"));
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("StateList"));

    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("states")) << "states key must be present after toggling twice";
}

TEST(StateMachineVisualDebugger_OnCommand, Toggle_TransitionHistory_Removes_HistoryKey)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    machine.Fire(Dia::Core::StringCRC("start_patrol"));

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("TransitionHistory"));

    const Json::Value state = GetState(domain);
    EXPECT_FALSE(state.isMember("history"))
        << "history key must be absent when TransitionHistory disabled";
}

TEST(StateMachineVisualDebugger_OnCommand, SetScale_NoOp_NoCrash)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    Json::Value args(Json::objectValue);
    args["key"]   = "debugScale";
    args["value"] = 2.5;

    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("setScale"), args));
}

TEST(StateMachineVisualDebugger_OnCommand, UnknownCommand_NoOp)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty));
}

TEST(StateMachineVisualDebugger_OnCommand, MalformedToggle_NoOp)
{
    NullCtx ctx;
    Dia::StateMachine::FlatStateMachine<NullCtx> machine(
        Dia::Core::StringCRC("test_machine"), BuildSimpleMachine(), ctx);

    Dia::StateMachine::StateMachineVisualDebugger domain(machine);

    // Missing "drawer" key
    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("toggle"), empty));

    // State should be unchanged
    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("states"))   << "StateList must still be enabled";
    EXPECT_TRUE(state.isMember("history"))  << "TransitionHistory must still be enabled";
}

#endif // DIA_DEBUG
