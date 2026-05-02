#include <gtest/gtest.h>

#include <DiaStateMachine/PushdownAutomaton.h>
#include <DiaStateMachine/PushdownAutomatonBuilder.h>

using namespace Dia::StateMachine;

struct PDAContext
{
	Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> callbackOrder;
};

static void PDA_Enter(void* ctx, const char* name)
{
	auto& c = *static_cast<PDAContext*>(ctx);
	c.callbackOrder.Add(Dia::Core::StringCRC(name));
}

static void PDA_EnterMain(void* ctx) { PDA_Enter(ctx, "EnterMain"); }
static void PDA_ExitMain(void* ctx) { PDA_Enter(ctx, "ExitMain"); }
static void PDA_PauseMain(void* ctx) { PDA_Enter(ctx, "PauseMain"); }
static void PDA_ResumeMain(void* ctx) { PDA_Enter(ctx, "ResumeMain"); }
static void PDA_EnterPause(void* ctx) { PDA_Enter(ctx, "EnterPause"); }
static void PDA_ExitPause(void* ctx) { PDA_Enter(ctx, "ExitPause"); }
static void PDA_EnterOptions(void* ctx) { PDA_Enter(ctx, "EnterOptions"); }
static void PDA_ExitOptions(void* ctx) { PDA_Enter(ctx, "ExitOptions"); }
static void PDA_PausePause(void* ctx) { PDA_Enter(ctx, "PausePause"); }
static void PDA_ResumePause(void* ctx) { PDA_Enter(ctx, "ResumePause"); }

static int sUpdateCount = 0;
static void PDA_UpdateMain(void* ctx, float dt)
{
	sUpdateCount++;
}

TEST(PushdownAutomaton, ConstructEntersInitialState)
{
	PDAContext ctx;
	auto def = PushdownAutomatonBuilder()
		.State(Dia::Core::StringCRC("MainMenu"))
			.OnEnter(PDA_EnterMain)
		.State(Dia::Core::StringCRC("PauseMenu"))
		.InitialState(Dia::Core::StringCRC("MainMenu"))
		.Build();

	PushdownAutomaton<PDAContext> pda(Dia::Core::StringCRC("TestPDA"),
		static_cast<PushdownAutomatonDefinition&&>(def), ctx);

	EXPECT_EQ(pda.GetCurrentStateId(), Dia::Core::StringCRC("MainMenu"));
	EXPECT_EQ(pda.GetStackDepth(), 1);
	ASSERT_EQ(ctx.callbackOrder.Size(), 1u);
	EXPECT_EQ(ctx.callbackOrder[0], Dia::Core::StringCRC("EnterMain"));
}

TEST(PushdownAutomaton, PushCallsPauseAndEnter)
{
	PDAContext ctx;
	auto def = PushdownAutomatonBuilder()
		.State(Dia::Core::StringCRC("MainMenu"))
			.OnPause(PDA_PauseMain)
		.State(Dia::Core::StringCRC("PauseMenu"))
			.OnEnter(PDA_EnterPause)
		.InitialState(Dia::Core::StringCRC("MainMenu"))
		.Build();

	PushdownAutomaton<PDAContext> pda(Dia::Core::StringCRC("TestPDA"),
		static_cast<PushdownAutomatonDefinition&&>(def), ctx);

	ctx.callbackOrder.RemoveAll();
	bool result = pda.Push(Dia::Core::StringCRC("PauseMenu"));
	EXPECT_TRUE(result);
	EXPECT_EQ(pda.GetCurrentStateId(), Dia::Core::StringCRC("PauseMenu"));
	EXPECT_EQ(pda.GetStackDepth(), 2);

	ASSERT_EQ(ctx.callbackOrder.Size(), 2u);
	EXPECT_EQ(ctx.callbackOrder[0], Dia::Core::StringCRC("PauseMain"));
	EXPECT_EQ(ctx.callbackOrder[1], Dia::Core::StringCRC("EnterPause"));
}

TEST(PushdownAutomaton, PopCallsExitAndResume)
{
	PDAContext ctx;
	auto def = PushdownAutomatonBuilder()
		.State(Dia::Core::StringCRC("MainMenu"))
			.OnResume(PDA_ResumeMain)
		.State(Dia::Core::StringCRC("PauseMenu"))
			.OnExit(PDA_ExitPause)
		.InitialState(Dia::Core::StringCRC("MainMenu"))
		.Build();

	PushdownAutomaton<PDAContext> pda(Dia::Core::StringCRC("TestPDA"),
		static_cast<PushdownAutomatonDefinition&&>(def), ctx);

	pda.Push(Dia::Core::StringCRC("PauseMenu"));
	ctx.callbackOrder.RemoveAll();

	bool result = pda.Pop();
	EXPECT_TRUE(result);
	EXPECT_EQ(pda.GetCurrentStateId(), Dia::Core::StringCRC("MainMenu"));
	EXPECT_EQ(pda.GetStackDepth(), 1);

	ASSERT_EQ(ctx.callbackOrder.Size(), 2u);
	EXPECT_EQ(ctx.callbackOrder[0], Dia::Core::StringCRC("ExitPause"));
	EXPECT_EQ(ctx.callbackOrder[1], Dia::Core::StringCRC("ResumeMain"));
}

TEST(PushdownAutomaton, PopAtDepthOneReturnsFalse)
{
	PDAContext ctx;
	auto def = PushdownAutomatonBuilder()
		.State(Dia::Core::StringCRC("MainMenu"))
		.InitialState(Dia::Core::StringCRC("MainMenu"))
		.Build();

	PushdownAutomaton<PDAContext> pda(Dia::Core::StringCRC("TestPDA"),
		static_cast<PushdownAutomatonDefinition&&>(def), ctx);

	EXPECT_FALSE(pda.Pop());
	EXPECT_EQ(pda.GetStackDepth(), 1);
}

TEST(PushdownAutomaton, OnlyTopOfStackReceivesUpdate)
{
	PDAContext ctx;
	sUpdateCount = 0;

	auto def = PushdownAutomatonBuilder()
		.State(Dia::Core::StringCRC("MainMenu"))
			.OnUpdate(PDA_UpdateMain)
		.State(Dia::Core::StringCRC("PauseMenu"))
		.InitialState(Dia::Core::StringCRC("MainMenu"))
		.Build();

	PushdownAutomaton<PDAContext> pda(Dia::Core::StringCRC("TestPDA"),
		static_cast<PushdownAutomatonDefinition&&>(def), ctx);

	pda.Update(0.016f);
	EXPECT_EQ(sUpdateCount, 1);

	pda.Push(Dia::Core::StringCRC("PauseMenu"));
	sUpdateCount = 0;
	pda.Update(0.016f);
	EXPECT_EQ(sUpdateCount, 0);
}

TEST(PushdownAutomaton, MultiLevelStack)
{
	PDAContext ctx;
	auto def = PushdownAutomatonBuilder()
		.State(Dia::Core::StringCRC("MainMenu"))
		.State(Dia::Core::StringCRC("PauseMenu"))
			.OnPause(PDA_PausePause)
		.State(Dia::Core::StringCRC("Options"))
			.OnEnter(PDA_EnterOptions)
		.InitialState(Dia::Core::StringCRC("MainMenu"))
		.Build();

	PushdownAutomaton<PDAContext> pda(Dia::Core::StringCRC("TestPDA"),
		static_cast<PushdownAutomatonDefinition&&>(def), ctx);

	pda.Push(Dia::Core::StringCRC("PauseMenu"));
	pda.Push(Dia::Core::StringCRC("Options"));
	EXPECT_EQ(pda.GetStackDepth(), 3);
	EXPECT_EQ(pda.GetCurrentStateId(), Dia::Core::StringCRC("Options"));

	pda.Pop();
	EXPECT_EQ(pda.GetCurrentStateId(), Dia::Core::StringCRC("PauseMenu"));

	pda.Pop();
	EXPECT_EQ(pda.GetCurrentStateId(), Dia::Core::StringCRC("MainMenu"));
}

TEST(PushdownAutomaton, PushPopRecordedInHistory)
{
	PDAContext ctx;
	auto def = PushdownAutomatonBuilder()
		.State(Dia::Core::StringCRC("MainMenu"))
		.State(Dia::Core::StringCRC("PauseMenu"))
		.InitialState(Dia::Core::StringCRC("MainMenu"))
		.Build();

	PushdownAutomaton<PDAContext> pda(Dia::Core::StringCRC("TestPDA"),
		static_cast<PushdownAutomatonDefinition&&>(def), ctx);

	pda.Push(Dia::Core::StringCRC("PauseMenu"));
	pda.Pop();

	Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32> history;
	pda.GetTransitionHistory(history);

	ASSERT_EQ(history.Size(), 2u);
	EXPECT_EQ(history[0].sourceStateId, Dia::Core::StringCRC("MainMenu"));
	EXPECT_EQ(history[0].targetStateId, Dia::Core::StringCRC("PauseMenu"));
	EXPECT_EQ(history[1].sourceStateId, Dia::Core::StringCRC("PauseMenu"));
	EXPECT_EQ(history[1].targetStateId, Dia::Core::StringCRC("MainMenu"));
}

TEST(PushdownAutomaton, PushSameStateTwice)
{
	PDAContext ctx;
	auto def = PushdownAutomatonBuilder()
		.State(Dia::Core::StringCRC("MainMenu"))
		.InitialState(Dia::Core::StringCRC("MainMenu"))
		.Build();

	PushdownAutomaton<PDAContext> pda(Dia::Core::StringCRC("TestPDA"),
		static_cast<PushdownAutomatonDefinition&&>(def), ctx);

	pda.Push(Dia::Core::StringCRC("MainMenu"));
	EXPECT_EQ(pda.GetStackDepth(), 2);
	EXPECT_EQ(pda.GetCurrentStateId(), Dia::Core::StringCRC("MainMenu"));
}
