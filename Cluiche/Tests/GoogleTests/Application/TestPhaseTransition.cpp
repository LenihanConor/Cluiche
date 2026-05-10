#include <gtest/gtest.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>

using namespace Dia::Application;
using namespace Dia::Core;

//-----------------------------------------------------------------------------
// Test helpers
//-----------------------------------------------------------------------------

static int sGlobalStartCounter = 0;
static int sGlobalStopCounter = 0;

class TransitionTestPU : public ProcessingUnit
{
public:
	TransitionTestPU()
		: ProcessingUnit(StringCRC("TransitionTestPU"), -1.0f, 16, 16)
		, mUpdateCount(0)
		, mMaxUpdates(0)
	{}

	void SetMaxUpdates(int n) { mMaxUpdates = n; mUpdateCount = 0; }
	virtual bool FlaggedToStopUpdating() const override
	{
		if (mMaxUpdates <= 0) return true;
		return (mUpdateCount++ >= mMaxUpdates);
	}

private:
	mutable int mUpdateCount;
	int mMaxUpdates;
};

class TransitionTestPhase : public Phase
{
public:
	TransitionTestPhase(ProcessingUnit* pu, const StringCRC& id)
		: Phase(pu, id)
	{}
	virtual bool FlaggedToStopUpdating() const override { return true; }

	int beforeStartCalls = 0;
	int afterStartCalls = 0;
	int beforeStopCalls = 0;
	int afterStopCalls = 0;

	virtual void BeforeModulesStart() override { beforeStartCalls++; }
	virtual void AfterModulesStart() override { afterStartCalls++; }
	virtual void BeforeModulesStop() override { beforeStopCalls++; }
	virtual void AfterModulesStop() override { afterStopCalls++; }
};

class TransitionTrackingModule : public Module
{
public:
	TransitionTrackingModule(ProcessingUnit* pu, const StringCRC& id, RunningEnum mode = RunningEnum::kUpdate)
		: Module(pu, id, mode)
	{}

	int startOrder = -1;
	int stopOrder = -1;
	int updateCount = 0;
	bool retainCalled = false;
	const Phase* retainStartPhase = nullptr;
	const Phase* retainEndPhase = nullptr;

	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		startOrder = sGlobalStartCounter++;
		return StateObject::OpertionResponse::kImmediate;
	}

	virtual void DoUpdate() override
	{
		updateCount++;
	}

	virtual void DoStop() override
	{
		stopOrder = sGlobalStopCounter++;
	}

	virtual void DoRetainThroughTransition(const Phase* startPhase, const Phase* endPhase) override
	{
		retainCalled = true;
		retainStartPhase = startPhase;
		retainEndPhase = endPhase;
	}
};

//-----------------------------------------------------------------------------
// Tests: Phase Transition — Retained Modules
//-----------------------------------------------------------------------------

// Mirrors the CluicheTest crash: module in both phases is retained through transition
TEST(PhaseTransition, RetainedModuleNotStopped)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("BootPhase"));
	TransitionTestPhase phase2(&pu, StringCRC("BootStrapPhase"));

	TransitionTrackingModule sharedModule(&pu, StringCRC("SharedMod"));

	phase1.AddModule(&sharedModule);
	phase2.AddModule(&sharedModule);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	EXPECT_TRUE(sharedModule.HasStarted());
	EXPECT_EQ(sharedModule.stopOrder, -1);

	// Transition from phase1 to phase2
	phase1.TransitionTo(&phase2);

	// Module should still be running, not stopped
	EXPECT_TRUE(sharedModule.HasStarted());
	EXPECT_EQ(sharedModule.stopOrder, -1);
	EXPECT_TRUE(sharedModule.retainCalled);
	EXPECT_EQ(sharedModule.retainStartPhase, &phase1);
	EXPECT_EQ(sharedModule.retainEndPhase, &phase2);

	// Clean up by stopping phase2
	phase2.Stop();
}

// Mirrors CluicheTest: KernelModule retained + UIModule/LevelFactory started fresh
TEST(PhaseTransition, RetainedAndNewModulesCoexist)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("BootPhase"));
	TransitionTestPhase phase2(&pu, StringCRC("BootStrapPhase"));

	// Shared module (like KernelModule)
	TransitionTrackingModule kernelMod(&pu, StringCRC("KernelMod"));

	// New modules only in phase2 (like UIModule, LevelFactoryModule)
	TransitionTrackingModule uiMod(&pu, StringCRC("UIMod"));
	TransitionTrackingModule levelMod(&pu, StringCRC("LevelMod"), Module::RunningEnum::kIdle);

	phase1.AddModule(&kernelMod);

	phase2.AddModule(&kernelMod);
	phase2.AddModule(&uiMod);
	phase2.AddModule(&levelMod);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	EXPECT_TRUE(kernelMod.HasStarted());
	EXPECT_FALSE(uiMod.HasStarted());
	EXPECT_FALSE(levelMod.HasStarted());

	// Transition
	phase1.TransitionTo(&phase2);

	// All modules should now be started
	EXPECT_TRUE(kernelMod.HasStarted());
	EXPECT_TRUE(uiMod.HasStarted());
	EXPECT_TRUE(levelMod.HasStarted());

	// KernelMod should have been retained, not re-started
	EXPECT_TRUE(kernelMod.retainCalled);
	EXPECT_EQ(kernelMod.stopOrder, -1);

	// New modules should have been started fresh
	EXPECT_FALSE(uiMod.retainCalled);
	EXPECT_FALSE(levelMod.retainCalled);

	phase2.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Phase Transition — Stopped Modules
//-----------------------------------------------------------------------------

// Module only in phase1 should be stopped during transition
TEST(PhaseTransition, ExclusiveModuleStopped)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("Phase1"));
	TransitionTestPhase phase2(&pu, StringCRC("Phase2"));

	TransitionTrackingModule sharedMod(&pu, StringCRC("SharedMod"));
	TransitionTrackingModule exclusiveMod(&pu, StringCRC("ExclusiveMod"));

	phase1.AddModule(&sharedMod);
	phase1.AddModule(&exclusiveMod);

	phase2.AddModule(&sharedMod);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	EXPECT_TRUE(sharedMod.HasStarted());
	EXPECT_TRUE(exclusiveMod.HasStarted());

	phase1.TransitionTo(&phase2);

	EXPECT_TRUE(sharedMod.HasStarted());
	EXPECT_TRUE(sharedMod.retainCalled);

	// Exclusive module should have been stopped
	EXPECT_TRUE(exclusiveMod.stopOrder >= 0);
	EXPECT_FALSE(exclusiveMod.retainCalled);

	phase2.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Phase Transition — Module Dependencies
//-----------------------------------------------------------------------------

// New module with dependency on retained module should start correctly
TEST(PhaseTransition, NewModuleDependsOnRetainedModule)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("Phase1"));
	TransitionTestPhase phase2(&pu, StringCRC("Phase2"));

	TransitionTrackingModule baseMod(&pu, StringCRC("BaseMod"));
	TransitionTrackingModule dependentMod(&pu, StringCRC("DependentMod"));

	dependentMod.AddDependancy(&baseMod);

	phase1.AddModule(&baseMod);

	phase2.AddModule(&baseMod);
	phase2.AddModule(&dependentMod);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	EXPECT_TRUE(baseMod.HasStarted());
	EXPECT_FALSE(dependentMod.HasStarted());

	phase1.TransitionTo(&phase2);

	// Both should be running; dependent started after base was already running
	EXPECT_TRUE(baseMod.HasStarted());
	EXPECT_TRUE(dependentMod.HasStarted());

	phase2.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Phase Transition — Update Order Preserved
//-----------------------------------------------------------------------------

// Retained module should still be in the update list after transition
TEST(PhaseTransition, RetainedModuleUpdatesAfterTransition)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("Phase1"));
	TransitionTestPhase phase2(&pu, StringCRC("Phase2"));

	TransitionTrackingModule sharedMod(&pu, StringCRC("SharedMod"));
	TransitionTrackingModule newMod(&pu, StringCRC("NewMod"));

	phase1.AddModule(&sharedMod);

	phase2.AddModule(&sharedMod);
	phase2.AddModule(&newMod);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	// Update in phase1
	phase1.Update();
	EXPECT_EQ(sharedMod.updateCount, 1);
	EXPECT_EQ(newMod.updateCount, 0);

	phase1.TransitionTo(&phase2);

	// Update in phase2 — both modules should update
	phase2.Update();
	EXPECT_EQ(sharedMod.updateCount, 2);
	EXPECT_EQ(newMod.updateCount, 1);

	phase2.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Phase Transition — Lifecycle Hooks
//-----------------------------------------------------------------------------

// Phase hooks fire correctly during transition
TEST(PhaseTransition, PhaseHooksFireDuringTransition)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("Phase1"));
	TransitionTestPhase phase2(&pu, StringCRC("Phase2"));

	TransitionTrackingModule mod(&pu, StringCRC("Mod"));
	phase1.AddModule(&mod);
	phase2.AddModule(&mod);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	// Phase1 start hooks already fired
	EXPECT_EQ(phase1.beforeStartCalls, 1);
	EXPECT_EQ(phase1.afterStartCalls, 1);

	phase1.TransitionTo(&phase2);

	// Phase1 stop hooks should have fired during transition
	EXPECT_EQ(phase1.beforeStopCalls, 1);
	EXPECT_EQ(phase1.afterStopCalls, 1);

	// Phase2 start hooks should have fired
	EXPECT_EQ(phase2.beforeStartCalls, 1);
	EXPECT_EQ(phase2.afterStartCalls, 1);

	phase2.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Phase Transition — Old Phase State
//-----------------------------------------------------------------------------

// After transition, old phase should be in kNotRunning state
TEST(PhaseTransition, OldPhaseTransitionsToNotRunning)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("Phase1"));
	TransitionTestPhase phase2(&pu, StringCRC("Phase2"));

	TransitionTrackingModule mod(&pu, StringCRC("Mod"));
	phase1.AddModule(&mod);
	phase2.AddModule(&mod);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	EXPECT_EQ(phase1.GetState(), StateObject::StateEnum::kRunning);

	phase1.TransitionTo(&phase2);

	EXPECT_EQ(phase1.GetState(), StateObject::StateEnum::kNotRunning);
	EXPECT_EQ(phase2.GetState(), StateObject::StateEnum::kRunning);

	phase2.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Phase Transition — Empty Phases
//-----------------------------------------------------------------------------

// Transition between phases with no modules (edge case)
TEST(PhaseTransition, EmptyPhaseTransition)
{
	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("EmptyPhase1"));
	TransitionTestPhase phase2(&pu, StringCRC("EmptyPhase2"));

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	// Should not crash
	phase1.TransitionTo(&phase2);

	EXPECT_EQ(phase1.GetState(), StateObject::StateEnum::kNotRunning);
	EXPECT_EQ(phase2.GetState(), StateObject::StateEnum::kRunning);

	phase2.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Phase Transition — All Modules Replaced
//-----------------------------------------------------------------------------

// Transition where no modules are shared (all stopped, all new)
TEST(PhaseTransition, AllModulesReplaced)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("Phase1"));
	TransitionTestPhase phase2(&pu, StringCRC("Phase2"));

	TransitionTrackingModule oldMod(&pu, StringCRC("OldMod"));
	TransitionTrackingModule newMod(&pu, StringCRC("NewMod"));

	phase1.AddModule(&oldMod);
	phase2.AddModule(&newMod);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	phase1.TransitionTo(&phase2);

	EXPECT_TRUE(oldMod.stopOrder >= 0);
	EXPECT_FALSE(oldMod.retainCalled);

	EXPECT_TRUE(newMod.HasStarted());
	EXPECT_FALSE(newMod.retainCalled);

	phase2.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Phase Transition — Idle Module Retained
//-----------------------------------------------------------------------------

// Idle (non-updating) modules should still be retained correctly
TEST(PhaseTransition, IdleModuleRetained)
{
	sGlobalStartCounter = 0;
	sGlobalStopCounter = 0;

	TransitionTestPU pu;
	TransitionTestPhase phase1(&pu, StringCRC("Phase1"));
	TransitionTestPhase phase2(&pu, StringCRC("Phase2"));

	TransitionTrackingModule idleMod(&pu, StringCRC("IdleMod"), Module::RunningEnum::kIdle);

	phase1.AddModule(&idleMod);
	phase2.AddModule(&idleMod);

	pu.SetInitialPhase(&phase1);
	pu.AddPhaseTransiton(&phase1, &phase2);
	pu.Initialize();
	pu.Start();

	phase1.TransitionTo(&phase2);

	EXPECT_TRUE(idleMod.HasStarted());
	EXPECT_TRUE(idleMod.retainCalled);
	EXPECT_EQ(idleMod.stopOrder, -1);

	// Idle modules should not receive updates
	phase2.Update();
	EXPECT_EQ(idleMod.updateCount, 0);

	phase2.Stop();
}
