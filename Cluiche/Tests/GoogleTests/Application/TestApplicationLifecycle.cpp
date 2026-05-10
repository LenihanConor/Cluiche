#include <gtest/gtest.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>

using namespace Dia::Application;
using namespace Dia::Core;

//-----------------------------------------------------------------------------
// Test helpers
//-----------------------------------------------------------------------------

class LifecycleTestPU : public ProcessingUnit
{
public:
	LifecycleTestPU()
		: ProcessingUnit(StringCRC("LifecyclePU"), -1.0f, 16, 16)
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

class LifecycleTestPhase : public Phase
{
public:
	LifecycleTestPhase(ProcessingUnit* pu, const StringCRC& id)
		: Phase(pu, id)
	{}
	virtual bool FlaggedToStopUpdating() const override { return true; }

	int beforeStartCalls = 0;
	int afterStartCalls = 0;
	int beforeStopCalls = 0;
	int afterStopCalls = 0;
	int beforeUpdateCalls = 0;
	int afterUpdateCalls = 0;

	virtual void BeforeModulesStart() override { beforeStartCalls++; }
	virtual void AfterModulesStart() override { afterStartCalls++; }
	virtual void BeforeModulesStop() override { beforeStopCalls++; }
	virtual void AfterModulesStop() override { afterStopCalls++; }
	virtual void BeforeModulesUpdate() override { beforeUpdateCalls++; }
	virtual void AfterModulesUpdate() override { afterUpdateCalls++; }
};

class TrackingModule : public Module
{
public:
	TrackingModule(ProcessingUnit* pu, const StringCRC& id, RunningEnum mode = RunningEnum::kUpdate)
		: Module(pu, id, mode)
	{}

	int startOrder = -1;
	int updateCount = 0;
	bool stopped = false;

	static int sStartCounter;

	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		startOrder = sStartCounter++;
		return StateObject::OpertionResponse::kImmediate;
	}

	virtual void DoUpdate() override
	{
		updateCount++;
	}

	virtual void DoStop() override
	{
		stopped = true;
	}
};

int TrackingModule::sStartCounter = 0;

//-----------------------------------------------------------------------------
// Tests
//-----------------------------------------------------------------------------

TEST(ApplicationLifecycle, InitializeTransitionsState)
{
	LifecycleTestPU pu;
	LifecycleTestPhase phase(&pu, StringCRC("Phase1"));
	TrackingModule module(&pu, StringCRC("Mod1"));
	phase.AddModule(&module);

	EXPECT_EQ(module.GetState(), StateObject::StateEnum::kConstructed);

	pu.Initialize();

	EXPECT_EQ(module.GetState(), StateObject::StateEnum::kNotRunning);
	EXPECT_EQ(phase.GetState(), StateObject::StateEnum::kNotRunning);
}

TEST(ApplicationLifecycle, StartAndStopProcessingUnit)
{
	LifecycleTestPU pu;
	LifecycleTestPhase phase(&pu, StringCRC("Phase1"));
	TrackingModule module(&pu, StringCRC("Mod1"));
	phase.AddModule(&module);

	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	EXPECT_TRUE(module.HasStarted());

	pu.Stop();

	EXPECT_TRUE(module.stopped);
	EXPECT_EQ(module.GetState(), StateObject::StateEnum::kNotRunning);
}

TEST(ApplicationLifecycle, ModuleUpdateCalled)
{
	LifecycleTestPU pu;
	LifecycleTestPhase phase(&pu, StringCRC("Phase1"));
	TrackingModule module(&pu, StringCRC("Mod1"));
	phase.AddModule(&module);

	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	EXPECT_EQ(module.updateCount, 0);

	pu.SetMaxUpdates(1);
	pu.Update();

	EXPECT_EQ(module.updateCount, 1);

	pu.Stop();
}

TEST(ApplicationLifecycle, ModuleIdleSkipsUpdate)
{
	LifecycleTestPU pu;
	LifecycleTestPhase phase(&pu, StringCRC("Phase1"));
	TrackingModule idleModule(&pu, StringCRC("IdleMod"), Module::RunningEnum::kIdle);
	TrackingModule updateModule(&pu, StringCRC("UpdateMod"), Module::RunningEnum::kUpdate);
	phase.AddModule(&idleModule);
	phase.AddModule(&updateModule);

	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	pu.SetMaxUpdates(1);
	pu.Update();

	EXPECT_EQ(idleModule.updateCount, 0);
	EXPECT_EQ(updateModule.updateCount, 1);

	pu.Stop();
}

TEST(ApplicationLifecycle, ModuleDependencyOrder)
{
	TrackingModule::sStartCounter = 0;

	LifecycleTestPU pu;
	LifecycleTestPhase phase(&pu, StringCRC("Phase1"));

	TrackingModule modA(&pu, StringCRC("ModA"));
	TrackingModule modB(&pu, StringCRC("ModB"));

	modA.AddDependancy(&modB);

	phase.AddModule(&modA);
	phase.AddModule(&modB);

	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	EXPECT_LT(modB.startOrder, modA.startOrder);

	pu.Stop();
}

TEST(ApplicationLifecycle, HasAllDependanciesStarted)
{
	LifecycleTestPU pu;
	LifecycleTestPhase phase(&pu, StringCRC("Phase1"));

	TrackingModule modA(&pu, StringCRC("ModA"));
	TrackingModule modB(&pu, StringCRC("ModB"));

	modA.AddDependancy(&modB);
	phase.AddModule(&modA);
	phase.AddModule(&modB);

	pu.Initialize();

	EXPECT_FALSE(modA.HasAllDependanciesStarted());

	pu.SetInitialPhase(&phase);
	pu.Start();

	EXPECT_TRUE(modA.HasAllDependanciesStarted());

	pu.Stop();
}

TEST(ApplicationLifecycle, PhaseLifecycleHooks)
{
	LifecycleTestPU pu;
	LifecycleTestPhase phase(&pu, StringCRC("Phase1"));
	TrackingModule module(&pu, StringCRC("Mod1"));
	phase.AddModule(&module);

	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	EXPECT_EQ(phase.beforeStartCalls, 1);
	EXPECT_EQ(phase.afterStartCalls, 1);

	pu.SetMaxUpdates(1);
	pu.Update();

	EXPECT_EQ(phase.beforeUpdateCalls, 1);
	EXPECT_EQ(phase.afterUpdateCalls, 1);

	pu.Stop();

	EXPECT_EQ(phase.beforeStopCalls, 1);
	EXPECT_EQ(phase.afterStopCalls, 1);
}

static ErrorInfo sReceivedError;
static bool sCallbackFired = false;

static void TestErrorCallback(const ErrorInfo& error)
{
	sReceivedError = error;
	sCallbackFired = true;
}

TEST(ApplicationLifecycle, ErrorCallbackFires)
{
	sCallbackFired = false;
	sReceivedError = ErrorInfo();

	LifecycleTestPU pu;
	pu.SetErrorCallback(TestErrorCallback);

	ErrorInfo testError(ErrorCode::kStartupFailed, StringCRC("TestModule"), "test error");

	pu.ReportError(testError);

	EXPECT_TRUE(sCallbackFired);
	EXPECT_TRUE(sReceivedError.IsFailure());
	EXPECT_STREQ(sReceivedError.message, "test error");
	EXPECT_EQ(pu.GetErrorHistory().size(), 1u);
}
