#include <gtest/gtest.h>

#include <DiaStateMachine/StateMachineTracer.h>
#include <DiaStateMachine/FlatStateMachine.h>
#include <DiaStateMachine/StateMachineBuilder.h>

using namespace Dia::StateMachine;

struct TracerTestContext {};

static void Tracer_OnEnterA(void*) {}
static void Tracer_OnExitA(void*) {}
static void Tracer_OnEnterB(void*) {}
static bool Tracer_Guard(const void*) { return true; }

struct MockListener : public ITransitionListener
{
    int transitionCount = 0;
    int failedCount = 0;
    TransitionEvent lastEvent;

    void OnTransition(const TransitionEvent& event) override
    {
        transitionCount++;
        lastEvent = event;
    }

    void OnTransitionFailed(Dia::Core::StringCRC, Dia::Core::StringCRC,
        Dia::Core::StringCRC) override
    {
        failedCount++;
    }
};

TEST(StateMachineTracer, AttachSetsListenerDetachClears)
{
    TracerTestContext ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<TracerTestContext> fsm(
        Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def),
        ctx);

    StateMachineTracer tracer;
    tracer.Attach(fsm);
    EXPECT_TRUE(tracer.IsEnabled());

    tracer.Detach();
}

TEST(StateMachineTracer, EnableDisableControls)
{
    StateMachineTracer tracer;
    EXPECT_TRUE(tracer.IsEnabled());

    tracer.Disable();
    EXPECT_FALSE(tracer.IsEnabled());

    tracer.Enable();
    EXPECT_TRUE(tracer.IsEnabled());
}

TEST(StateMachineTracer, VerbosityGetSet)
{
    StateMachineTracer tracer;
    EXPECT_EQ(tracer.GetVerbosity(), TracerVerbosity::kTransitionsOnly);

    tracer.SetVerbosity(TracerVerbosity::kFull);
    EXPECT_EQ(tracer.GetVerbosity(), TracerVerbosity::kFull);

    tracer.SetVerbosity(TracerVerbosity::kTransitionsAndGuards);
    EXPECT_EQ(tracer.GetVerbosity(), TracerVerbosity::kTransitionsAndGuards);
}

TEST(StateMachineTracer, DisabledTracerDoesNotFire)
{
    TracerTestContext ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<TracerTestContext> fsm(
        Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def),
        ctx);

    StateMachineTracer tracer;
    tracer.Disable();
    tracer.Attach(fsm);

    fsm.Update(0.016f);
    fsm.Fire(Dia::Core::StringCRC("Go"));
    EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("B"));
}

TEST(StateMachineTracer, RateLimiterBlocksExcessEntriesPerSecond)
{
    TracerTestContext ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B"))
            .Transition(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Back"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<TracerTestContext> fsm(
        Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def),
        ctx);

    StateMachineTracer tracer;
    tracer.SetMaxEntriesPerSecond(2);
    tracer.Attach(fsm);

    fsm.Update(0.016f);
    fsm.Fire(Dia::Core::StringCRC("Go"));
    fsm.Fire(Dia::Core::StringCRC("Back"));
    fsm.Fire(Dia::Core::StringCRC("Go"));

    EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("B"));

    tracer.Detach();
}

TEST(StateMachineTracer, RateLimiterResetsAfterOneSecond)
{
    TracerTestContext ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
            .Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Go"))
        .State(Dia::Core::StringCRC("B"))
            .Transition(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Back"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<TracerTestContext> fsm(
        Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def),
        ctx);

    StateMachineTracer tracer;
    tracer.SetMaxEntriesPerSecond(1);
    tracer.Attach(fsm);

    fsm.Update(0.016f);
    fsm.Fire(Dia::Core::StringCRC("Go"));

    fsm.Update(2.0f);
    fsm.Fire(Dia::Core::StringCRC("Back"));

    EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("A"));

    tracer.Detach();
}

TEST(StateMachineTracer, OnTransitionFailedCalledOnNoMatch)
{
    TracerTestContext ctx;
    auto def = StateMachineBuilder()
        .State(Dia::Core::StringCRC("A"))
        .InitialState(Dia::Core::StringCRC("A"))
        .Build();

    FlatStateMachine<TracerTestContext> fsm(
        Dia::Core::StringCRC("M"),
        static_cast<StateMachineDefinition&&>(def),
        ctx);

    StateMachineTracer tracer;
    tracer.Attach(fsm);

    bool result = fsm.Fire(Dia::Core::StringCRC("Ghost"));
    EXPECT_FALSE(result);

    tracer.Detach();
}
