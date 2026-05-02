#include <gtest/gtest.h>

#include <DiaStateMachine/Testing/TransitionRecorder.h>
#include <DiaStateMachine/FlatStateMachine.h>
#include <DiaStateMachine/StateMachineBuilder.h>
#include <DiaStateMachine/StateMachineDefinition.h>
#include <DiaStateMachine/StateMachineTracer.h>
#include <DiaStateMachine/HierarchicalStateMachine.h>
#include <DiaStateMachine/HierarchicalStateMachineBuilder.h>

using namespace Dia::StateMachine;
using namespace Dia::StateMachine::Testing;

// ============================================================
// TransitionRecorder
// ============================================================

TEST(TransitionRecorder, InitiallyEmpty)
{
    TransitionRecorder rec;
    EXPECT_EQ(rec.Count(), 0u);
    EXPECT_EQ(rec.GetSequence().Size(), 0u);
}

TEST(TransitionRecorder, RecordEnterAppearsInSequence)
{
    TransitionRecorder rec;
    rec.RecordEnter(Dia::Core::StringCRC("Idle"));

    ASSERT_EQ(rec.Count(), 1u);
    EXPECT_EQ(rec.GetSequence()[0].type, CallbackType::kEnter);
    EXPECT_EQ(rec.GetSequence()[0].stateId, Dia::Core::StringCRC("Idle"));
}

TEST(TransitionRecorder, RecordExitAppearsInSequence)
{
    TransitionRecorder rec;
    rec.RecordExit(Dia::Core::StringCRC("Idle"));

    ASSERT_EQ(rec.Count(), 1u);
    EXPECT_EQ(rec.GetSequence()[0].type, CallbackType::kExit);
}

TEST(TransitionRecorder, RecordUpdateAppearsInSequence)
{
    TransitionRecorder rec;
    rec.RecordUpdate(Dia::Core::StringCRC("Running"));

    ASSERT_EQ(rec.Count(), 1u);
    EXPECT_EQ(rec.GetSequence()[0].type, CallbackType::kUpdate);
    EXPECT_EQ(rec.GetSequence()[0].stateId, Dia::Core::StringCRC("Running"));
}

TEST(TransitionRecorder, RecordPauseAppearsInSequence)
{
    TransitionRecorder rec;
    rec.RecordPause(Dia::Core::StringCRC("Main"));

    ASSERT_EQ(rec.Count(), 1u);
    EXPECT_EQ(rec.GetSequence()[0].type, CallbackType::kPause);
}

TEST(TransitionRecorder, RecordResumeAppearsInSequence)
{
    TransitionRecorder rec;
    rec.RecordResume(Dia::Core::StringCRC("Main"));

    ASSERT_EQ(rec.Count(), 1u);
    EXPECT_EQ(rec.GetSequence()[0].type, CallbackType::kResume);
}

TEST(TransitionRecorder, SequencePreservesOrder)
{
    TransitionRecorder rec;
    rec.RecordEnter(Dia::Core::StringCRC("A"));
    rec.RecordExit(Dia::Core::StringCRC("A"));
    rec.RecordEnter(Dia::Core::StringCRC("B"));

    ASSERT_EQ(rec.Count(), 3u);
    EXPECT_EQ(rec.GetSequence()[0].type, CallbackType::kEnter);
    EXPECT_EQ(rec.GetSequence()[0].stateId, Dia::Core::StringCRC("A"));
    EXPECT_EQ(rec.GetSequence()[1].type, CallbackType::kExit);
    EXPECT_EQ(rec.GetSequence()[1].stateId, Dia::Core::StringCRC("A"));
    EXPECT_EQ(rec.GetSequence()[2].type, CallbackType::kEnter);
    EXPECT_EQ(rec.GetSequence()[2].stateId, Dia::Core::StringCRC("B"));
}

TEST(TransitionRecorder, ClearResetsToEmpty)
{
    TransitionRecorder rec;
    rec.RecordEnter(Dia::Core::StringCRC("A"));
    rec.RecordExit(Dia::Core::StringCRC("A"));
    EXPECT_EQ(rec.Count(), 2u);

    rec.Clear();
    EXPECT_EQ(rec.Count(), 0u);
    EXPECT_EQ(rec.GetSequence().Size(), 0u);
}

TEST(TransitionRecorder, ClearThenRecordAgain)
{
    TransitionRecorder rec;
    rec.RecordEnter(Dia::Core::StringCRC("A"));
    rec.Clear();
    rec.RecordEnter(Dia::Core::StringCRC("B"));

    ASSERT_EQ(rec.Count(), 1u);
    EXPECT_EQ(rec.GetSequence()[0].stateId, Dia::Core::StringCRC("B"));
}

// Use TransitionRecorder as actual state machine callbacks to verify it works
// as intended in a real FSM setup.

static TransitionRecorder* sRecorder = nullptr;

static void TR_EnterA(void*) { sRecorder->RecordEnter(Dia::Core::StringCRC("A")); }
static void TR_ExitA(void*)  { sRecorder->RecordExit(Dia::Core::StringCRC("A"));  }
static void TR_EnterB(void*) { sRecorder->RecordEnter(Dia::Core::StringCRC("B")); }

TEST(TransitionRecorder, WorksAsActualFSMCallback)
{
    struct Ctx {};
    Ctx ctx;
    TransitionRecorder rec;
    sRecorder = &rec;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A")).OnEnter(TR_EnterA).OnExit(TR_ExitA)
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B")).OnEnter(TR_EnterB)
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<Ctx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    rec.Clear();
    fsm.Fire(Dia::Core::StringCRC("Go"));

    ASSERT_EQ(rec.Count(), 2u);
    EXPECT_EQ(rec.GetSequence()[0].type, CallbackType::kExit);
    EXPECT_EQ(rec.GetSequence()[0].stateId, Dia::Core::StringCRC("A"));
    EXPECT_EQ(rec.GetSequence()[1].type, CallbackType::kEnter);
    EXPECT_EQ(rec.GetSequence()[1].stateId, Dia::Core::StringCRC("B"));

    sRecorder = nullptr;
}

// ============================================================
// FlatStateMachine: transitions-per-frame counter resets on Update
// ============================================================

struct ResetCtx {};

TEST(FlatStateMachine, TransitionsPerFrameCounterResetsOnUpdate)
{
    ResetCtx ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Flip"))
        .State(Dia::Core::StringCRC("B"))
            .Transition(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Flip"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<ResetCtx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    fsm.Update(0.016f);
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(fsm.Fire(Dia::Core::StringCRC("Flip"))) << "frame 1, transition " << i;
    }

    fsm.Update(0.016f);
    EXPECT_TRUE(fsm.Fire(Dia::Core::StringCRC("Flip")));
}

// ============================================================
// StateMachineDefinition: Clone and IsValid
// ============================================================

TEST(StateMachineDefinition, CloneProducesIndependentCopy)
{
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    auto copy = def.Clone();

    EXPECT_EQ(copy.GetStates().Size(), def.GetStates().Size());
    EXPECT_EQ(copy.GetTransitions().Size(), def.GetTransitions().Size());
    EXPECT_EQ(copy.GetInitialStateId(), def.GetInitialStateId());
    EXPECT_TRUE(copy.IsValid());
}

TEST(StateMachineDefinition, CloneIsIndependentFromOriginal)
{
    struct Ctx {};
    Ctx ctx1, ctx2;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    auto copy = def.Clone();

    FlatStateMachine<Ctx> fsm1(Dia::Core::StringCRC("M1"),
        static_cast<StateMachineDefinition&&>(def), ctx1);
    FlatStateMachine<Ctx> fsm2(Dia::Core::StringCRC("M2"),
        static_cast<StateMachineDefinition&&>(copy), ctx2);

    EXPECT_EQ(fsm1.GetCurrentStateId(), Dia::Core::StringCRC("A"));
    EXPECT_EQ(fsm2.GetCurrentStateId(), Dia::Core::StringCRC("A"));
}

TEST(StateMachineDefinition, IsValidTrueForWellFormedDefinition)
{
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    EXPECT_TRUE(def.IsValid());
}

// ============================================================
// StateMachineTracer: kFull verbosity fires without crashing
// ============================================================

TEST(StateMachineTracer, FullVerbosityDoesNotCrash)
{
    struct Ctx {};
    Ctx ctx;

    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<Ctx> fsm(Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def), ctx);

    StateMachineTracer tracer;
    tracer.SetVerbosity(TracerVerbosity::kFull);
    tracer.Attach(fsm);

    fsm.Update(0.5f);
    fsm.Fire(Dia::Core::StringCRC("Go"));

    EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("B"));

    tracer.Detach();
}

// ============================================================
// HierarchicalStateMachine: 3-level deep cross-branch transition
// ============================================================

// Root → A → AA (initial)
//           → AB
//      → B  → BA (initial)
//           → BB

struct DeepCtx
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> log;
};

static void Deep_OnEnter(void* ctx, const char* name)
{
    static_cast<DeepCtx*>(ctx)->log.Add(Dia::Core::StringCRC(name));
}
static void Deep_OnExit(void* ctx, const char* name)
{
    static_cast<DeepCtx*>(ctx)->log.Add(Dia::Core::StringCRC(name));
}

static void EnterRoot(void* c) { Deep_OnEnter(c, "EnterRoot"); }
static void EnterA(void* c)    { Deep_OnEnter(c, "EnterA");    }
static void EnterAA(void* c)   { Deep_OnEnter(c, "EnterAA");   }
static void EnterAB(void* c)   { Deep_OnEnter(c, "EnterAB");   }
static void EnterB(void* c)    { Deep_OnEnter(c, "EnterB");    }
static void EnterBA(void* c)   { Deep_OnEnter(c, "EnterBA");   }
static void ExitAA(void* c)    { Deep_OnExit(c,  "ExitAA");    }
static void ExitA(void* c)     { Deep_OnExit(c,  "ExitA");     }

TEST(HierarchicalStateMachine, ThreeLevelDeepCrossBranchTransition)
{
    DeepCtx ctx;

    auto def = HierarchicalStateMachineBuilder()
        .State(Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("A"))
            .OnEnter(EnterRoot)
        .ChildState(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("AA"))
            .OnEnter(EnterA).OnExit(ExitA)
        .ChildState(Dia::Core::StringCRC("AA"), Dia::Core::StringCRC("A"))
            .OnEnter(EnterAA).OnExit(ExitAA)
            .Transition(Dia::Core::StringCRC("BA"), Dia::Core::StringCRC("Switch"))
        .ChildState(Dia::Core::StringCRC("AB"), Dia::Core::StringCRC("A"))
            .OnEnter(EnterAB)
        .ChildState(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("BA"))
            .OnEnter(EnterB)
        .ChildState(Dia::Core::StringCRC("BA"), Dia::Core::StringCRC("B"))
            .OnEnter(EnterBA)
        .InitialState(Dia::Core::StringCRC("Root"))
        .Build();

    HierarchicalStateMachine<DeepCtx> hsm(Dia::Core::StringCRC("DeepHSM"),
        static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

    EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("AA"));
    EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("A")));
    EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("Root")));

    ctx.log.RemoveAll();
    bool result = hsm.Fire(Dia::Core::StringCRC("Switch"));

    EXPECT_TRUE(result);
    EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("BA"));
    EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("B")));
    EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("Root")));
    EXPECT_FALSE(hsm.IsInState(Dia::Core::StringCRC("A")));

    // Exit order: AA, A (LCA is Root, so Root stays active)
    // Enter order: B, BA
    ASSERT_GE(ctx.log.Size(), 4u);
    EXPECT_EQ(ctx.log[0], Dia::Core::StringCRC("ExitAA"));
    EXPECT_EQ(ctx.log[1], Dia::Core::StringCRC("ExitA"));
    EXPECT_EQ(ctx.log[2], Dia::Core::StringCRC("EnterB"));
    EXPECT_EQ(ctx.log[3], Dia::Core::StringCRC("EnterBA"));
}

TEST(HierarchicalStateMachine, ThreeLevelActivePathIsCorrect)
{
    DeepCtx ctx;

    auto def = HierarchicalStateMachineBuilder()
        .State(Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("A"))
        .ChildState(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Root"))
            .InitialChild(Dia::Core::StringCRC("AA"))
        .ChildState(Dia::Core::StringCRC("AA"), Dia::Core::StringCRC("A"))
        .InitialState(Dia::Core::StringCRC("Root"))
        .Build();

    HierarchicalStateMachine<DeepCtx> hsm(Dia::Core::StringCRC("DeepHSM"),
        static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

    Dia::Core::Containers::DynamicArrayC<StateInfo, 64> states;
    hsm.GetAllStates(states);

    int activeCount = 0;
    for (unsigned int i = 0; i < states.Size(); ++i)
    {
        if (states[i].isActive) activeCount++;
    }
    EXPECT_EQ(activeCount, 3);
    EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("Root")));
    EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("A")));
    EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("AA")));
}
