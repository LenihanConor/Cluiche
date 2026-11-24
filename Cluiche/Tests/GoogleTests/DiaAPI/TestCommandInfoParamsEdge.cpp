////////////////////////////////////////////////////////////////////////////////
// Filename: TestCommandInfoParamsEdge.cpp
// Description: Edge / stress tests for CommandInfo + CommandInfoJson ValueParam
//              metadata, ListCommandInfos / ListJsonCommandInfos and Generation().
// Spec: docs/specs/applications/dia/systems/diavalueschema/code-side-descriptions.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaCore/CRC/StringCRC.h>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace Dia::API;
using Dia::Core::StringCRC;

// Layout contract: the first four members keep legacy aggregate init compiling.
static_assert(offsetof(ValueParam, name) == 0, "ValueParam::name must be first");
static_assert(offsetof(ValueParam, type) == sizeof(void*), "ValueParam::type must be second");
static_assert(offsetof(ValueParam, required) == 2 * sizeof(void*), "ValueParam::required must be third");
static_assert(offsetof(ValueParam, description) == 3 * sizeof(void*), "ValueParam::description must be fourth");
static_assert(offsetof(ValueParam, unit) > offsetof(ValueParam, description), "new members come after the legacy four");
static_assert(sizeof(ValueParam) == (sizeof(void*) == 8 ? 64u : 36u), "ValueParam size changed");
static_assert(std::is_standard_layout<ValueParam>::value, "ValueParam must stay standard layout");
static_assert(std::is_trivially_copyable<ValueParam>::value, "ValueParam must stay trivially copyable");
static_assert(std::is_aggregate<ValueParam>::value, "ValueParam must stay an aggregate");

namespace
{
	int NoopCli(const CommandArgs&) { return 0; }
	Json::Value NoopJson(const Json::Value&) { return Json::Value(Json::objectValue); }

	// Storage with distinct addresses so tests can tell pointers apart.
	const ValueParam kParamsA[2] = {
		{ "alpha", "string", true, "A" },
		{ "beta", "int", false, "B" },
	};
	const ValueParam kParamsB[3] = {
		{ "one", "float", true, "1" },
		{ "two", "float", true, "2" },
		{ "three", "bool", false, "3" },
	};

	std::string NameFor(const char* prefix, int a, int b = -1)
	{
		char buf[64];
		if (b < 0) snprintf(buf, sizeof(buf), "%s.%d", prefix, a);
		else       snprintf(buf, sizeof(buf), "%s.%d.%d", prefix, a, b);
		return buf;
	}
}

class CommandInfoParamsEdgeTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (IsInitialized())
			Shutdown();
		Initialize();
	}
	void TearDown() override
	{
		if (IsInitialized())
			Shutdown();
	}

	static CommandInfo MakeInfo(const char* name)
	{
		CommandInfo info;
		info.name = StringCRC(name);
		info.description = "desc";
		info.category = StringCRC("test");
		info.owner = "Test";
		info.version = "1.0.0";
		info.example = "";
		info.callback = NoopCli;
		return info;
	}

	static CommandInfoJson MakeJson(const char* name)
	{
		CommandInfoJson info;
		info.name = StringCRC(name);
		info.description = "desc";
		info.category = StringCRC("test");
		info.owner = "Test";
		info.callback = NoopJson;
		return info;
	}
};

////////////////////////////////////////////////////////////////////////////////
// Generation: failed registrations never bump
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandInfoParamsEdgeTest, GenerationUnchangedByFailedPlainRegistrations)
{
	ASSERT_TRUE(RegisterCommand(MakeInfo("cipe.gen.ok")));
	const unsigned g = Generation();

	EXPECT_FALSE(RegisterCommand(MakeInfo("cipe.gen.ok")));           // duplicate

	EXPECT_FALSE(RegisterCommand(MakeInfo("Cipe.Bad")));              // uppercase
	EXPECT_FALSE(RegisterCommand(MakeInfo("")));                      // empty name
	EXPECT_FALSE(RegisterCommand(MakeInfo("has space")));             // bad char

	CommandInfo nullCb = MakeInfo("cipe.gen.nullcb");
	nullCb.callback = nullptr;
	EXPECT_FALSE(RegisterCommand(nullCb));

	CommandInfo nullDesc = MakeInfo("cipe.gen.nulldesc");
	nullDesc.description = nullptr;
	EXPECT_FALSE(RegisterCommand(nullDesc));

	CommandInfo emptyDesc = MakeInfo("cipe.gen.emptydesc");
	emptyDesc.description = "";
	EXPECT_FALSE(RegisterCommand(emptyDesc));

	CommandInfo nullOwner = MakeInfo("cipe.gen.nullowner");
	nullOwner.owner = nullptr;
	EXPECT_FALSE(RegisterCommand(nullOwner));

	EXPECT_EQ(g, Generation());
}

TEST_F(CommandInfoParamsEdgeTest, GenerationUnchangedByFailedJsonRegistrations)
{
	ASSERT_TRUE(RegisterCommandJson(MakeJson("cipe.gen.json.ok")));
	const unsigned g = Generation();

	EXPECT_FALSE(RegisterCommandJson(MakeJson("cipe.gen.json.ok")));  // duplicate
	EXPECT_FALSE(RegisterCommandJson(MakeJson("Cipe.Bad")));
	EXPECT_FALSE(RegisterCommandJson(MakeJson("")));

	CommandInfoJson nullCb = MakeJson("cipe.gen.json.nullcb");
	nullCb.callback = nullptr;
	EXPECT_FALSE(RegisterCommandJson(nullCb));

	CommandInfoJson nullDesc = MakeJson("cipe.gen.json.nulldesc");
	nullDesc.description = nullptr;
	EXPECT_FALSE(RegisterCommandJson(nullDesc));

	CommandInfoJson nullOwner = MakeJson("cipe.gen.json.nullowner");
	nullOwner.owner = nullptr;
	EXPECT_FALSE(RegisterCommandJson(nullOwner));

	EXPECT_EQ(g, Generation());
}

// PIN: Generation is NOT bumped by UnregisterCommand / UnregisterCommandJson (documented:
// it counts registrations only). A consumer polling Generation cannot see removals.
TEST_F(CommandInfoParamsEdgeTest, PinGenerationNotBumpedByUnregister)
{
	ASSERT_TRUE(RegisterCommand(MakeInfo("cipe.unreg.plain")));
	ASSERT_TRUE(RegisterCommandJson(MakeJson("cipe.unreg.json")));
	const unsigned g = Generation();

	EXPECT_TRUE(UnregisterCommand(StringCRC("cipe.unreg.plain")));
	EXPECT_EQ(g, Generation());
	EXPECT_TRUE(UnregisterCommandJson(StringCRC("cipe.unreg.json")));
	EXPECT_EQ(g, Generation());

	// Failed unregistrations do not bump either.
	EXPECT_FALSE(UnregisterCommand(StringCRC("cipe.unreg.plain")));
	EXPECT_FALSE(UnregisterCommandJson(StringCRC("cipe.unreg.json")));
	EXPECT_EQ(g, Generation());

	// Re-registering after unregistering does bump.
	ASSERT_TRUE(RegisterCommand(MakeInfo("cipe.unreg.plain")));
	EXPECT_EQ(g + 1, Generation());
}

// Registration before Initialize() goes to the pending list; it still bumps, and
// Initialize() (which publishes pending commands) does not bump a second time.
TEST_F(CommandInfoParamsEdgeTest, PendingRegistrationBumpsOnceAndInitializeDoesNotBump)
{
	Shutdown();
	const unsigned g = Generation();
	ASSERT_TRUE(RegisterCommand(MakeInfo("cipe.pending.one")));
	EXPECT_EQ(g + 1, Generation());

	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> before;
	ListCommandInfos(before);
	EXPECT_EQ(0u, before.Size()); // pending ones are excluded

	Initialize();
	EXPECT_EQ(g + 1, Generation());

	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> after;
	ListCommandInfos(after);
	ASSERT_EQ(1u, after.Size());
	EXPECT_STREQ("cipe.pending.one", after[0].name.AsChar());
}

TEST_F(CommandInfoParamsEdgeTest, GenerationSurvivesShutdownAndInitialize)
{
	ASSERT_TRUE(RegisterCommand(MakeInfo("cipe.survive")));
	const unsigned g = Generation();
	Shutdown();
	EXPECT_EQ(g, Generation());
	Initialize();
	EXPECT_EQ(g, Generation());
}

TEST_F(CommandInfoParamsEdgeTest, GenerationIsMonotonicOverRandomOperations)
{
	std::mt19937 rng(1234);
	unsigned last = Generation();
	for (int i = 0; i < 300; i++)
	{
		const std::string name = NameFor("cipe.mono", (int)(rng() % 20));
		switch (rng() % 4)
		{
		case 0: RegisterCommand(MakeInfo(name.c_str())); break;
		case 1: UnregisterCommand(StringCRC(name.c_str())); break;
		case 2: RegisterCommandJson(MakeJson(name.c_str())); break;
		default: UnregisterCommandJson(StringCRC(name.c_str())); break;
		}
		const unsigned now = Generation();
		ASSERT_GE(now, last);
		ASSERT_LE(now, last + 1) << "one operation bumps at most once";
		last = now;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Round trips: plain vs JSON parity
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandInfoParamsEdgeTest, PlainAndJsonParityOfParamsAndNoArgs)
{
	const ValueParam* const arrays[] = { nullptr, kParamsA, kParamsB };
	const unsigned counts[] = { 0u, 2u, 3u };
	int n = 0;
	for (int a = 0; a < 3; a++)
		for (int noArgs = 0; noArgs < 2; noArgs++)
		{
			const std::string plainName = NameFor("cipe.parity.plain", n);
			const std::string jsonName = NameFor("cipe.parity.json", n);
			n++;

			CommandInfo c = MakeInfo(plainName.c_str());
			c.params = arrays[a];
			c.paramCount = counts[a];
			c.noArgs = noArgs != 0;
			ASSERT_TRUE(RegisterCommand(c));

			CommandInfoJson j = MakeJson(jsonName.c_str());
			j.params = arrays[a];
			j.paramCount = counts[a];
			j.noArgs = noArgs != 0;
			ASSERT_TRUE(RegisterCommandJson(j));

			CommandInfo outC{};
			ASSERT_TRUE(CopyCommandInfo(StringCRC(plainName.c_str()), outC));
			EXPECT_EQ(arrays[a], outC.params);
			EXPECT_EQ(counts[a], outC.paramCount);
			EXPECT_EQ(noArgs != 0, outC.noArgs);
		}

	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> plain;
	ListCommandInfos(plain);
	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> json;
	ListJsonCommandInfos(json);
	ASSERT_EQ(6u, plain.Size());
	ASSERT_EQ(6u, json.Size());
	for (unsigned i = 0; i < 6; i++)
	{
		EXPECT_EQ(plain[i].params, json[i].params) << i;
		EXPECT_EQ(plain[i].paramCount, json[i].paramCount) << i;
		EXPECT_EQ(plain[i].noArgs, json[i].noArgs) << i;
	}
}

// PIN: the registry does not validate params/paramCount/noArgs combinations; whatever the
// caller supplies is stored and returned verbatim.
TEST_F(CommandInfoParamsEdgeTest, PinNoArgsWithParamsIsStoredVerbatim)
{
	CommandInfo c = MakeInfo("cipe.pin.noargs-params");
	c.params = kParamsA;
	c.paramCount = 2;
	c.noArgs = true;
	ASSERT_TRUE(RegisterCommand(c));

	CommandInfoJson j = MakeJson("cipe.pin.noargs-params-json");
	j.params = kParamsA;
	j.paramCount = 2;
	j.noArgs = true;
	ASSERT_TRUE(RegisterCommandJson(j));

	const CommandInfo* got = GetCommand(StringCRC("cipe.pin.noargs-params"));
	ASSERT_NE(nullptr, got);
	EXPECT_TRUE(got->noArgs);
	EXPECT_EQ(kParamsA, got->params);
	EXPECT_EQ(2u, got->paramCount);

	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> json;
	ListJsonCommandInfos(json);
	ASSERT_EQ(1u, json.Size());
	EXPECT_TRUE(json[0].noArgs);
	EXPECT_EQ(kParamsA, json[0].params);
}

TEST_F(CommandInfoParamsEdgeTest, PinNullParamsWithCountIsStoredVerbatim)
{
	CommandInfo c = MakeInfo("cipe.pin.nullparams");
	c.params = nullptr;
	c.paramCount = 7;
	ASSERT_TRUE(RegisterCommand(c));
	const CommandInfo* got = GetCommand(StringCRC("cipe.pin.nullparams"));
	ASSERT_NE(nullptr, got);
	EXPECT_EQ(nullptr, got->params);
	EXPECT_EQ(7u, got->paramCount);
}

TEST_F(CommandInfoParamsEdgeTest, ReRegisterAfterUnregisterUsesNewParamsPointer)
{
	CommandInfo c = MakeInfo("cipe.rereg");
	c.params = kParamsA;
	c.paramCount = 2;
	ASSERT_TRUE(RegisterCommand(c));
	ASSERT_TRUE(UnregisterCommand(StringCRC("cipe.rereg")));
	EXPECT_EQ(nullptr, GetCommand(StringCRC("cipe.rereg")));

	c.params = kParamsB;
	c.paramCount = 3;
	ASSERT_TRUE(RegisterCommand(c));
	const CommandInfo* got = GetCommand(StringCRC("cipe.rereg"));
	ASSERT_NE(nullptr, got);
	EXPECT_EQ(kParamsB, got->params);
	EXPECT_EQ(3u, got->paramCount);

	CommandInfoJson j = MakeJson("cipe.rereg.json");
	j.params = kParamsA;
	j.paramCount = 2;
	ASSERT_TRUE(RegisterCommandJson(j));
	ASSERT_TRUE(UnregisterCommandJson(StringCRC("cipe.rereg.json")));
	j.params = kParamsB;
	j.paramCount = 3;
	ASSERT_TRUE(RegisterCommandJson(j));
	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> list;
	ListJsonCommandInfos(list);
	ASSERT_EQ(1u, list.Size());
	EXPECT_EQ(kParamsB, list[0].params);
	EXPECT_EQ(3u, list[0].paramCount);
}

TEST_F(CommandInfoParamsEdgeTest, CopyKeepsParamsAfterUnregister)
{
	CommandInfo c = MakeInfo("cipe.copy.after");
	c.params = kParamsA;
	c.paramCount = 2;
	ASSERT_TRUE(RegisterCommand(c));
	CommandInfo copy{};
	ASSERT_TRUE(CopyCommandInfo(StringCRC("cipe.copy.after"), copy));
	ASSERT_TRUE(UnregisterCommand(StringCRC("cipe.copy.after")));
	EXPECT_EQ(kParamsA, copy.params);
	EXPECT_EQ(2u, copy.paramCount);
	EXPECT_STREQ("alpha", copy.params[0].name);
	EXPECT_FALSE(CopyCommandInfo(StringCRC("cipe.copy.after"), copy));
}

// PIN: a plain and a JSON command may share a name (the JSON register only logs a
// warning); Generation counts both, and the two lists are independent.
TEST_F(CommandInfoParamsEdgeTest, PinPlainAndJsonMayShareName)
{
	const unsigned g = Generation();
	CommandInfo c = MakeInfo("cipe.shared");
	c.params = kParamsA;
	c.paramCount = 2;
	CommandInfoJson j = MakeJson("cipe.shared");
	j.params = kParamsB;
	j.paramCount = 3;
	ASSERT_TRUE(RegisterCommand(c));
	ASSERT_TRUE(RegisterCommandJson(j));
	EXPECT_EQ(g + 2, Generation());

	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> plain;
	ListCommandInfos(plain);
	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> json;
	ListJsonCommandInfos(json);
	ASSERT_EQ(1u, plain.Size());
	ASSERT_EQ(1u, json.Size());
	EXPECT_EQ(kParamsA, plain[0].params);
	EXPECT_EQ(kParamsB, json[0].params);

	// Removing one leaves the other.
	EXPECT_TRUE(UnregisterCommandJson(StringCRC("cipe.shared")));
	EXPECT_NE(nullptr, GetCommand(StringCRC("cipe.shared")));
	EXPECT_TRUE(UnregisterCommand(StringCRC("cipe.shared")));
}

TEST_F(CommandInfoParamsEdgeTest, ListsPreserveRegistrationOrderAndAppend)
{
	for (int i = 0; i < 10; i++)
	{
		std::string n = NameFor("cipe.order", i);
		ASSERT_TRUE(RegisterCommand(MakeInfo(n.c_str())));
		ASSERT_TRUE(RegisterCommandJson(MakeJson(n.c_str())));
	}
	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> plain;
	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> json;
	// Pre-existing element is kept (lists append, they do not clear).
	plain.Add(MakeInfo("cipe.order.pre"));
	json.Add(MakeJson("cipe.order.pre"));
	ListCommandInfos(plain);
	ListJsonCommandInfos(json);
	ASSERT_EQ(11u, plain.Size());
	ASSERT_EQ(11u, json.Size());
	EXPECT_STREQ("cipe.order.pre", plain[0].name.AsChar());
	for (int i = 0; i < 10; i++)
	{
		std::string n = NameFor("cipe.order", i);
		EXPECT_STREQ(n.c_str(), plain[1 + i].name.AsChar());
		EXPECT_STREQ(n.c_str(), json[1 + i].name.AsChar());
	}
}

TEST_F(CommandInfoParamsEdgeTest, ListedJsonCallbackStillInvocable)
{
	CommandInfoJson j = MakeJson("cipe.callback");
	int hits = 0;
	j.callback = [&hits](const Json::Value&) { hits++; return Json::Value(Json::objectValue); };
	ASSERT_TRUE(RegisterCommandJson(j));
	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> list;
	ListJsonCommandInfos(list);
	ASSERT_EQ(1u, list.Size());
	list[0].callback(Json::Value());
	EXPECT_EQ(1, hits);
}

////////////////////////////////////////////////////////////////////////////////
// Capacity: lists are DynamicArrayC<..., 256>
//
// PIN: the registry arrays and the out-parameters hold 256 entries. A 257th
// registration (or listing into an out-array that is already partly full) trips
// DIA_ASSERT("Array is full") and writes out of bounds, so only the exact limit
// of 256 is exercised here.
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandInfoParamsEdgeTest, PinListCommandInfosAtExactCapacity256)
{
	for (int i = 0; i < 256; i++)
	{
		std::string n = NameFor("cipe.cap", i);
		CommandInfo c = MakeInfo(n.c_str());
		c.params = (i % 2) ? kParamsA : kParamsB;
		c.paramCount = (i % 2) ? 2u : 3u;
		ASSERT_TRUE(RegisterCommand(c)) << i;
	}
	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> list;
	ListCommandInfos(list);
	ASSERT_EQ(256u, list.Size());
	EXPECT_STREQ("cipe.cap.0", list[0].name.AsChar());
	EXPECT_STREQ("cipe.cap.255", list[255].name.AsChar());
	EXPECT_EQ(kParamsA, list[255].params);
	EXPECT_EQ(kParamsB, list[254].params);
}

TEST_F(CommandInfoParamsEdgeTest, PinListJsonCommandInfosAtExactCapacity256)
{
	for (int i = 0; i < 256; i++)
	{
		std::string n = NameFor("cipe.jcap", i);
		CommandInfoJson c = MakeJson(n.c_str());
		c.params = (i % 2) ? kParamsA : kParamsB;
		c.paramCount = (i % 2) ? 2u : 3u;
		ASSERT_TRUE(RegisterCommandJson(c)) << i;
	}
	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> list;
	ListJsonCommandInfos(list);
	ASSERT_EQ(256u, list.Size());
	EXPECT_STREQ("cipe.jcap.0", list[0].name.AsChar());
	EXPECT_STREQ("cipe.jcap.255", list[255].name.AsChar());
	EXPECT_EQ(kParamsA, list[255].params);
}

////////////////////////////////////////////////////////////////////////////////
// Threads
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandInfoParamsEdgeTest, StressGenerationDeltaIsExactForConcurrentPlainRegistrations)
{
	const unsigned g0 = Generation();
	std::vector<std::thread> threads;
	std::vector<int> failures(4, 0);
	for (int t = 0; t < 4; t++)
	{
		threads.emplace_back([t, &failures]()
		{
			for (int i = 0; i < 200; i++)
			{
				const std::string n = NameFor("cipe.mt.plain", t, i);
				CommandInfo c = MakeInfo(n.c_str());
				c.params = kParamsA;
				c.paramCount = 2;
				if (!RegisterCommand(c)) failures[t]++;
				// Keep the live set small (the registry holds at most 256 commands).
				if (!UnregisterCommand(StringCRC(n.c_str()))) failures[t]++;
			}
		});
	}
	for (auto& th : threads) th.join();
	for (int f : failures) EXPECT_EQ(0, f);
	EXPECT_EQ(g0 + 800u, Generation());
}

TEST_F(CommandInfoParamsEdgeTest, StressGenerationDeltaIsExactForConcurrentJsonRegistrations)
{
	const unsigned g0 = Generation();
	std::vector<std::thread> threads;
	std::vector<int> failures(4, 0);
	for (int t = 0; t < 4; t++)
	{
		threads.emplace_back([t, &failures]()
		{
			for (int i = 0; i < 200; i++)
			{
				const std::string n = NameFor("cipe.mt.json", t, i);
				CommandInfoJson c = MakeJson(n.c_str());
				c.params = kParamsB;
				c.paramCount = 3;
				if (!RegisterCommandJson(c)) failures[t]++;
				if (!UnregisterCommandJson(StringCRC(n.c_str()))) failures[t]++;
			}
		});
	}
	for (auto& th : threads) th.join();
	for (int f : failures) EXPECT_EQ(0, f);
	EXPECT_EQ(g0 + 800u, Generation());
}

TEST_F(CommandInfoParamsEdgeTest, StressReadersSeeConsistentParamsWhileWritersRegister)
{
	std::atomic<bool> stop{ false };
	std::atomic<int> bad{ 0 };
	std::thread reader([&]()
	{
		unsigned last = 0;
		while (!stop.load())
		{
			Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> list;
			ListCommandInfos(list);
			for (unsigned i = 0; i < list.Size(); i++)
			{
				if (list[i].params != kParamsA || list[i].paramCount != 2u) bad++;
			}
			const unsigned g = Generation();
			if (g < last) bad++;
			last = g;
		}
	});
	std::vector<std::thread> writers;
	for (int t = 0; t < 2; t++)
	{
		writers.emplace_back([t]()
		{
			for (int i = 0; i < 100; i++)
			{
				const std::string n = NameFor("cipe.mt.read", t, i);
				CommandInfo c = MakeInfo(n.c_str());
				c.params = kParamsA;
				c.paramCount = 2;
				RegisterCommand(c);
				UnregisterCommand(StringCRC(n.c_str()));
			}
		});
	}
	for (auto& w : writers) w.join();
	stop = true;
	reader.join();
	EXPECT_EQ(0, bad.load());
}
