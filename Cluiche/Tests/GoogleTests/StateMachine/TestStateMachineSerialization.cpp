#include <gtest/gtest.h>

#include <DiaStateMachine/StateMachineDefinition.h>
#include <DiaStateMachine/StateMachineBuilder.h>
#include <DiaStateMachine/HierarchicalStateMachineDefinition.h>
#include <DiaStateMachine/HierarchicalStateMachineBuilder.h>
#include <DiaStateMachine/PushdownAutomatonDefinition.h>
#include <DiaStateMachine/PushdownAutomatonBuilder.h>
#include <DiaStateMachine/IStateMachineSerializer.h>
#include <DiaStateMachine/JsonStateMachineSerializer.h>
#include <DiaStateMachine/CallbackRegistry.h>
#include <DiaSerializer/SerializeResult.h>
#include <cstdio>

using namespace Dia::StateMachine;
using Dia::Serializer::SetMetadata;
using Dia::Serializer::FindMetadata;

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
	reg.Finalize();
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
	reg.Finalize();
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

// ---------------------------------------------------------------------------
// 1. kAnyState round-trip
// ---------------------------------------------------------------------------

TEST(JsonStateMachineSerializer, AnyStateSourceRoundTrip)
{
	StateMachineDefinition original = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("B"))
		.State(Dia::Core::StringCRC("C"))
			.Transition(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("goB"))  // normal
		.Build();

	// Manually inject a wildcard transition via the definition's mutable getter
	TransitionDef wildcard;
	wildcard.sourceStateId = kAnyState;
	wildcard.targetStateId = Dia::Core::StringCRC("C");
	wildcard.triggerId     = Dia::Core::StringCRC("escape");
	original.GetTransitions().Add(wildcard);

	char buffer[4096] = {};
	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.Save(original, buffer, sizeof(buffer)));

	CallbackRegistry reg;
	reg.Finalize();
	StateMachineDefinition loaded;
	ASSERT_TRUE(serializer.Load(loaded, reg, buffer));

	bool foundWildcard = false;
	const auto& transitions = loaded.GetTransitions();
	for (unsigned int i = 0; i < transitions.Size(); ++i)
	{
		if (transitions[i].sourceStateId == kAnyState)
		{
			foundWildcard = true;
			EXPECT_EQ(transitions[i].targetStateId, Dia::Core::StringCRC("C"));
			EXPECT_EQ(transitions[i].triggerId, Dia::Core::StringCRC("escape"));
		}
	}
	EXPECT_TRUE(foundWildcard);
}

// ---------------------------------------------------------------------------
// 2. Clone() preserves metadata and callback names
// ---------------------------------------------------------------------------

TEST(StateMachineDefinition, ClonePreservesMetadataAndCallbackNames)
{
	StateMachineDefinition original = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
			.OnEnter(OnEnterA, Dia::Core::StringCRC("OnEnterA"))
			.StateMetadata(Dia::Core::StringCRC("clip"), MetadataValue::FromString("idle"))
		.MachineMetadata(Dia::Core::StringCRC("layer"), MetadataValue::FromInt(3))
		.State(Dia::Core::StringCRC("B"))
		.Build();

	StateMachineDefinition clone = original.Clone();

	EXPECT_EQ(clone.GetStates()[0].onEnterName, Dia::Core::StringCRC("OnEnterA"));

	const MetadataValue* clip = FindMetadata(clone.GetStates()[0].metadata, Dia::Core::StringCRC("clip"));
	ASSERT_NE(clip, nullptr);
	EXPECT_STREQ(clip->stringVal.AsChar(), "idle");

	const MetadataValue* layer = FindMetadata(clone.GetMetadata(), Dia::Core::StringCRC("layer"));
	ASSERT_NE(layer, nullptr);
	EXPECT_EQ(layer->intVal, 3);
}

TEST(HierarchicalStateMachineDefinition, ClonePreservesMetadata)
{
	HierarchicalStateMachineDefinition original = HierarchicalStateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("Root"))
		.State(Dia::Core::StringCRC("Root"))
			.StateMetadata(Dia::Core::StringCRC("weight"), MetadataValue::FromFloat(0.8f))
		.MachineMetadata(Dia::Core::StringCRC("blend"), MetadataValue::FromBool(true))
		.Build();

	HierarchicalStateMachineDefinition clone = original.Clone();

	const MetadataValue* weight = FindMetadata(clone.GetStates()[0].metadata, Dia::Core::StringCRC("weight"));
	ASSERT_NE(weight, nullptr);
	EXPECT_FLOAT_EQ(weight->floatVal, 0.8f);

	const MetadataValue* blend = FindMetadata(clone.GetMetadata(), Dia::Core::StringCRC("blend"));
	ASSERT_NE(blend, nullptr);
	EXPECT_TRUE(blend->boolVal);
}

TEST(PushdownAutomatonDefinition, ClonePreservesMetadata)
{
	PushdownAutomatonDefinition original = PushdownAutomatonBuilder()
		.InitialState(Dia::Core::StringCRC("Menu"))
		.State(Dia::Core::StringCRC("Menu"))
			.OnPause(OnPauseA, Dia::Core::StringCRC("OnPauseA"))
			.StateMetadata(Dia::Core::StringCRC("depth"), MetadataValue::FromInt(1))
		.Build();

	PushdownAutomatonDefinition clone = original.Clone();

	EXPECT_EQ(clone.GetStates()[0].onPauseName, Dia::Core::StringCRC("OnPauseA"));

	const MetadataValue* depth = FindMetadata(clone.GetStates()[0].metadata, Dia::Core::StringCRC("depth"));
	ASSERT_NE(depth, nullptr);
	EXPECT_EQ(depth->intVal, 1);
}

// ---------------------------------------------------------------------------
// 3. HSM and PDA builder callback names (direct unit tests)
// ---------------------------------------------------------------------------

TEST(HierarchicalStateMachineBuilder, CallbackNamesStoredInDef)
{
	HierarchicalStateMachineDefinition def = HierarchicalStateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("Root"))
		.State(Dia::Core::StringCRC("Root"))
			.OnEnter(OnEnterA,   Dia::Core::StringCRC("OnEnterA"))
			.OnExit(OnExitA,     Dia::Core::StringCRC("OnExitA"))
			.OnUpdate(OnUpdateA, Dia::Core::StringCRC("OnUpdateA"))
		.Build();

	EXPECT_EQ(def.GetStates()[0].onEnterName,  Dia::Core::StringCRC("OnEnterA"));
	EXPECT_EQ(def.GetStates()[0].onExitName,   Dia::Core::StringCRC("OnExitA"));
	EXPECT_EQ(def.GetStates()[0].onUpdateName, Dia::Core::StringCRC("OnUpdateA"));
}

TEST(PushdownAutomatonBuilder, CallbackNamesStoredInDef)
{
	PushdownAutomatonDefinition def = PushdownAutomatonBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
			.OnEnter(OnEnterA,   Dia::Core::StringCRC("OnEnterA"))
			.OnExit(OnExitA,     Dia::Core::StringCRC("OnExitA"))
			.OnUpdate(OnUpdateA, Dia::Core::StringCRC("OnUpdateA"))
			.OnPause(OnPauseA,   Dia::Core::StringCRC("OnPauseA"))
			.OnResume(OnResumeA, Dia::Core::StringCRC("OnResumeA"))
		.Build();

	EXPECT_EQ(def.GetStates()[0].onEnterName,  Dia::Core::StringCRC("OnEnterA"));
	EXPECT_EQ(def.GetStates()[0].onExitName,   Dia::Core::StringCRC("OnExitA"));
	EXPECT_EQ(def.GetStates()[0].onUpdateName, Dia::Core::StringCRC("OnUpdateA"));
	EXPECT_EQ(def.GetStates()[0].onPauseName,  Dia::Core::StringCRC("OnPauseA"));
	EXPECT_EQ(def.GetStates()[0].onResumeName, Dia::Core::StringCRC("OnResumeA"));
}

// ---------------------------------------------------------------------------
// 4. Malformed JSON input
// ---------------------------------------------------------------------------

TEST(JsonStateMachineSerializer, MalformedJsonReturnsFalse)
{
	const char* garbage = "not json at all }{{{";
	CallbackRegistry reg;
	reg.Finalize();
	StateMachineDefinition def;
	JsonStateMachineSerializer serializer;
	EXPECT_FALSE(serializer.Load(def, reg, garbage));
}

// ---------------------------------------------------------------------------
// 5. Topology-only machine (no callbacks) loads and validates cleanly
// ---------------------------------------------------------------------------

TEST(JsonStateMachineSerializer, TopologyOnlyNoCallbacksLoadsClean)
{
	StateMachineDefinition original = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("Idle"))
		.State(Dia::Core::StringCRC("Idle"))
			.Transition(Dia::Core::StringCRC("Running"), Dia::Core::StringCRC("start"))
		.State(Dia::Core::StringCRC("Running"))
			.Transition(Dia::Core::StringCRC("Idle"), Dia::Core::StringCRC("stop"))
		.Build();

	char buffer[4096] = {};
	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.Save(original, buffer, sizeof(buffer)));

	CallbackRegistry reg;  // empty — no callbacks registered
	reg.Finalize();
	StateMachineDefinition loaded;
	ASSERT_TRUE(serializer.Load(loaded, reg, buffer));

	ASSERT_EQ(loaded.GetStates().Size(), 2u);
	ASSERT_EQ(loaded.GetTransitions().Size(), 2u);
	EXPECT_EQ(loaded.GetInitialStateId(), Dia::Core::StringCRC("Idle"));

	// All function pointers must be null — no callbacks were in the JSON
	EXPECT_EQ(loaded.GetStates()[0].onEnter,  nullptr);
	EXPECT_EQ(loaded.GetStates()[0].onExit,   nullptr);
	EXPECT_EQ(loaded.GetStates()[0].onUpdate, nullptr);
	EXPECT_TRUE(loaded.IsValid());
}

// ---------------------------------------------------------------------------
// 6. SerializeResult error strings
// ---------------------------------------------------------------------------

TEST(JsonStateMachineSerializer, FailureResultCarriesErrorString)
{
	const char* garbage = "not json at all }{{{";
	CallbackRegistry reg;
	reg.Finalize();
	StateMachineDefinition def;
	JsonStateMachineSerializer serializer;
	auto result = serializer.Load(def, reg, garbage);
	EXPECT_FALSE(result);
	EXPECT_NE(result.error, nullptr);
	EXPECT_GT(strlen(result.error), 0u);
}

TEST(JsonStateMachineSerializer, VersionMismatchResultCarriesErrorString)
{
	const char* badJson = R"({"version":"99.0","type":"FlatStateMachine","initialState":"A","states":[],"transitions":[]})";
	CallbackRegistry reg = MakeRegistry();
	StateMachineDefinition def;
	JsonStateMachineSerializer serializer;
	auto result = serializer.Load(def, reg, badJson);
	EXPECT_FALSE(result);
	EXPECT_NE(result.error, nullptr);
}

TEST(JsonStateMachineSerializer, SuccessResultHasNullError)
{
	StateMachineDefinition original = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("A"))
		.State(Dia::Core::StringCRC("A"))
		.Build();

	char buffer[4096] = {};
	JsonStateMachineSerializer serializer;
	auto saveResult = serializer.Save(original, buffer, sizeof(buffer));
	EXPECT_TRUE(saveResult);
	EXPECT_EQ(saveResult.error, nullptr);

	CallbackRegistry reg;
	reg.Finalize();
	StateMachineDefinition loaded;
	auto loadResult = serializer.Load(loaded, reg, buffer);
	EXPECT_TRUE(loadResult);
	EXPECT_EQ(loadResult.error, nullptr);
}

// ---------------------------------------------------------------------------
// 7. LoadFromFile / SaveToFile — all three definition types
// ---------------------------------------------------------------------------

static const char* kTmpFlat  = "C:\\Temp\\dia_test_flat.json";
static const char* kTmpHsm   = "C:\\Temp\\dia_test_hsm.json";
static const char* kTmpPda   = "C:\\Temp\\dia_test_pda.json";

TEST(JsonStateMachineSerializer, SaveToFileAndLoadFromFile_Flat)
{
	StateMachineDefinition original = StateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("Idle"))
		.State(Dia::Core::StringCRC("Idle"))
			.OnEnter(OnEnterA, Dia::Core::StringCRC("OnEnterA"))
			.StateMetadata(Dia::Core::StringCRC("clip"), MetadataValue::FromString("idle"))
			.Transition(Dia::Core::StringCRC("Run"), Dia::Core::StringCRC("start"))
		.State(Dia::Core::StringCRC("Run"))
		.Build();

	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.SaveToFile(kTmpFlat, original));

	CallbackRegistry reg = MakeRegistry();
	StateMachineDefinition loaded;
	ASSERT_TRUE(serializer.LoadFromFile(kTmpFlat, loaded, reg));

	EXPECT_EQ(loaded.GetStates().Size(), 2u);
	EXPECT_EQ(loaded.GetInitialStateId(), Dia::Core::StringCRC("Idle"));
	EXPECT_EQ(loaded.GetStates()[0].onEnterName, Dia::Core::StringCRC("OnEnterA"));
	EXPECT_EQ(loaded.GetStates()[0].onEnter, OnEnterA);

	const MetadataValue* clip = FindMetadata(loaded.GetStates()[0].metadata, Dia::Core::StringCRC("clip"));
	ASSERT_NE(clip, nullptr);
	EXPECT_STREQ(clip->stringVal.AsChar(), "idle");

	remove(kTmpFlat);
}

TEST(JsonStateMachineSerializer, SaveToFileAndLoadFromFile_Hsm)
{
	HierarchicalStateMachineDefinition original = HierarchicalStateMachineBuilder()
		.InitialState(Dia::Core::StringCRC("Root"))
		.State(Dia::Core::StringCRC("Root"))
			.InitialChild(Dia::Core::StringCRC("A"))
		.ChildState(Dia::Core::StringCRC("A"), Dia::Core::StringCRC("Root"))
			.OnEnter(OnEnterA, Dia::Core::StringCRC("OnEnterA"))
		.ChildState(Dia::Core::StringCRC("B"), Dia::Core::StringCRC("Root"))
		.Build();

	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.SaveToFile(kTmpHsm, original));

	CallbackRegistry reg = MakeRegistry();
	HierarchicalStateMachineDefinition loaded;
	ASSERT_TRUE(serializer.LoadFromFile(kTmpHsm, loaded, reg));

	EXPECT_EQ(loaded.GetStates().Size(), 3u);
	EXPECT_EQ(loaded.GetInitialStateId(), Dia::Core::StringCRC("Root"));

	remove(kTmpHsm);
}

TEST(JsonStateMachineSerializer, SaveToFileAndLoadFromFile_Pda)
{
	PushdownAutomatonDefinition original = PushdownAutomatonBuilder()
		.InitialState(Dia::Core::StringCRC("Menu"))
		.State(Dia::Core::StringCRC("Menu"))
			.OnPause(OnPauseA, Dia::Core::StringCRC("OnPauseA"))
		.Build();

	JsonStateMachineSerializer serializer;
	ASSERT_TRUE(serializer.SaveToFile(kTmpPda, original));

	CallbackRegistry reg = MakeRegistry();
	PushdownAutomatonDefinition loaded;
	ASSERT_TRUE(serializer.LoadFromFile(kTmpPda, loaded, reg));

	EXPECT_EQ(loaded.GetStates().Size(), 1u);
	EXPECT_EQ(loaded.GetStates()[0].onPause, OnPauseA);

	remove(kTmpPda);
}

TEST(JsonStateMachineSerializer, LoadFromFile_MissingFile_ReturnsFalse)
{
	JsonStateMachineSerializer serializer;
	CallbackRegistry reg;
	reg.Finalize();
	StateMachineDefinition def;
	auto result = serializer.LoadFromFile("C:\\Temp\\nonexistent_dia_test.json", def, reg);
	EXPECT_FALSE(result);
	EXPECT_NE(result.error, nullptr);
}

// ---------------------------------------------------------------------------
// 8. LoadBeforeFinalize asserts
// ---------------------------------------------------------------------------

TEST(JsonStateMachineSerializer, LoadWithUnfinalizedRegistryAsserts_Flat)
{
	const char* json = R"({"version":"1.0","type":"FlatStateMachine","initialState":"A","states":[],"transitions":[]})";
	CallbackRegistry reg;  // not finalized
	StateMachineDefinition def;
	JsonStateMachineSerializer serializer;
	EXPECT_DEATH(serializer.Load(def, reg, json), "");
}

TEST(JsonStateMachineSerializer, LoadWithUnfinalizedRegistryAsserts_Hsm)
{
	const char* json = R"({"version":"1.0","type":"HierarchicalStateMachine","initialState":"A","states":[],"transitions":[]})";
	CallbackRegistry reg;
	HierarchicalStateMachineDefinition def;
	JsonStateMachineSerializer serializer;
	EXPECT_DEATH(serializer.Load(def, reg, json), "");
}

TEST(JsonStateMachineSerializer, LoadWithUnfinalizedRegistryAsserts_Pda)
{
	const char* json = R"({"version":"1.0","type":"PushdownAutomaton","initialState":"A","states":[]})";
	CallbackRegistry reg;
	PushdownAutomatonDefinition def;
	JsonStateMachineSerializer serializer;
	EXPECT_DEATH(serializer.Load(def, reg, json), "");
}
