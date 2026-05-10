#include <gtest/gtest.h>
#include <DiaApplicationFlow/Introspection/ApplicationIntrospector.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>

using namespace Dia::Application;
using namespace Dia::Core;

class IntrospectorTestPU : public ProcessingUnit
{
public:
	IntrospectorTestPU()
		: ProcessingUnit(StringCRC("IntrospectorPU"), -1.0f, 16, 16)
		, mMaxUpdates(0)
		, mUpdateCount(0)
	{}

	void SetMaxUpdates(int n) { mMaxUpdates = n; mUpdateCount = 0; }
	virtual bool FlaggedToStopUpdating() const override
	{
		if (mMaxUpdates <= 0) return true;
		return (mUpdateCount++ >= mMaxUpdates);
	}

private:
	int mMaxUpdates;
	mutable int mUpdateCount;
};

class IntrospectorTestPhase : public Phase
{
public:
	IntrospectorTestPhase(ProcessingUnit* pu, const StringCRC& id)
		: Phase(pu, id)
	{}
	virtual bool FlaggedToStopUpdating() const override { return true; }
};

class IntrospectorTestModule : public Module
{
public:
	IntrospectorTestModule(ProcessingUnit* pu, const StringCRC& id, RunningEnum mode = RunningEnum::kUpdate)
		: Module(pu, id, mode)
	{}

	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		return StateObject::OpertionResponse::kImmediate;
	}
	virtual void DoUpdate() override {}
	virtual void DoStop() override {}
};

TEST(ApplicationIntrospector, GetPhasesEmptyWhenNoPhasesAdded)
{
	IntrospectorTestPU pu;
	ApplicationIntrospector introspector(&pu);

	auto phases = introspector.GetPhases();
	EXPECT_EQ(phases.Size(), 0u);
}

TEST(ApplicationIntrospector, GetPhasesReturnsAddedPhases)
{
	IntrospectorTestPU pu;
	IntrospectorTestPhase phase1(&pu, StringCRC("Phase1"));
	IntrospectorTestPhase phase2(&pu, StringCRC("Phase2"));
	ApplicationIntrospector introspector(&pu);

	auto phases = introspector.GetPhases();
	EXPECT_EQ(phases.Size(), 2u);
}

TEST(ApplicationIntrospector, GetModulesReturnsAutoRegisteredModules)
{
	IntrospectorTestPU pu;
	IntrospectorTestModule mod1(&pu, StringCRC("Mod1"));
	IntrospectorTestModule mod2(&pu, StringCRC("Mod2"));
	IntrospectorTestModule mod3(&pu, StringCRC("Mod3"));
	ApplicationIntrospector introspector(&pu);

	auto modules = introspector.GetModules();
	EXPECT_EQ(modules.Size(), 3u);
}

TEST(ApplicationIntrospector, GetCurrentPhaseReturnsNullBeforeStart)
{
	IntrospectorTestPU pu;
	IntrospectorTestPhase phase(&pu, StringCRC("Phase1"));
	IntrospectorTestModule mod(&pu, StringCRC("Mod1"));
	phase.AddModule(&mod);
	ApplicationIntrospector introspector(&pu);

	EXPECT_EQ(introspector.GetCurrentPhase(), nullptr);
}

TEST(ApplicationIntrospector, GetCurrentPhaseReturnsInitialPhaseAfterStart)
{
	IntrospectorTestPU pu;
	IntrospectorTestPhase phase(&pu, StringCRC("Phase1"));
	IntrospectorTestModule mod(&pu, StringCRC("Mod1"));
	phase.AddModule(&mod);

	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	ApplicationIntrospector introspector(&pu);
	EXPECT_EQ(introspector.GetCurrentPhase(), &phase);

	pu.Stop();
}

TEST(ApplicationIntrospector, GetModuleStateReturnsCorrectStates)
{
	IntrospectorTestPU pu;
	IntrospectorTestPhase phase(&pu, StringCRC("Phase1"));
	IntrospectorTestModule mod(&pu, StringCRC("Mod1"));
	phase.AddModule(&mod);
	ApplicationIntrospector introspector(&pu);

	EXPECT_EQ(introspector.GetModuleState(&mod), static_cast<int>(StateObject::StateEnum::kConstructed));

	pu.Initialize();
	EXPECT_EQ(introspector.GetModuleState(&mod), static_cast<int>(StateObject::StateEnum::kNotRunning));

	pu.SetInitialPhase(&phase);
	pu.Start();
	EXPECT_EQ(introspector.GetModuleState(&mod), static_cast<int>(StateObject::StateEnum::kRunning));

	pu.Stop();
	EXPECT_EQ(introspector.GetModuleState(&mod), static_cast<int>(StateObject::StateEnum::kNotRunning));
}

TEST(ApplicationIntrospector, GetPhaseStateReturnsCorrectStates)
{
	IntrospectorTestPU pu;
	IntrospectorTestPhase phase(&pu, StringCRC("Phase1"));
	IntrospectorTestModule mod(&pu, StringCRC("Mod1"));
	phase.AddModule(&mod);
	ApplicationIntrospector introspector(&pu);

	EXPECT_EQ(introspector.GetPhaseState(&phase), static_cast<int>(StateObject::StateEnum::kConstructed));

	pu.Initialize();
	EXPECT_EQ(introspector.GetPhaseState(&phase), static_cast<int>(StateObject::StateEnum::kNotRunning));

	pu.SetInitialPhase(&phase);
	pu.Start();
	EXPECT_EQ(introspector.GetPhaseState(&phase), static_cast<int>(StateObject::StateEnum::kRunning));

	pu.Stop();
	EXPECT_EQ(introspector.GetPhaseState(&phase), static_cast<int>(StateObject::StateEnum::kNotRunning));
}

TEST(ApplicationIntrospector, GetModuleDependenciesReturnsCorrectDeps)
{
	IntrospectorTestPU pu;
	IntrospectorTestPhase phase(&pu, StringCRC("Phase1"));
	IntrospectorTestModule modA(&pu, StringCRC("ModA"));
	IntrospectorTestModule modB(&pu, StringCRC("ModB"));
	IntrospectorTestModule modC(&pu, StringCRC("ModC"));

	modA.AddDependancy(&modB);
	modA.AddDependancy(&modC);

	phase.AddModule(&modA);
	phase.AddModule(&modB);
	phase.AddModule(&modC);

	ApplicationIntrospector introspector(&pu);

	auto depsA = introspector.GetModuleDependencies(&modA);
	EXPECT_EQ(depsA.Size(), 2u);

	auto depsB = introspector.GetModuleDependencies(&modB);
	EXPECT_EQ(depsB.Size(), 0u);
}

TEST(ApplicationIntrospector, GetModulePlacementsReturnsCorrectMapping)
{
	IntrospectorTestPU pu;
	IntrospectorTestPhase phase1(&pu, StringCRC("Phase1"));
	IntrospectorTestPhase phase2(&pu, StringCRC("Phase2"));
	IntrospectorTestModule modShared(&pu, StringCRC("SharedMod"));
	IntrospectorTestModule modOnly1(&pu, StringCRC("OnlyPhase1Mod"));

	phase1.AddModule(&modShared);
	phase1.AddModule(&modOnly1);
	phase2.AddModule(&modShared);

	ApplicationIntrospector introspector(&pu);

	auto placements = introspector.GetModulePlacements();
	EXPECT_EQ(placements.Size(), 2u);

	for (unsigned int i = 0; i < placements.Size(); ++i)
	{
		if (placements[i].module == &modShared)
		{
			EXPECT_EQ(placements[i].phases.Size(), 2u);
		}
		else if (placements[i].module == &modOnly1)
		{
			EXPECT_EQ(placements[i].phases.Size(), 1u);
		}
	}
}

TEST(ApplicationIntrospector, GetTransitionsReturnsAddedTransitions)
{
	IntrospectorTestPU pu;
	IntrospectorTestPhase phaseA(&pu, StringCRC("PhaseA"));
	IntrospectorTestPhase phaseB(&pu, StringCRC("PhaseB"));
	IntrospectorTestPhase phaseC(&pu, StringCRC("PhaseC"));

	pu.AddPhaseTransiton(&phaseA, &phaseB);
	pu.AddPhaseTransiton(&phaseA, &phaseC);
	pu.AddPhaseTransiton(&phaseB, &phaseC);

	ApplicationIntrospector introspector(&pu);

	auto transitions = introspector.GetTransitions();
	EXPECT_EQ(transitions.Size(), 3u);
}
