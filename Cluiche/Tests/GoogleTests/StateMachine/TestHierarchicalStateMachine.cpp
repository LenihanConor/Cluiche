#include <gtest/gtest.h>

#include <DiaStateMachine/HierarchicalStateMachine.h>
#include <DiaStateMachine/HierarchicalStateMachineBuilder.h>

using namespace Dia::StateMachine;

struct HSMContext
{
	int enterCount = 0;
	int exitCount = 0;
	int updateCount = 0;
	Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> callbackOrder;
};

static void HSM_OnEnter(void* ctx, const char* name)
{
	auto& c = *static_cast<HSMContext*>(ctx);
	c.enterCount++;
	c.callbackOrder.Add(Dia::Core::StringCRC(name));
}

static void HSM_OnExit(void* ctx, const char* name)
{
	auto& c = *static_cast<HSMContext*>(ctx);
	c.exitCount++;
	c.callbackOrder.Add(Dia::Core::StringCRC(name));
}

static void EnterRoot(void* ctx) { HSM_OnEnter(ctx, "EnterRoot"); }
static void ExitRoot(void* ctx) { HSM_OnExit(ctx, "ExitRoot"); }
static void EnterAlive(void* ctx) { HSM_OnEnter(ctx, "EnterAlive"); }
static void ExitAlive(void* ctx) { HSM_OnExit(ctx, "ExitAlive"); }
static void EnterIdle(void* ctx) { HSM_OnEnter(ctx, "EnterIdle"); }
static void ExitIdle(void* ctx) { HSM_OnExit(ctx, "ExitIdle"); }
static void EnterWalking(void* ctx) { HSM_OnEnter(ctx, "EnterWalking"); }
static void ExitWalking(void* ctx) { HSM_OnExit(ctx, "ExitWalking"); }
static void EnterDead(void* ctx) { HSM_OnEnter(ctx, "EnterDead"); }
static void ExitDead(void* ctx) { HSM_OnExit(ctx, "ExitDead"); }
static void EnterRunning(void* ctx) { HSM_OnEnter(ctx, "EnterRunning"); }

static void UpdateIdle(void* ctx, float)
{
	auto& c = *static_cast<HSMContext*>(ctx);
	c.updateCount++;
}

static void UpdateAlive(void* ctx, float)
{
	auto& c = *static_cast<HSMContext*>(ctx);
	c.updateCount++;
}

// Hierarchy:
//   Root
//     Alive (initial child: Idle)
//       Idle
//       Walking
//       Running
//     Dead

TEST(HierarchicalStateMachine, ConstructEntersRootAndLeaf)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Alive"))
			.OnEnter(EnterRoot)
		.ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Idle"))
			.OnEnter(EnterAlive)
		.ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
			.OnEnter(EnterIdle)
		.ChildState(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Alive"))
			.OnEnter(EnterWalking)
		.ChildState(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Root"))
			.OnEnter(EnterDead)
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("Idle"));
	EXPECT_EQ(ctx.enterCount, 3);
	ASSERT_EQ(ctx.callbackOrder.Size(), 3u);
	EXPECT_EQ(ctx.callbackOrder[0], Dia::Core::StringCRC("EnterRoot"));
	EXPECT_EQ(ctx.callbackOrder[1], Dia::Core::StringCRC("EnterAlive"));
	EXPECT_EQ(ctx.callbackOrder[2], Dia::Core::StringCRC("EnterIdle"));
}

TEST(HierarchicalStateMachine, IsInStateWorksForAncestors)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Idle"))
		.ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Root"))
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("Idle")));
	EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("Alive")));
	EXPECT_TRUE(hsm.IsInState(Dia::Core::StringCRC("Root")));
	EXPECT_FALSE(hsm.IsInState(Dia::Core::StringCRC("Dead")));
}

TEST(HierarchicalStateMachine, InheritedTransitionFiresFromLeaf)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Idle"))
			.Transition(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Die"))
			.OnExit(ExitAlive)
		.ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
			.OnExit(ExitIdle)
		.ChildState(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Root"))
			.OnEnter(EnterDead)
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	ctx.callbackOrder.RemoveAll();
	bool result = hsm.Fire(Dia::Core::StringCRC("Die"));
	EXPECT_TRUE(result);
	EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("Dead"));
}

TEST(HierarchicalStateMachine, ChildTransitionOverridesParent)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Idle"))
			.Transition(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Go"))
		.ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Go"))
		.ChildState(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Root"))
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	hsm.Fire(Dia::Core::StringCRC("Go"));
	EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("Walking"));
}

TEST(HierarchicalStateMachine, CrossBranchExitEnterOrder)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Idle"))
			.OnEnter(EnterAlive).OnExit(ExitAlive)
		.ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
			.OnEnter(EnterIdle).OnExit(ExitIdle)
			.Transition(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Die"))
		.ChildState(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Root"))
			.OnEnter(EnterDead)
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	ctx.callbackOrder.RemoveAll();
	hsm.Fire(Dia::Core::StringCRC("Die"));

	ASSERT_GE(ctx.callbackOrder.Size(), 3u);
	EXPECT_EQ(ctx.callbackOrder[0], Dia::Core::StringCRC("ExitIdle"));
	EXPECT_EQ(ctx.callbackOrder[1], Dia::Core::StringCRC("ExitAlive"));
	EXPECT_EQ(ctx.callbackOrder[2], Dia::Core::StringCRC("EnterDead"));
}

TEST(HierarchicalStateMachine, ShallowHistoryResumesLastChild)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Idle"))
			.EnableHistory()
			.Transition(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Die"))
		.ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Walk"))
		.ChildState(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("Root"))
			.Transition(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Revive"))
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	hsm.Fire(Dia::Core::StringCRC("Walk"));
	EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("Walking"));

	hsm.Fire(Dia::Core::StringCRC("Die"));
	EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("Dead"));

	hsm.Fire(Dia::Core::StringCRC("Revive"));
	EXPECT_EQ(hsm.GetCurrentStateId(), Dia::Core::StringCRC("Walking"));
}

TEST(HierarchicalStateMachine, UpdateCallsAllActiveStates)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Idle"))
			.OnUpdate(UpdateAlive)
		.ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
			.OnUpdate(UpdateIdle)
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	hsm.Update(0.016f);
	EXPECT_EQ(ctx.updateCount, 2);
}

TEST(HierarchicalStateMachine, GetAllStatesReportsLeafAndParent)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Child"))
		.ChildState(Dia::Core::StringCRC("Child"), Dia::Core::StringCRC("Root"))
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	Dia::Core::Containers::DynamicArrayC<StateInfo, 64> states;
	hsm.GetAllStates(states);

	ASSERT_EQ(states.Size(), 2u);

	bool rootFound = false;
	bool childFound = false;
	for (unsigned int i = 0; i < states.Size(); ++i)
	{
		if (states[i].id == Dia::Core::StringCRC("Root"))
		{
			EXPECT_FALSE(states[i].isLeaf);
			EXPECT_TRUE(states[i].isActive);
			rootFound = true;
		}
		if (states[i].id == Dia::Core::StringCRC("Child"))
		{
			EXPECT_TRUE(states[i].isLeaf);
			EXPECT_TRUE(states[i].isActive);
			EXPECT_EQ(states[i].parentId, Dia::Core::StringCRC("Root"));
			childFound = true;
		}
	}
	EXPECT_TRUE(rootFound);
	EXPECT_TRUE(childFound);
}

TEST(HierarchicalStateMachine, TransitionHistoryRecorded)
{
	HSMContext ctx;
	auto def = HierarchicalStateMachineBuilder()
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Alive"))
		.ChildState(Dia::Core::StringCRC("Alive"), Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("Idle"))
		.ChildState(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("Alive"))
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Walk"))
		.ChildState(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Alive"))
		.InitialState(Dia::Core::StringCRC("Root"))
		.Build();

	HierarchicalStateMachine<HSMContext> hsm(Dia::Core::StringCRC("TestHSM"),
		static_cast<HierarchicalStateMachineDefinition&&>(def), ctx);

	hsm.Fire(Dia::Core::StringCRC("Walk"));

	Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32> history;
	hsm.GetTransitionHistory(history);

	ASSERT_EQ(history.Size(), 1u);
	EXPECT_EQ(history[0].sourceStateId, Dia::Core::StringCRC("Idle"));
	EXPECT_EQ(history[0].targetStateId, Dia::Core::StringCRC("Walking"));
}
