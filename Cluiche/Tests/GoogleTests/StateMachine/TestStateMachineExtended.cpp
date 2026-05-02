#include <gtest/gtest.h>

#include <DiaStateMachine/FlatStateMachine.h>
#include <DiaStateMachine/StateMachineBuilder.h>
#include <DiaStateMachine/HierarchicalStateMachine.h>
#include <DiaStateMachine/HierarchicalStateMachineBuilder.h>
#include <DiaStateMachine/PushdownAutomaton.h>
#include <DiaStateMachine/PushdownAutomatonBuilder.h>

using namespace Dia::StateMachine;

// ============================================================
// Shared helpers
// ============================================================

struct ExtCtx
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> log;
    int enterCount = 0;
    int exitCount = 0;
    float lastDeltaTime = 0.0f;
};

static void Ext_OnEnter(void* ctx, const char* name)
{
    auto& c = *static_cast<ExtCtx*>(ctx);
    c.enterCount++;
    c.log.Add(Dia::Core::StringCRC(name));
}
static void Ext_OnExit(void* ctx, const char* name)
{
    auto& c = *static_cast<ExtCtx*>(ctx);
    c.exitCount++;
    c.log.Add(Dia::Core::StringCRC(name));
}
static void EnterA(void* ctx) { Ext_OnEnter(ctx, "EnterA"); }
static void ExitA(void* ctx)  { Ext_OnExit(ctx,  "ExitA");  }
static void EnterB(void* ctx) { Ext_OnEnter(ctx, "EnterB"); }
static void ExitB(void* ctx)  { Ext_OnExit(ctx,  "ExitB");  }
static void EnterC(void* ctx) { Ext_OnEnter(ctx, "EnterC"); }

struct CapturingListener : public ITransitionListener
{
    int transitionCount = 0;
    int failedCount = 0;
    TransitionEvent lastEvent;
    Dia::Core::StringCRC lastFailedMachine;
    Dia::Core::StringCRC lastFailedState;
    Dia::Core::StringCRC lastFailedTrigger;

    void OnTransition(const TransitionEvent& event) override
    {
        transitionCount++;
        lastEvent = event;
    }

    void OnTransitionFailed(Dia::Core::StringCRC machineId,
        Dia::Core::StringCRC currentStateId,
        Dia::Core::StringCRC triggerId) override
    {
        failedCount++;
        lastFailedMachine = machineId;
        lastFailedState = currentStateId;
        lastFailedTrigger = triggerId;
    }
};

// ============================================================
// FlatStateMachine — additional tests
// ============================================================

TEST(FlatStateMachine, SelfTransitionCallsExitAndReenter)
{
    ExtCtx ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .OnEnter(EnterA).OnExit(ExitA)
            .Transition(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Reset"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    ctx.log.RemoveAll();
    ctx.enterCount = 0;
    ctx.exitCount = 0;

    bool result = fsm.Fire(Dia::Core::StringCRC("Reset"));
    EXPECT_TRUE(result);
    EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("A"));
    EXPECT_EQ(ctx.exitCount, 1);
    EXPECT_EQ(ctx.enterCount, 1);
    ASSERT_GE(ctx.log.Size(), 2u);
    EXPECT_EQ(ctx.log[0], Dia::Core::StringCRC("ExitA"));
    EXPECT_EQ(ctx.log[1], Dia::Core::StringCRC("EnterA"));
}

TEST(FlatStateMachine, GetAllTransitionsReturnsCorrectInfo)
{
    ExtCtx ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    Dia::Core::Containers::DynamicArrayC<TransitionInfo, 64> transitions;
    fsm.GetAllTransitions(transitions);

    ASSERT_EQ(transitions.Size(), 1u);
    EXPECT_EQ(transitions[0].sourceStateId, Dia::Core::StringCRC("A"));
    EXPECT_EQ(transitions[0].targetStateId, Dia::Core::StringCRC("B"));
    EXPECT_EQ(transitions[0].triggerId, Dia::Core::StringCRC("Go"));
    EXPECT_FALSE(transitions[0].hasGuard);
}

static bool GuardAlwaysTrue(const void*) { return true; }

TEST(FlatStateMachine, GetAllTransitionsReportsGuardPresence)
{
    ExtCtx ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
                .Guard(GuardAlwaysTrue)
        .State(Dia::Core::StringCRC("B"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    Dia::Core::Containers::DynamicArrayC<TransitionInfo, 64> transitions;
    fsm.GetAllTransitions(transitions);

    ASSERT_EQ(transitions.Size(), 1u);
    EXPECT_TRUE(transitions[0].hasGuard);
}

TEST(FlatStateMachine, ListenerReceivesTransitionEvent)
{
    ExtCtx ctx;
    CapturingListener listener;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("TestMachine"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    fsm.SetTransitionListener(&listener);
    fsm.Update(1.0f);
    fsm.Fire(Dia::Core::StringCRC("Go"));

    EXPECT_EQ(listener.transitionCount, 1);
    EXPECT_EQ(listener.lastEvent.sourceStateId, Dia::Core::StringCRC("A"));
    EXPECT_EQ(listener.lastEvent.targetStateId, Dia::Core::StringCRC("B"));
    EXPECT_EQ(listener.lastEvent.triggerId, Dia::Core::StringCRC("Go"));
    EXPECT_NEAR(listener.lastEvent.timestamp, 1.0f, 0.001f);
}

TEST(FlatStateMachine, ListenerReceivesResidenceTime)
{
    ExtCtx ctx;
    CapturingListener listener;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    fsm.SetTransitionListener(&listener);
    fsm.Update(0.5f);
    fsm.Update(0.5f);
    fsm.Fire(Dia::Core::StringCRC("Go"));

    EXPECT_NEAR(listener.lastEvent.stateResidenceTime, 1.0f, 0.001f);
}

static bool GuardFail(const void*) { return false; }
static bool GuardPass(const void*) { return true; }

TEST(FlatStateMachine, ListenerReceivesGuardResults)
{
    ExtCtx ctx;
    CapturingListener listener;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
                .Guard(GuardFail, Dia::Core::StringCRC("FailGuard"))
            .Transition(Dia::Core::StringCRC("C"), Dia::Core::StringCRC("Go"))
                .Guard(GuardPass, Dia::Core::StringCRC("PassGuard"))
        .State(Dia::Core::StringCRC("B"))
        .State(Dia::Core::StringCRC("C"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    fsm.SetTransitionListener(&listener);
    fsm.Fire(Dia::Core::StringCRC("Go"));

    EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("C"));
    EXPECT_EQ(listener.transitionCount, 1);
    ASSERT_GE(listener.lastEvent.guardCount, 1);
}

TEST(FlatStateMachine, ListenerReceivesFailedEvent)
{
    ExtCtx ctx;
    CapturingListener listener;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("TestMachine"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    fsm.SetTransitionListener(&listener);
    fsm.Fire(Dia::Core::StringCRC("Ghost"));

    EXPECT_EQ(listener.failedCount, 1);
    EXPECT_EQ(listener.lastFailedMachine, Dia::Core::StringCRC("TestMachine"));
    EXPECT_EQ(listener.lastFailedState, Dia::Core::StringCRC("A"));
    EXPECT_EQ(listener.lastFailedTrigger, Dia::Core::StringCRC("Ghost"));
}

TEST(FlatStateMachine, SetListenerNullSilences)
{
    ExtCtx ctx;
    CapturingListener listener;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    fsm.SetTransitionListener(&listener);
    fsm.SetTransitionListener(nullptr);
    fsm.Fire(Dia::Core::StringCRC("Ghost"));

    EXPECT_EQ(listener.failedCount, 0);
}

// Boundary: kMaxTransitionsPerFrame == 8. All 8 succeed within one Update window.
TEST(FlatStateMachine_Stress, MaxTransitionsPerFrameAllSucceed)
{
    ExtCtx ctx;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A")).Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Flip"))
        .State(Dia::Core::StringCRC("B")).Transition(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Flip"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    fsm.Update(0.016f);

    for (int i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(fsm.Fire(Dia::Core::StringCRC("Flip"))) << "transition " << i << " should succeed";
    }
}

// kHistorySize == 32, so stay within that limit across frames
TEST(FlatStateMachine_Stress, RapidTransitionsOverManyFrames)
{
    ExtCtx ctx;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A")).Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Flip"))
        .State(Dia::Core::StringCRC("B")).Transition(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Flip"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ExtCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    for (int frame = 0; frame < 30; ++frame)
    {
        fsm.Update(0.016f);
        fsm.Fire(Dia::Core::StringCRC("Flip"));
    }

    Dia::Core::StringCRC finalState = fsm.GetCurrentStateId();
    bool validState = (finalState == Dia::Core::StringCRC("A") ||
                       finalState == Dia::Core::StringCRC("B"));
    EXPECT_TRUE(validState);
}

// ============================================================
// HierarchicalStateMachine — additional tests
// ============================================================

TEST(HierarchicalStateMachine, SelfTransitionExitsAndReenters)
{
    ExtCtx ctx;
    auto def = HierarchicalStateMachineBuilder()
        .State(Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("A"))
        .ChildState(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Root"))
            .OnEnter(EnterA).OnExit(ExitA)
            .Transition(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Reset"))
        .InitialState(Dia::Core::StringCRC("Root"))
        .Build();

    HierarchicalStateMachine<ExtCtx> hsm(Dia::Core::StringCRC("HSM"),
        static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

    ctx.log.RemoveAll();
    ctx.enterCount = 0;
    ctx.exitCount = 0;

    bool result = hsm.Fire(Dia::Core::StringCRC("Reset"));
    EXPECT_TRUE(result);
    EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("A"));
    EXPECT_GE(ctx.exitCount, 1);
    EXPECT_GE(ctx.enterCount, 1);
}

TEST(HierarchicalStateMachine, GetAllTransitionsReturnsInfo)
{
    ExtCtx ctx;
    auto def = HierarchicalStateMachineBuilder()
        .State(Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("A"))
        .ChildState(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Root"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .ChildState(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Root"))
        .InitialState(Dia::Core::StringCRC("Root"))
        .Build();

    HierarchicalStateMachine<ExtCtx> hsm(Dia::Core::StringCRC("HSM"),
        static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

    Dia::Core::Containers::DynamicArrayC<TransitionInfo, 64> transitions;
    hsm.GetAllTransitions(transitions);

    ASSERT_EQ(transitions.Size(), 1u);
    EXPECT_EQ(transitions[0].sourceStateId, Dia::Core::StringCRC("A"));
    EXPECT_EQ(transitions[0].targetStateId, Dia::Core::StringCRC("B"));
    EXPECT_EQ(transitions[0].triggerId, Dia::Core::StringCRC("Go"));
}

TEST(HierarchicalStateMachine, ListenerReceivesEvent)
{
    ExtCtx ctx;
    CapturingListener listener;

    auto def = HierarchicalStateMachineBuilder()
        .State(Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("A"))
        .ChildState(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Root"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .ChildState(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Root"))
        .InitialState(Dia::Core::StringCRC("Root"))
        .Build();

    HierarchicalStateMachine<ExtCtx> hsm(Dia::Core::StringCRC("HSM"),
        static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

    hsm.SetTransitionListener(&listener);
    hsm.Fire(Dia::Core::StringCRC("Go"));

    EXPECT_EQ(listener.transitionCount, 1);
    EXPECT_EQ(listener.lastEvent.sourceStateId, Dia::Core::StringCRC("A"));
    EXPECT_EQ(listener.lastEvent.targetStateId, Dia::Core::StringCRC("B"));
}

TEST(HierarchicalStateMachine, ListenerReceivesFailedEvent)
{
    ExtCtx ctx;
    CapturingListener listener;

    auto def = HierarchicalStateMachineBuilder()
        .State(Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("A"))
        .ChildState(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Root"))
        .InitialState(Dia::Core::StringCRC("Root"))
        .Build();

    HierarchicalStateMachine<ExtCtx> hsm(Dia::Core::StringCRC("HSM"),
        static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

    hsm.SetTransitionListener(&listener);
    hsm.Fire(Dia::Core::StringCRC("Ghost"));

    EXPECT_EQ(listener.failedCount, 1);
    EXPECT_EQ(listener.lastFailedTrigger, Dia::Core::StringCRC("Ghost"));
}

TEST(HierarchicalStateMachine, NoHistoryDefaultsToInitialChild)
{
    ExtCtx ctx;
    auto def = HierarchicalStateMachineBuilder()
        .State(Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("Alive"))
        .ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("Idle"))
            .Transition(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Die"))
        .ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
            .Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Walk"))
        .ChildState(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Alive"))
        .ChildState(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Root"))
            .Transition(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Revive"))
        .InitialState(Dia::Core::StringCRC("Root"))
        .Build();

    HierarchicalStateMachine<ExtCtx> hsm(Dia::Core::StringCRC("HSM"),
        static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

    hsm.Fire(Dia::Core::StringCRC("Walk"));
    EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("Walking"));

    hsm.Fire(Dia::Core::StringCRC("Die"));
    hsm.Fire(Dia::Core::StringCRC("Revive"));

    EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("Idle"));
}

// ============================================================
// PushdownAutomaton — additional tests
// ============================================================

struct PDAExtCtx
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> log;
};

static void PDAExt_Enter(void* ctx, const char* name)
{
    static_cast<PDAExtCtx*>(ctx)->log.Add(Dia::Core::StringCRC(name));
}
static void EnterMain(void* ctx) { PDAExt_Enter(ctx, "EnterMain"); }
static void EnterPause(void* ctx) { PDAExt_Enter(ctx, "EnterPause"); }

TEST(PushdownAutomaton, PushUnknownStateReturnsFalse)
{
    PDAExtCtx ctx;
    auto def = PushdownAutomatonBuilder()
        .State(Dia::Core::StringCRC("Main"))
        .InitialState(Dia::Core::StringCRC("Main"))
        .Build();

    PushdownAutomaton<PDAExtCtx> pda(Dia::Core::StringCRC("PDA"),
        static_cast<PushdownAutomatonDefinition&&>(def), ctx);

    bool result = pda.Push(Dia::Core::StringCRC("NonExistent"));
    EXPECT_FALSE(result);
    EXPECT_EQ(pda.GetStackDepth(), 1);
    EXPECT_EQ(pda.GetCurrentStateId(), Dia::Core::StringCRC("Main"));
}

TEST(PushdownAutomaton, IsInStateMatchesTopOfStack)
{
    PDAExtCtx ctx;
    auto def = PushdownAutomatonBuilder()
        .State(Dia::Core::StringCRC("Main"))
        .State(Dia::Core::StringCRC("Pause"))
        .InitialState(Dia::Core::StringCRC("Main"))
        .Build();

    PushdownAutomaton<PDAExtCtx> pda(Dia::Core::StringCRC("PDA"),
        static_cast<PushdownAutomatonDefinition&&>(def), ctx);

    EXPECT_TRUE(pda.IsInState(Dia::Core::StringCRC("Main")));
    EXPECT_FALSE(pda.IsInState(Dia::Core::StringCRC("Pause")));

    pda.Push(Dia::Core::StringCRC("Pause"));
    EXPECT_TRUE(pda.IsInState(Dia::Core::StringCRC("Pause")));
    EXPECT_FALSE(pda.IsInState(Dia::Core::StringCRC("Main")));
}

TEST(PushdownAutomaton, GetAllStatesReportsTopAsActive)
{
    PDAExtCtx ctx;
    auto def = PushdownAutomatonBuilder()
        .State(Dia::Core::StringCRC("Main"))
        .State(Dia::Core::StringCRC("Pause"))
        .InitialState(Dia::Core::StringCRC("Main"))
        .Build();

    PushdownAutomaton<PDAExtCtx> pda(Dia::Core::StringCRC("PDA"),
        static_cast<PushdownAutomatonDefinition&&>(def), ctx);

    pda.Push(Dia::Core::StringCRC("Pause"));

    Dia::Core::Containers::DynamicArrayC<StateInfo, 64> states;
    pda.GetAllStates(states);

    ASSERT_EQ(states.Size(), 2u);
    for (unsigned int i = 0; i < states.Size(); ++i)
    {
        if (states[i].id == Dia::Core::StringCRC("Pause"))
        {
            EXPECT_TRUE(states[i].isActive);
        }
        else
        {
            EXPECT_FALSE(states[i].isActive);
        }
    }
}

TEST(PushdownAutomaton, GetAllTransitionsReturnsEmpty)
{
    PDAExtCtx ctx;
    auto def = PushdownAutomatonBuilder()
        .State(Dia::Core::StringCRC("Main"))
        .InitialState(Dia::Core::StringCRC("Main"))
        .Build();

    PushdownAutomaton<PDAExtCtx> pda(Dia::Core::StringCRC("PDA"),
        static_cast<PushdownAutomatonDefinition&&>(def), ctx);

    Dia::Core::Containers::DynamicArrayC<TransitionInfo, 64> transitions;
    pda.GetAllTransitions(transitions);

    EXPECT_EQ(transitions.Size(), 0u);
}

TEST(PushdownAutomaton, ListenerReceivesPushEvent)
{
    PDAExtCtx ctx;
    CapturingListener listener;

    auto def = PushdownAutomatonBuilder()
        .State(Dia::Core::StringCRC("Main"))
        .State(Dia::Core::StringCRC("Pause"))
        .InitialState(Dia::Core::StringCRC("Main"))
        .Build();

    PushdownAutomaton<PDAExtCtx> pda(Dia::Core::StringCRC("PDA"),
        static_cast<PushdownAutomatonDefinition&&>(def), ctx);

    pda.SetTransitionListener(&listener);
    pda.Push(Dia::Core::StringCRC("Pause"));

    EXPECT_EQ(listener.transitionCount, 1);
    EXPECT_EQ(listener.lastEvent.sourceStateId, Dia::Core::StringCRC("Main"));
    EXPECT_EQ(listener.lastEvent.targetStateId, Dia::Core::StringCRC("Pause"));
    EXPECT_EQ(listener.lastEvent.triggerId, Dia::Core::StringCRC("Push"));
}

TEST(PushdownAutomaton, ListenerReceivesPopEvent)
{
    PDAExtCtx ctx;
    CapturingListener listener;

    auto def = PushdownAutomatonBuilder()
        .State(Dia::Core::StringCRC("Main"))
        .State(Dia::Core::StringCRC("Pause"))
        .InitialState(Dia::Core::StringCRC("Main"))
        .Build();

    PushdownAutomaton<PDAExtCtx> pda(Dia::Core::StringCRC("PDA"),
        static_cast<PushdownAutomatonDefinition&&>(def), ctx);

    pda.Push(Dia::Core::StringCRC("Pause"));
    pda.SetTransitionListener(&listener);
    pda.Pop();

    EXPECT_EQ(listener.transitionCount, 1);
    EXPECT_EQ(listener.lastEvent.sourceStateId, Dia::Core::StringCRC("Pause"));
    EXPECT_EQ(listener.lastEvent.targetStateId, Dia::Core::StringCRC("Main"));
    EXPECT_EQ(listener.lastEvent.triggerId, Dia::Core::StringCRC("Pop"));
}

TEST(PushdownAutomaton, GetMachineIdCorrect)
{
    PDAExtCtx ctx;
    auto def = PushdownAutomatonBuilder()
        .State(Dia::Core::StringCRC("Main"))
        .InitialState(Dia::Core::StringCRC("Main"))
        .Build();

    PushdownAutomaton<PDAExtCtx> pda(Dia::Core::StringCRC("MyPDA"),
        static_cast<PushdownAutomatonDefinition&&>(def), ctx);

    EXPECT_EQ(pda.GetMachineId(), Dia::Core::StringCRC("MyPDA"));
}
