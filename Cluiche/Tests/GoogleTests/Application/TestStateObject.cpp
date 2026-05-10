////////////////////////////////////////////////////////////////////////////////
// TestStateObject.cpp
//
// Tests for Dia::Application::StateObject lifecycle, state transitions,
// and assertion guards.
//
// StateObject is abstract; tests exercise it via a minimal Module subclass
// (which provides stub DoStart/DoUpdate/DoStop implementations).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/ApplicationStateObject.h>
#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>

using namespace Dia::Application;
using namespace Dia::Core;

//-----------------------------------------------------------------------------
// Minimal ProcessingUnit required to construct a Module
//-----------------------------------------------------------------------------
class StateObjectTestPU : public ProcessingUnit
{
public:
    StateObjectTestPU()
        : ProcessingUnit(StringCRC("StateObjectTestPU"), -1.0f, 4, 4)
    {}
    virtual bool FlaggedToStopUpdating() const override { return true; }
};

//-----------------------------------------------------------------------------
// Minimal Module that records lifecycle calls for assertion
//-----------------------------------------------------------------------------
class MinimalModule : public Module
{
public:
    MinimalModule(ProcessingUnit* pu, const StringCRC& id)
        : Module(pu, id, RunningEnum::kUpdate)
    {}

    int startCalls  = 0;
    int updateCalls = 0;
    int stopCalls   = 0;

    virtual OpertionResponse DoStart(const IStartData*) override
    {
        startCalls++;
        return OpertionResponse::kImmediate;
    }

    virtual void DoUpdate() override
    {
        updateCalls++;
    }

    virtual void DoStop() override
    {
        stopCalls++;
    }
};

//-----------------------------------------------------------------------------
// Helper: produce a Module that has been through BuildDependancies() so it
// is in kNotRunning and ready for Start().
//-----------------------------------------------------------------------------
static void ReadyForStart(StateObjectTestPU& pu, MinimalModule& mod)
{
    // BuildDependancies transitions kConstructed -> kNotRunning
    mod.BuildDependancies(nullptr);
    (void)pu;
}

//=============================================================================
// Tests
//=============================================================================

// 1. DefaultConstruction — freshly constructed module is in kConstructed,
//    HasStarted() == false.
TEST(DiaApplicationFlow_StateObject, DefaultConstruction)
{
    StateObjectTestPU pu;
    MinimalModule mod(&pu, StringCRC("ModDefault"));

    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kConstructed);
    EXPECT_FALSE(mod.HasStarted());
}

// 2. Start_TransitionsToRunning — after BuildDependancies then Start(),
//    state is kRunning and HasStarted() == true.
TEST(DiaApplicationFlow_StateObject, Start_TransitionsToRunning)
{
    StateObjectTestPU pu;
    MinimalModule mod(&pu, StringCRC("ModStart"));

    ReadyForStart(pu, mod);

    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kNotRunning);

    mod.Start();

    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kRunning);
    EXPECT_TRUE(mod.HasStarted());
}

// 3. Stop_TransitionsToStopped — after Start() then Stop(), state is
//    kNotRunning and HasStarted() == false.
TEST(DiaApplicationFlow_StateObject, Stop_TransitionsToStopped)
{
    StateObjectTestPU pu;
    MinimalModule mod(&pu, StringCRC("ModStop"));

    ReadyForStart(pu, mod);
    mod.Start();

    EXPECT_TRUE(mod.HasStarted());

    mod.Stop();

    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kNotRunning);
    EXPECT_FALSE(mod.HasStarted());
}

// 4. DoubleStart_Asserts — calling Start() twice without an intervening
//    Stop() triggers a DIA_ASSERT (process terminates in Debug builds).
TEST(DiaApplicationFlow_StateObject, DoubleStart_Asserts)
{
    EXPECT_DEATH(
    {
        StateObjectTestPU pu;
        MinimalModule mod(&pu, StringCRC("ModDoubleStart"));
        ReadyForStart(pu, mod);
        mod.Start();
        mod.Start();  // second Start() while kRunning — asserts
    }, "");
}

// 5. StopWithoutStart_Asserts — calling Stop() from kNotRunning (never
//    started) triggers a DIA_ASSERT.
TEST(DiaApplicationFlow_StateObject, StopWithoutStart_Asserts)
{
    EXPECT_DEATH(
    {
        StateObjectTestPU pu;
        MinimalModule mod(&pu, StringCRC("ModStopNoStart"));
        ReadyForStart(pu, mod);
        mod.Stop();   // Stop() while kNotRunning — asserts
    }, "");
}

// 6. IsStarted_BeforeStart_ReturnsFalse — HasStarted() is false before any
//    Start() call (in kConstructed and kNotRunning states).
TEST(DiaApplicationFlow_StateObject, IsStarted_BeforeStart_ReturnsFalse)
{
    StateObjectTestPU pu;
    MinimalModule mod(&pu, StringCRC("ModIsStartedPre"));

    EXPECT_FALSE(mod.HasStarted());  // kConstructed

    ReadyForStart(pu, mod);

    EXPECT_FALSE(mod.HasStarted());  // kNotRunning
}

// 7. IsStarted_AfterStart_ReturnsTrue — HasStarted() is true immediately
//    after a synchronous Start() call.
TEST(DiaApplicationFlow_StateObject, IsStarted_AfterStart_ReturnsTrue)
{
    StateObjectTestPU pu;
    MinimalModule mod(&pu, StringCRC("ModIsStartedPost"));

    ReadyForStart(pu, mod);
    mod.Start();

    EXPECT_TRUE(mod.HasStarted());
}

// 8. IsStopped_BeforeStop_ReturnsFalse — while running, GetState() is
//    kRunning, not kNotRunning.
TEST(DiaApplicationFlow_StateObject, IsStopped_BeforeStop_ReturnsFalse)
{
    StateObjectTestPU pu;
    MinimalModule mod(&pu, StringCRC("ModIsStoppedPre"));

    ReadyForStart(pu, mod);
    mod.Start();

    EXPECT_NE(mod.GetState(), StateObject::StateEnum::kNotRunning);
}

// 9. IsStopped_AfterStop_ReturnsTrue — after Stop(), state is kNotRunning.
TEST(DiaApplicationFlow_StateObject, IsStopped_AfterStop_ReturnsTrue)
{
    StateObjectTestPU pu;
    MinimalModule mod(&pu, StringCRC("ModIsStoppedPost"));

    ReadyForStart(pu, mod);
    mod.Start();
    mod.Stop();

    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kNotRunning);
}

// 10. FullLifecycle — complete Start -> Stop round-trip leaves the object in
//     kNotRunning with DoStart/DoStop each called exactly once.
TEST(DiaApplicationFlow_StateObject, FullLifecycle_CorrectFinalState)
{
    StateObjectTestPU pu;
    MinimalModule mod(&pu, StringCRC("ModFullCycle"));

    // Pre-conditions
    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kConstructed);
    EXPECT_FALSE(mod.HasStarted());

    ReadyForStart(pu, mod);
    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kNotRunning);

    mod.Start();
    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kRunning);
    EXPECT_TRUE(mod.HasStarted());
    EXPECT_EQ(mod.startCalls, 1);

    mod.Stop();
    EXPECT_EQ(mod.GetState(), StateObject::StateEnum::kNotRunning);
    EXPECT_FALSE(mod.HasStarted());
    EXPECT_EQ(mod.stopCalls, 1);
}
