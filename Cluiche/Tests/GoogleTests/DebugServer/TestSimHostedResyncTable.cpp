#include <gtest/gtest.h>
#include <DiaDebugServer/Testing/SimHelpTesting.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <Types/SimRegistrationMessage.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ReplaceSimHosted / GetSimHostedDigest: the whole-table swap and digest used by the announcement resync.

namespace
{
	using Dia::Core::StringCRC;
	using Dia::DebugServer::DebugServer;
	using Dia::DebugServer::SimHostedName;
	using Dia::DebugServer::SimKind;

	SimHostedName Make(const char* name, SimKind kind = SimKind::Query)
	{
		SimHostedName n;
		std::memset(&n, 0, sizeof(n));
		std::strncpy(n.name, name, sizeof(n.name) - 1);
		n.kind = kind;
		n.help = &Dia::DebugServer::Testing::DefaultSimHelp();
		return n;
	}

	uint32_t SetHash(const std::vector<SimHostedName>& v)
	{
		uint32_t h = 0;
		for (const SimHostedName& n : v)
			h += Cluiche::AppFlow::SimNameSetEntryHash(n.name, n.kind);
		return h;
	}

	void Digest(const DebugServer& s, uint32_t& count, uint32_t& hash) { s.GetSimHostedDigest(count, hash); }
}

TEST(SimHostedResyncTable, ReplaceSwapsTableAndRemovedNamesBecomeUnknown)
{
	DebugServer s;
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("old.a"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("keep.b"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));

	const SimHostedName next[] = { Make("keep.b"), Make("new.c", SimKind::Command) };
	ASSERT_TRUE(s.ReplaceSimHosted(next, 2));

	uint32_t count = 0, hash = 0;
	Digest(s, count, hash);
	EXPECT_EQ(2u, count);
	EXPECT_EQ(SetHash({ next[0], next[1] }), hash);

	// old.a is gone: it can be registered afresh; new.c is taken.
	EXPECT_TRUE(s.RegisterSimHosted(StringCRC("old.a"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	EXPECT_FALSE(s.RegisterSimHosted(StringCRC("new.c"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	EXPECT_FALSE(s.RegisterSimHosted(StringCRC("keep.b"), SimKind::Command, Dia::DebugServer::Testing::DefaultSimHelp()));
}

TEST(SimHostedResyncTable, ReplaceWithEmptyClearsTable)
{
	DebugServer s;
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("a"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	ASSERT_TRUE(s.ReplaceSimHosted(nullptr, 0));
	uint32_t count = 99, hash = 99;
	Digest(s, count, hash);
	EXPECT_EQ(0u, count);
	EXPECT_EQ(0u, hash);
}

TEST(SimHostedResyncTable, ReplaceRejectsNullArrayWithCount)
{
	DebugServer s;
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("a"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	Dia::CellLayers::Testing::LogCapture log("DebugServer");
	EXPECT_FALSE(s.ReplaceSimHosted(nullptr, 3));
	EXPECT_EQ(1u, log.Errors("null name array"));
	uint32_t count = 0, hash = 0;
	Digest(s, count, hash);
	EXPECT_EQ(1u, count);
}

TEST(SimHostedResyncTable, ReplaceRejectsEmptyNameAndLeavesTableIntact)
{
	DebugServer s;
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("a"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	uint32_t c0 = 0, h0 = 0;
	Digest(s, c0, h0);

	const SimHostedName next[] = { Make("ok"), Make("") };
	EXPECT_FALSE(s.ReplaceSimHosted(next, 2));
	uint32_t c1 = 0, h1 = 0;
	Digest(s, c1, h1);
	EXPECT_EQ(c0, c1);
	EXPECT_EQ(h0, h1);
}

TEST(SimHostedResyncTable, ReplaceOverCapacityRejectsWholeSwapAndLeavesTableIntact)
{
	DebugServer s;
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("a"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	uint32_t c0 = 0, h0 = 0;
	Digest(s, c0, h0);

	std::vector<SimHostedName> big;
	for (int i = 0; i < 65; ++i)
		big.push_back(Make(("cap" + std::to_string(i)).c_str()));
	Dia::CellLayers::Testing::LogCapture log("DebugServer");
	EXPECT_FALSE(s.ReplaceSimHosted(big.data(), static_cast<uint32_t>(big.size())));
	EXPECT_EQ(1u, log.Errors("exceed capacity"));

	uint32_t c1 = 0, h1 = 0;
	Digest(s, c1, h1);
	EXPECT_EQ(c0, c1);
	EXPECT_EQ(h0, h1);
	EXPECT_FALSE(s.RegisterSimHosted(StringCRC("a"), SimKind::Command, Dia::DebugServer::Testing::DefaultSimHelp()));   // still there (kind conflict)

	// Exactly 64 fits.
	big.pop_back();
	EXPECT_TRUE(s.ReplaceSimHosted(big.data(), 64));
	Digest(s, c1, h1);
	EXPECT_EQ(64u, c1);
}

TEST(SimHostedResyncTable, ReplaceDuplicatesLastWinsAndCountsOnce)
{
	DebugServer s;
	const SimHostedName next[] = { Make("dup", SimKind::Query), Make("other"), Make("dup", SimKind::Command) };
	ASSERT_TRUE(s.ReplaceSimHosted(next, 3));
	uint32_t count = 0, hash = 0;
	Digest(s, count, hash);
	EXPECT_EQ(2u, count);
	EXPECT_EQ(SetHash({ next[2], next[1] }), hash);   // kind of the last 'dup' is Command
}

TEST(SimHostedResyncTable, DuplicatesDoNotCountTowardCapacity)
{
	DebugServer s;
	std::vector<SimHostedName> v;
	for (int i = 0; i < 40; ++i)
		v.push_back(Make("same"));
	EXPECT_TRUE(s.ReplaceSimHosted(v.data(), 40));
	uint32_t count = 0, hash = 0;
	Digest(s, count, hash);
	EXPECT_EQ(1u, count);
}

TEST(SimHostedResyncTable, ReplaceRejectsNameAlreadyInQueryRegistry)
{
	DebugServer s;
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("a"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	s.GetQueryRegistry().Register(StringCRC("local.q"), [](const Json::Value&) { return Json::Value(); });
	ASSERT_TRUE(s.GetQueryRegistry().Has(StringCRC("local.q")));

	const SimHostedName next[] = { Make("fine"), Make("local.q") };
	EXPECT_FALSE(s.ReplaceSimHosted(next, 2));
	uint32_t count = 0, hash = 0;
	Digest(s, count, hash);
	EXPECT_EQ(1u, count);
}

TEST(SimHostedResyncTable, DigestOfEmptyTableIsZero)
{
	DebugServer s;
	uint32_t count = 7, hash = 7;
	s.GetSimHostedDigest(count, hash);
	EXPECT_EQ(0u, count);
	EXPECT_EQ(0u, hash);
}

TEST(SimHostedResyncTable, DigestIsOrderIndependentAndMatchesMessageHelpers)
{
	DebugServer a, b;
	ASSERT_TRUE(a.RegisterSimHosted(StringCRC("x.one"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	ASSERT_TRUE(a.RegisterSimHosted(StringCRC("x.two"), SimKind::Command, Dia::DebugServer::Testing::DefaultSimHelp()));
	ASSERT_TRUE(a.RegisterSimHosted(StringCRC("x.three"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	ASSERT_TRUE(b.RegisterSimHosted(StringCRC("x.three"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	ASSERT_TRUE(b.RegisterSimHosted(StringCRC("x.one"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	ASSERT_TRUE(b.RegisterSimHosted(StringCRC("x.two"), SimKind::Command, Dia::DebugServer::Testing::DefaultSimHelp()));

	uint32_t ca = 0, ha = 0, cb = 0, hb = 0;
	a.GetSimHostedDigest(ca, ha);
	b.GetSimHostedDigest(cb, hb);
	EXPECT_EQ(3u, ca);
	EXPECT_EQ(ca, cb);
	EXPECT_EQ(ha, hb);
	EXPECT_EQ(SetHash({ Make("x.one"), Make("x.two", SimKind::Command), Make("x.three") }), ha);
}

TEST(SimHostedResyncTable, DigestChangesWithKindRegisterAndUnregister)
{
	DebugServer s;
	uint32_t c = 0, h0 = 0, h1 = 0, h2 = 0;
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("q"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	s.GetSimHostedDigest(c, h0);
	ASSERT_TRUE(s.RegisterSimHosted(StringCRC("r"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	s.GetSimHostedDigest(c, h1);
	EXPECT_EQ(2u, c);
	EXPECT_NE(h0, h1);
	s.UnregisterSimHosted(StringCRC("r"));
	s.GetSimHostedDigest(c, h2);
	EXPECT_EQ(1u, c);
	EXPECT_EQ(h0, h2);

	const SimHostedName asCommand[] = { Make("q", SimKind::Command) };
	ASSERT_TRUE(s.ReplaceSimHosted(asCommand, 1));
	s.GetSimHostedDigest(c, h2);
	EXPECT_NE(h0, h2);
}
