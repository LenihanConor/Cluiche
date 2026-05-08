#include <gtest/gtest.h>

#include <DiaStateMachine/FlatStateMachine.h>
#include <DiaStateMachine/StateMachineBuilder.h>

using namespace Dia::StateMachine;

struct TestContext
{
	int enterCount = 0;
	int exitCount = 0;
	int updateCount = 0;
	float lastDeltaTime = 0.0f;
	Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> callbackOrder;
};

static void OnEnterIdle(void* ctx)
{
	auto& c = *static_cast<TestContext*>(ctx);
	c.enterCount++;
	c.callbackOrder.Add(Dia::Core::StringCRC("EnterIdle"));
}

static void OnExitIdle(void* ctx)
{
	auto& c = *static_cast<TestContext*>(ctx);
	c.exitCount++;
	c.callbackOrder.Add(Dia::Core::StringCRC("ExitIdle"));
}

static void OnEnterWalking(void* ctx)
{
	auto& c = *static_cast<TestContext*>(ctx);
	c.enterCount++;
	c.callbackOrder.Add(Dia::Core::StringCRC("EnterWalking"));
}

static void OnExitWalking(void* ctx)
{
	auto& c = *static_cast<TestContext*>(ctx);
	c.exitCount++;
	c.callbackOrder.Add(Dia::Core::StringCRC("ExitWalking"));
}

static void OnEnterRunning(void* ctx)
{
	auto& c = *static_cast<TestContext*>(ctx);
	c.enterCount++;
	c.callbackOrder.Add(Dia::Core::StringCRC("EnterRunning"));
}

static void OnEnterDead(void* ctx)
{
	auto& c = *static_cast<TestContext*>(ctx);
	c.enterCount++;
	c.callbackOrder.Add(Dia::Core::StringCRC("EnterDead"));
}

static void OnUpdateIdle(void* ctx, float dt)
{
	auto& c = *static_cast<TestContext*>(ctx);
	c.updateCount++;
	c.lastDeltaTime = dt;
}

static bool AlwaysTrue(const void*) { return true; }
static bool AlwaysFalse(const void*) { return false; }

// AC1: Construct from definition + context, enters initial state, OnEnter called
TEST(FlatStateMachine, ConstructEntersInitialState)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle")).OnEnter(OnEnterIdle)
		.State(Dia::Core::StringCRC("Walking"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("TestMachine"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("Idle"));
	EXPECT_EQ(ctx.enterCount, 1);
}

// AC2: Fire triggers transition when trigger matches
TEST(FlatStateMachine, FireTransitionsToTarget)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
			.OnEnter(OnEnterIdle).OnExit(OnExitIdle)
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("StartWalk"))
		.State(Dia::Core::StringCRC("Walking"))
			.OnEnter(OnEnterWalking)
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("TestMachine"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	bool result = fsm.Fire(Dia::Core::StringCRC("StartWalk"));
	EXPECT_TRUE(result);
	EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("Walking"));
}

// AC3: Fire returns false when no matching transition
TEST(FlatStateMachine, FireReturnsFalseOnNoMatch)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
		.State(Dia::Core::StringCRC("Walking"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("TestMachine"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	bool result = fsm.Fire(Dia::Core::StringCRC("NonexistentTrigger"));
	EXPECT_FALSE(result);
	EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("Idle"));
}

// AC4: Fire calls OnExit then OnEnter in correct order
TEST(FlatStateMachine, FireCallsExitThenEnter)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
			.OnEnter(OnEnterIdle).OnExit(OnExitIdle)
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("StartWalk"))
		.State(Dia::Core::StringCRC("Walking"))
			.OnEnter(OnEnterWalking)
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("TestMachine"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	ctx.callbackOrder.RemoveAll();
	fsm.Fire(Dia::Core::StringCRC("StartWalk"));

	ASSERT_EQ(ctx.callbackOrder.Size(), 2u);
	EXPECT_EQ(ctx.callbackOrder[0], Dia::Core::StringCRC("ExitIdle"));
	EXPECT_EQ(ctx.callbackOrder[1], Dia::Core::StringCRC("EnterWalking"));
}

// AC5: Update calls current state's OnUpdate
TEST(FlatStateMachine, UpdateCallsOnUpdate)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle")).OnUpdate(OnUpdateIdle)
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("TestMachine"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	fsm.Update(0.016f);
	EXPECT_EQ(ctx.updateCount, 1);
	EXPECT_FLOAT_EQ(ctx.lastDeltaTime, 0.016f);
}

// AC7: Multiple transitions same trigger — first guard that passes wins
TEST(FlatStateMachine, GuardsFirstMatchWins)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("Go"))
				.Guard(AlwaysFalse)
			.Transition(Dia::Core::StringCRC("Running"), Dia::Core::StringCRC("Go"))
				.Guard(AlwaysTrue)
		.State(Dia::Core::StringCRC("Walking"))
		.State(Dia::Core::StringCRC("Running"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("TestMachine"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	fsm.Fire(Dia::Core::StringCRC("Go"));
	EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("Running"));
}

// AC8: Wildcard transition fires from any state
TEST(FlatStateMachine, WildcardTransitionFiresFromAnyState)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("StartWalk"))
		.State(Dia::Core::StringCRC("Walking"))
			.Transition(Dia::Core::StringCRC("Running"), Dia::Core::StringCRC("StartRun"))
		.State(Dia::Core::StringCRC("Running"))
		.State(Dia::Core::StringCRC("Dead")).OnEnter(OnEnterDead)
		.State(Dia::Core::StringCRC(kAnyState))
			.Transition(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("OnDie"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	// From Idle
	{
		TestContext c;
		auto d = def.Clone();
		FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("Test"),
			static_cast<StateMachineDefinition&&>(d), c);
		EXPECT_TRUE(fsm.Fire(Dia::Core::StringCRC("OnDie")));
		EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("Dead"));
	}

	// From Walking
	{
		TestContext c;
		auto d = def.Clone();
		FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("Test"),
			static_cast<StateMachineDefinition&&>(d), c);
		fsm.Fire(Dia::Core::StringCRC("StartWalk"));
		EXPECT_TRUE(fsm.Fire(Dia::Core::StringCRC("OnDie")));
		EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("Dead"));
	}
}

// AC9: State-specific transition takes priority over wildcard
TEST(FlatStateMachine, StateSpecificBeatsWildcard)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("OnDie"))
		.State(Dia::Core::StringCRC("Walking"))
		.State(Dia::Core::StringCRC("Dead"))
		.State(Dia::Core::StringCRC(kAnyState))
			.Transition(Dia::Core::StringCRC("Dead"), Dia::Core::StringCRC("OnDie"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("Test"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	fsm.Fire(Dia::Core::StringCRC("OnDie"));
	EXPECT_EQ(fsm.GetCurrentStateId(), Dia::Core::StringCRC("Walking"));
}

// AC11: IsInState returns correct value
TEST(FlatStateMachine, IsInStateCorrect)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
		.State(Dia::Core::StringCRC("Walking"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("Test"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	EXPECT_TRUE(fsm.IsInState(Dia::Core::StringCRC("Idle")));
	EXPECT_FALSE(fsm.IsInState(Dia::Core::StringCRC("Walking")));
}

// AC12: GetCurrentStateId via IStateMachineInspectable
TEST(FlatStateMachine, GetMachineIdCorrect)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("EnemyAI"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	EXPECT_EQ(fsm.GetMachineId(), Dia::Core::StringCRC("EnemyAI"));
}

// AC13: Transition history populated
TEST(FlatStateMachine, TransitionHistoryRecorded)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
			.Transition(Dia::Core::StringCRC("Walking"), Dia::Core::StringCRC("StartWalk"))
		.State(Dia::Core::StringCRC("Walking"))
			.Transition(Dia::Core::StringCRC("Running"), Dia::Core::StringCRC("StartRun"))
		.State(Dia::Core::StringCRC("Running"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("Test"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	fsm.Update(0.016f);
	fsm.Fire(Dia::Core::StringCRC("StartWalk"));
	fsm.Update(0.016f);
	fsm.Fire(Dia::Core::StringCRC("StartRun"));

	Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32> history;
	fsm.GetTransitionHistory(history);

	ASSERT_EQ(history.Size(), 2u);
	EXPECT_EQ(history[0].sourceStateId, Dia::Core::StringCRC("Idle"));
	EXPECT_EQ(history[0].targetStateId, Dia::Core::StringCRC("Walking"));
	EXPECT_EQ(history[1].sourceStateId, Dia::Core::StringCRC("Walking"));
	EXPECT_EQ(history[1].targetStateId, Dia::Core::StringCRC("Running"));
}

// Inspection: GetAllStates
TEST(FlatStateMachine, GetAllStatesReturnsCorrectInfo)
{
	TestContext ctx;
	auto def = StateMachineBuilder()
		.State(Dia::Core::StringCRC("Idle"))
		.State(Dia::Core::StringCRC("Walking"))
		.State(Dia::Core::StringCRC("Running"))
		.InitialState(Dia::Core::StringCRC("Idle"))
		.Build();

	FlatStateMachine<TestContext> fsm(Dia::Core::StringCRC("Test"),
		static_cast<StateMachineDefinition&&>(def), ctx);

	Dia::Core::Containers::DynamicArrayC<StateInfo, 64> states;
	fsm.GetAllStates(states);

	ASSERT_EQ(states.Size(), 3u);

	bool idleActive = false;
	for (unsigned int i = 0; i < states.Size(); ++i)
	{
		EXPECT_TRUE(states[i].isLeaf);
		if (states[i].id == Dia::Core::StringCRC("Idle"))
		{
			EXPECT_TRUE(states[i].isActive);
			idleActive = true;
		}
		else
		{
			EXPECT_FALSE(states[i].isActive);
		}
	}
	EXPECT_TRUE(idleActive);
}
