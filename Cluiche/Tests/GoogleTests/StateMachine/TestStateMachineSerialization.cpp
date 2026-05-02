#include <gtest/gtest.h>

#include <DiaStateMachine/StateMachineDefinition.h>
#include <DiaStateMachine/StateMachineBuilder.h>
#include <DiaStateMachine/HierarchicalStateMachineDefinition.h>
#include <DiaStateMachine/HierarchicalStateMachineBuilder.h>
#include <DiaStateMachine/PushdownAutomatonDefinition.h>
#include <DiaStateMachine/PushdownAutomatonBuilder.h>
#include <DiaStateMachine/StateMachineMetadata.h>
#include <DiaStateMachine/IStateMachineSerializer.h>
#include <DiaStateMachine/JsonStateMachineSerializer.h>
#include <DiaStateMachine/CallbackRegistry.h>

using namespace Dia::StateMachine;

// ---------------------------------------------------------------------------
// Test callbacks
// ---------------------------------------------------------------------------

static void OnEnterA(void*) {}
static void OnExitA(void*)  {}
static void OnUpdateA(void*, float) {}
static void OnEnterB(void*) {}
static bool GuardAB(const void*) { return true; }
static void OnPauseA(void*)  {}
static void OnResumeA(void*) {}

static CallbackRegistry MakeRegistry()
{
	CallbackRegistry reg;
	reg.RegisterAction(Dia::Core::StringCRC("OnEnterA"),  OnEnterA);
	reg.RegisterAction(Dia::Core::StringCRC("OnExitA"),   OnExitA);
	reg.RegisterUpdate(Dia::Core::StringCRC("OnUpdateA"), OnUpdateA);
	reg.RegisterAction(Dia::Core::StringCRC("OnEnterB"),  OnEnterB);
	reg.RegisterGuard(Dia::Core::StringCRC("GuardAB"),    GuardAB);
	reg.RegisterAction(Dia::Core::StringCRC("OnPauseA"),  OnPauseA);
	reg.RegisterAction(Dia::Core::StringCRC("OnResumeA"), OnResumeA);
	return reg;
}

// ---------------------------------------------------------------------------
// StateMachineMetadata tests
// ---------------------------------------------------------------------------

TEST(StateMachineMetadata, SetAndFindBool)
{
	MetadataArray arr;
	SetMetadata(arr, Dia::Core::StringCRC("flag"), MetadataValue::FromBool(true));
	const MetadataValue* v = FindMetadata(arr, Dia::Core::StringCRC("flag"));
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->type, MetadataValue::kBool);
	EXPECT_TRUE(v->boolVal);
}

TEST(StateMachineMetadata, SetAndFindInt)
{
	MetadataArray arr;
	SetMetadata(arr, Dia::Core::StringCRC("count"), MetadataValue::FromInt(42));
	const MetadataValue* v = FindMetadata(arr, Dia::Core::StringCRC("count"));
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->type, MetadataValue::kInt);
	EXPECT_EQ(v->intVal, 42);
}

TEST(StateMachineMetadata, SetAndFindFloat)
{
	MetadataArray arr;
	SetMetadata(arr, Dia::Core::StringCRC("speed"), MetadataValue::FromFloat(1.5f));
	const MetadataValue* v = FindMetadata(arr, Dia::Core::StringCRC("speed"));
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->type, MetadataValue::kFloat);
	EXPECT_FLOAT_EQ(v->floatVal, 1.5f);
}

TEST(StateMachineMetadata, SetAndFindString)
{
	MetadataArray arr;
	SetMetadata(arr, Dia::Core::StringCRC("clip"), MetadataValue::FromString("idle_anim"));
	const MetadataValue* v = FindMetadata(arr, Dia::Core::StringCRC("clip"));
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->type, MetadataValue::kString);
	EXPECT_STREQ(v->stringVal.AsChar(), "idle_anim");
}

TEST(StateMachineMetadata, UpdateExistingKey)
{
	MetadataArray arr;
	SetMetadata(arr, Dia::Core::StringCRC("x"), MetadataValue::FromInt(1));
	SetMetadata(arr, Dia::Core::StringCRC("x"), MetadataValue::FromInt(99));
	EXPECT_EQ(arr.Size(), 1u);
	EXPECT_EQ(FindMetadata(arr, Dia::Core::StringCRC("x"))->intVal, 99);
}

TEST(StateMachineMetadata, MissingKeyReturnsNull)
{
	MetadataArray arr;
	EXPECT_EQ(FindMetadata(arr, Dia::Core::StringCRC("missing")), nullptr);
}

// ---------------------------------------------------------------------------
// Builder — callback names + metadata
// ---------------------------------------------------------------------------

TEST(StateMachineBuilder, CallbackNamesStoredInDef)
{
	StateMachineDefinition def = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
			.OnEnter(OnEnterA, Dia::Core::StringCRC("OnEnterA"))
			.OnExit(OnExitA,   Dia::Core::StringCRC("OnExitA"))
			.OnUpdate(OnUpdateA, Dia::Core::StringCRC("OnUpdateA"))
		.State(Dia::Core::StringCRC("B"))
		.Build();

	const auto& states = def.GetStates();
	EXPECT_EQ(states[0].onEnterName,  Dia::Core::StringCRC("OnEnterA"));
	EXPECT_EQ(states[0].onExitName,   Dia::Core::StringCRC("OnExitA"));
	EXPECT_EQ(states[0].onUpdateName, Dia::Core::StringCRC("OnUpdateA"));
}

TEST(StateMachineBuilder, StateAndMachineMetadata)
{
	StateMachineDefinition def = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
			.StateMetadata(Dia::Core::StringCRC("clip"), MetadataValue::FromString("idle"))
			.StateMetadata(Dia::Core::StringCRC("loop"), MetadataValue::FromBool(true))
		.MachineMetadata(Dia::Core::StringCRC("layer"), MetadataValue::FromInt(0))
		.State(Dia::Core::StringCRC("B"))
		.Build();

	const auto& states = def.GetStates();
	const MetadataValue* clip = FindMetadata(states[0].metadata, Dia::Core::StringCRC("clip"));
	ASSERT_NE(clip, nullptr);
	EXPECT_STREQ(clip->stringVal.AsChar(), "idle");

	const MetadataValue* layer = FindMetadata(def.GetMetadata(), Dia::Core::StringCRC("layer"));
	ASSERT_NE(layer, nullptr);
	EXPECT_EQ(layer->intVal, 0);
}

// ---------------------------------------------------------------------------
// JsonStateMachineSerializer — FlatStateMachine round-trip
// ---------------------------------------------------------------------------

TEST(JsonStateMachineSerializer, FlatRoundTrip)
{
	StateMachineDefinition original = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
			.OnEnter(OnEnterA, Dia::Core::StringCRC("OnEnterA"))
			.OnExit(OnExitA,   Dia::Core::StringCRC("OnExitA"))
			.StateMetadata(Dia::Core::StringCRC("clip"), MetadataValue::FromString("idle"))
			.Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("go"))
				.Guard(GuardAB, Dia::Core::StringCRC("GuardAB"))
		.State(Dia::Core::StringCRC("B"))
			.OnEnter(OnEnterB, Dia::Core::StringCRC("OnEnterB"))
		.MachineMetadata(Dia::Core::StringCRC("layer"), MetadataValue::FromInt(2))
		.Build();

	char buffer[4096] = {};
	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.Save(original, buffer, sizeof(buffer)));

	CallbackRegistry reg = MakeRegistry();
	StateMachineDefinition loaded;
	ASSERT_TRUE(serializer.Load(loaded, reg, buffer));

	ASSERT_EQ(loaded.GetStates().Size(), 2u);
	EXPECT_EQ(loaded.GetStates()[0].id, Dia::Core::StringCRC("A"));
	EXPECT_EQ(loaded.GetStates()[1].id, Dia::Core::StringCRC("B"));
	EXPECT_EQ(loaded.GetInitialStateId(), Dia::Core::StringCRC("A"));

	EXPECT_EQ(loaded.GetStates()[0].onEnterName, Dia::Core::StringCRC("OnEnterA"));
	EXPECT_EQ(loaded.GetStates()[0].onEnter, OnEnterA);
	EXPECT_EQ(loaded.GetStates()[0].onExitName,  Dia::Core::StringCRC("OnExitA"));
	EXPECT_EQ(loaded.GetStates()[0].onExit, OnExitA);

	ASSERT_EQ(loaded.GetTransitions().Size(), 1u);
	EXPECT_EQ(loaded.GetTransitions()[0].sourceStateId, Dia::Core::StringCRC("A"));
	EXPECT_EQ(loaded.GetTransitions()[0].targetStateId, Dia::Core::StringCRC("B"));
	EXPECT_EQ(loaded.GetTransitions()[0].guardName,     Dia::Core::StringCRC("GuardAB"));
	EXPECT_EQ(loaded.GetTransitions()[0].guard, GuardAB);

	const MetadataValue* clip = FindMetadata(loaded.GetStates()[0].metadata, Dia::Core::StringCRC("clip"));
	ASSERT_NE(clip, nullptr);
	EXPECT_STREQ(clip->stringVal.AsChar(), "idle");

	const MetadataValue* layer = FindMetadata(loaded.GetMetadata(), Dia::Core::StringCRC("layer"));
	ASSERT_NE(layer, nullptr);
	EXPECT_EQ(layer->intVal, 2);
}

TEST(JsonStateMachineSerializer, FlatVersionMismatchReturnsFalse)
{
	const char* badJson = R"({"version":"99.0","type":"FlatStateMachine","initialState":"A","states":[],"transitions":[]})";
	CallbackRegistry reg = MakeRegistry();
	StateMachineDefinition def;
	JsonStateMachineSerializer serializer;
	EXPECT_FALSE(serializer.Load(def, reg, badJson));
}

TEST(JsonStateMachineSerializer, FlatTypeMismatchReturnsFalse)
{
	const char* badJson = R"({"version":"1.0","type":"WrongType","initialState":"A","states":[],"transitions":[]})";
	CallbackRegistry reg = MakeRegistry();
	StateMachineDefinition def;
	JsonStateMachineSerializer serializer;
	EXPECT_FALSE(serializer.Load(def, reg, badJson));
}

// ---------------------------------------------------------------------------
// JsonStateMachineSerializer — HierarchicalStateMachine round-trip
// ---------------------------------------------------------------------------

TEST(JsonStateMachineSerializer, HsmRoundTrip)
{
	HierarchicalStateMachineDefinition original = HierarchicalStateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("Root"))
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("A"))
		.ChildState(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Root"))
			.OnEnter(OnEnterA, Dia::Core::StringCRC("OnEnterA"))
			.EnableHistory()
			.StateMetadata(Dia::Core::StringCRC("priority"), MetadataValue::FromInt(1))
		.ChildState(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Root"))
			.OnEnter(OnEnterB, Dia::Core::StringCRC("OnEnterB"))
		.Build();

	char buffer[4096] = {};
	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.Save(original, buffer, sizeof(buffer)));

	CallbackRegistry reg = MakeRegistry();
	HierarchicalStateMachineDefinition loaded;
	ASSERT_TRUE(serializer.Load(loaded, reg, buffer));

	ASSERT_EQ(loaded.GetStates().Size(), 3u);
	EXPECT_EQ(loaded.GetInitialStateId(), Dia::Core::StringCRC("Root"));

	const auto& states = loaded.GetStates();
	int aIdx = -1;
	for (unsigned int i = 0; i < states.Size(); ++i)
	{
		if (states[i].id == Dia::Core::StringCRC("A"))
		{
			aIdx = static_cast<int>(i);
			break;
		}
	}
	ASSERT_GE(aIdx, 0);
	EXPECT_EQ(states[aIdx].parentId, Dia::Core::StringCRC("Root"));
	EXPECT_TRUE(states[aIdx].hasHistory);
	EXPECT_EQ(states[aIdx].onEnterName, Dia::Core::StringCRC("OnEnterA"));
	EXPECT_EQ(states[aIdx].onEnter, OnEnterA);

	const MetadataValue* priority = FindMetadata(states[aIdx].metadata, Dia::Core::StringCRC("priority"));
	ASSERT_NE(priority, nullptr);
	EXPECT_EQ(priority->intVal, 1);
}

// ---------------------------------------------------------------------------
// JsonStateMachineSerializer — PushdownAutomaton round-trip
// ---------------------------------------------------------------------------

TEST(JsonStateMachineSerializer, PdaRoundTrip)
{
	PushdownAutomatonDefinition original = PushdownAutomatonBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
			.OnEnter(OnEnterA,   Dia::Core::StringCRC("OnEnterA"))
			.OnExit(OnExitA,     Dia::Core::StringCRC("OnExitA"))
			.OnPause(OnPauseA,   Dia::Core::StringCRC("OnPauseA"))
			.OnResume(OnResumeA, Dia::Core::StringCRC("OnResumeA"))
			.StateMetadata(Dia::Core::StringCRC("depth"), MetadataValue::FromFloat(0.5f))
		.MachineMetadata(Dia::Core::StringCRC("mode"), MetadataValue::FromString("stack"))
		.Build();

	char buffer[4096] = {};
	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.Save(original, buffer, sizeof(buffer)));

	CallbackRegistry reg = MakeRegistry();
	PushdownAutomatonDefinition loaded;
	ASSERT_TRUE(serializer.Load(loaded, reg, buffer));

	ASSERT_EQ(loaded.GetStates().Size(), 1u);
	EXPECT_EQ(loaded.GetInitialStateId(), Dia::Core::StringCRC("A"));

	const PushdownStateDef& s = loaded.GetStates()[0];
	EXPECT_EQ(s.onEnterName,  Dia::Core::StringCRC("OnEnterA"));
	EXPECT_EQ(s.onEnter, OnEnterA);
	EXPECT_EQ(s.onPauseName,  Dia::Core::StringCRC("OnPauseA"));
	EXPECT_EQ(s.onPause, OnPauseA);
	EXPECT_EQ(s.onResumeName, Dia::Core::StringCRC("OnResumeA"));
	EXPECT_EQ(s.onResume, OnResumeA);

	const MetadataValue* depth = FindMetadata(s.metadata, Dia::Core::StringCRC("depth"));
	ASSERT_NE(depth, nullptr);
	EXPECT_FLOAT_EQ(depth->floatVal, 0.5f);

	const MetadataValue* mode = FindMetadata(loaded.GetMetadata(), Dia::Core::StringCRC("mode"));
	ASSERT_NE(mode, nullptr);
	EXPECT_STREQ(mode->stringVal.AsChar(), "stack");
}

TEST(JsonStateMachineSerializer, MetadataAllTypesRoundTrip)
{
	StateMachineDefinition original = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
			.StateMetadata(Dia::Core::StringCRC("b"), MetadataValue::FromBool(false))
			.StateMetadata(Dia::Core::StringCRC("i"), MetadataValue::FromInt(-7))
			.StateMetadata(Dia::Core::StringCRC("f"), MetadataValue::FromFloat(3.14f))
			.StateMetadata(Dia::Core::StringCRC("s"), MetadataValue::FromString("hello"))
		.Build();

	char buffer[4096] = {};
	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.Save(original, buffer, sizeof(buffer)));

	CallbackRegistry reg;
	StateMachineDefinition loaded;
	ASSERT_TRUE(serializer.Load(loaded, reg, buffer));

	const auto& meta = loaded.GetStates()[0].metadata;
	const MetadataValue* b = FindMetadata(meta, Dia::Core::StringCRC("b"));
	const MetadataValue* i = FindMetadata(meta, Dia::Core::StringCRC("i"));
	const MetadataValue* f = FindMetadata(meta, Dia::Core::StringCRC("f"));
	const MetadataValue* s = FindMetadata(meta, Dia::Core::StringCRC("s"));

	ASSERT_NE(b, nullptr); EXPECT_EQ(b->type, MetadataValue::kBool);   EXPECT_FALSE(b->boolVal);
	ASSERT_NE(i, nullptr); EXPECT_EQ(i->type, MetadataValue::kInt);    EXPECT_EQ(i->intVal, -7);
	ASSERT_NE(f, nullptr); EXPECT_EQ(f->type, MetadataValue::kFloat);  EXPECT_FLOAT_EQ(f->floatVal, 3.14f);
	ASSERT_NE(s, nullptr); EXPECT_EQ(s->type, MetadataValue::kString); EXPECT_STREQ(s->stringVal.AsChar(), "hello");
}
