#include <gtest/gtest.h>
#include <DiaApplication/ModuleRef.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>

using namespace Dia::Application;
using namespace Dia::Core;

//-----------------------------------------------------------------------------
// Test helpers
//-----------------------------------------------------------------------------

class ModuleRefTestPU : public ProcessingUnit
{
public:
	ModuleRefTestPU()
		: ProcessingUnit(StringCRC("ModuleRefPU"), -1.0f, 16, 16)
	{}

	virtual bool FlaggedToStopUpdating() const override { return true; }
};

class ModuleRefTestPhase : public Phase
{
public:
	ModuleRefTestPhase(ProcessingUnit* pu, const StringCRC& id)
		: Phase(pu, id)
	{}
	virtual bool FlaggedToStopUpdating() const override { return true; }
};

class TargetModule : public Module
{
public:
	static const StringCRC kTypeId;

	TargetModule(ProcessingUnit* pu)
		: Module(pu, kTypeId, RunningEnum::kUpdate)
	{}

	int GetValue() const { return 42; }

	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		return StateObject::OpertionResponse::kImmediate;
	}
	virtual void DoUpdate() override {}
	virtual void DoStop() override {}
};

const StringCRC TargetModule::kTypeId("TargetModule");

class OwnerModule : public Module
{
public:
	static const StringCRC kTypeId;

	OwnerModule(ProcessingUnit* pu)
		: Module(pu, kTypeId, RunningEnum::kUpdate)
		, mTargetRef(this)
	{}

	ModuleRef<TargetModule>& GetTargetRef() { return mTargetRef; }

	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		return StateObject::OpertionResponse::kImmediate;
	}
	virtual void DoUpdate() override {}
	virtual void DoStop() override {}

private:
	ModuleRef<TargetModule> mTargetRef;
};

const StringCRC OwnerModule::kTypeId("OwnerModule");

class UnregisteredModule : public Module
{
public:
	static const StringCRC kTypeId;

	UnregisteredModule(ProcessingUnit* pu)
		: Module(pu, kTypeId, RunningEnum::kUpdate)
	{}

	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		return StateObject::OpertionResponse::kImmediate;
	}
	virtual void DoUpdate() override {}
	virtual void DoStop() override {}
};

const StringCRC UnregisteredModule::kTypeId("UnregisteredModule");

//-----------------------------------------------------------------------------
// Tests
//-----------------------------------------------------------------------------

TEST(ModuleRef, ReturnsNullptrBeforeStart)
{
	ModuleRefTestPU pu;
	ModuleRefTestPhase phase(&pu, StringCRC("Phase1"));
	TargetModule target(&pu);
	OwnerModule owner(&pu);

	phase.AddModule(&target);
	phase.AddModule(&owner);

	pu.AddModule(&target);
	pu.AddModule(&owner);
	pu.Initialize();

	EXPECT_EQ(owner.GetTargetRef().Get(), nullptr);
}

TEST(ModuleRef, ReturnsPointerWhenRunning)
{
	ModuleRefTestPU pu;
	ModuleRefTestPhase phase(&pu, StringCRC("Phase1"));
	TargetModule target(&pu);
	OwnerModule owner(&pu);

	phase.AddModule(&target);
	phase.AddModule(&owner);

	pu.AddModule(&target);
	pu.AddModule(&owner);
	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	TargetModule* result = owner.GetTargetRef().Get();
	EXPECT_NE(result, nullptr);
	EXPECT_EQ(result, &target);
	EXPECT_EQ(result->GetValue(), 42);

	pu.Stop();
}

TEST(ModuleRef, ReturnsNullptrAfterStop)
{
	ModuleRefTestPU pu;
	ModuleRefTestPhase phase(&pu, StringCRC("Phase1"));
	TargetModule target(&pu);
	OwnerModule owner(&pu);

	phase.AddModule(&target);
	phase.AddModule(&owner);

	pu.AddModule(&target);
	pu.AddModule(&owner);
	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	EXPECT_NE(owner.GetTargetRef().Get(), nullptr);

	pu.Stop();

	EXPECT_EQ(owner.GetTargetRef().Get(), nullptr);
}

TEST(ModuleRef, IsRegisteredReturnsTrueWhenModuleAdded)
{
	ModuleRefTestPU pu;
	ModuleRefTestPhase phase(&pu, StringCRC("Phase1"));
	TargetModule target(&pu);
	OwnerModule owner(&pu);

	phase.AddModule(&target);
	phase.AddModule(&owner);

	pu.AddModule(&target);
	pu.AddModule(&owner);
	pu.Initialize();

	EXPECT_TRUE(owner.GetTargetRef().IsRegistered());
}

TEST(ModuleRef, IsRegisteredReturnsFalseWhenModuleNotAdded)
{
	ModuleRefTestPU pu;
	ModuleRefTestPhase phase(&pu, StringCRC("Phase1"));
	OwnerModule owner(&pu);

	phase.AddModule(&owner);

	pu.AddModule(&owner);
	pu.Initialize();

	EXPECT_FALSE(owner.GetTargetRef().IsRegistered());
}

TEST(ModuleRef, ReturnsNullptrForNonExistentModule)
{
	ModuleRefTestPU pu;
	ModuleRefTestPhase phase(&pu, StringCRC("Phase1"));
	OwnerModule owner(&pu);

	phase.AddModule(&owner);

	pu.AddModule(&owner);
	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	EXPECT_EQ(owner.GetTargetRef().Get(), nullptr);

	pu.Stop();
}

TEST(ModuleRef, CachesPointerAcrossCalls)
{
	ModuleRefTestPU pu;
	ModuleRefTestPhase phase(&pu, StringCRC("Phase1"));
	TargetModule target(&pu);
	OwnerModule owner(&pu);

	phase.AddModule(&target);
	phase.AddModule(&owner);

	pu.AddModule(&target);
	pu.AddModule(&owner);
	pu.Initialize();
	pu.SetInitialPhase(&phase);
	pu.Start();

	TargetModule* first = owner.GetTargetRef().Get();
	TargetModule* second = owner.GetTargetRef().Get();
	EXPECT_EQ(first, second);
	EXPECT_EQ(first, &target);

	pu.Stop();
}
