#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugServer/Testing/SimHelpTesting.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Sim-Hosted Command Help, Task 3: the Main-side catalogue kept with the sim-hosted table.
// Host-module level cases (register via message, resync repair, teardown) live in TestSimAnnounceResyncIntegration.cpp.

namespace
{
	using Dia::CellLayers::Testing::LogCapture;
	using Dia::Core::StringCRC;
	using Dia::DebugServer::DebugServer;
	using Dia::DebugServer::SimHelpStored;
	using Dia::DebugServer::SimHostedEntryView;
	using Dia::DebugServer::SimHostedName;
	using Dia::DebugServer::SimKind;
	using Dia::DebugServer::Testing::MakeSimHelp;

	// DebugServer holds ~7 KB per sim name on the heap, but keep the server itself off the stack anyway.
	std::unique_ptr<DebugServer> NewServer() { return std::make_unique<DebugServer>(); }

	SimHostedName Entry(const char* name, SimKind kind, const SimHelpStored* help)
	{
		SimHostedName n;
		std::memset(&n, 0, sizeof(n));
		std::strncpy(n.name, name, sizeof(n.name) - 1);
		n.kind = kind;
		n.help = help;
		return n;
	}

	std::string Desc(const DebugServer& s, const char* name)
	{
		const SimHelpStored* h = s.FindSimHostedHelp(StringCRC(name));
		return h != nullptr ? std::string(h->description) : std::string("<none>");
	}
}

TEST(SimHostedCatalogue, RegisterStoresHelpAndKind)
{
	auto s = NewServer();
	SimHelpStored help = MakeSimHelp("lists the cells");
	std::strcpy(help.category, "cells");
	std::strcpy(help.owner, "DiaCellLayers");
	help.noArgs = true;
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.list"), SimKind::Command, help));

	EXPECT_EQ(1u, s->SimHostedCount());
	const SimHelpStored* found = s->FindSimHostedHelp(StringCRC("cat.list"));
	ASSERT_NE(nullptr, found);
	EXPECT_STREQ("lists the cells", found->description);
	EXPECT_STREQ("cells", found->category);
	EXPECT_STREQ("DiaCellLayers", found->owner);
	EXPECT_TRUE(found->noArgs);

	SimHostedEntryView view;
	ASSERT_TRUE(s->GetSimHostedEntry(0, view));
	EXPECT_EQ(StringCRC("cat.list"), view.name);
	EXPECT_EQ(SimKind::Command, view.kind);
	EXPECT_EQ(found, view.help);
}

TEST(SimHostedCatalogue, StoredHelpIsACopyNotAReference)
{
	auto s = NewServer();
	SimHelpStored help = MakeSimHelp("original");
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.copy"), SimKind::Query, help));
	std::strcpy(help.description, "mutated after the call");
	EXPECT_EQ("original", Desc(*s, "cat.copy"));
}

TEST(SimHostedCatalogue, UnknownNameAndOutOfRangeIndexAreEmpty)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.one"), SimKind::Query, MakeSimHelp()));
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("cat.other")));
	SimHostedEntryView view;
	EXPECT_FALSE(s->GetSimHostedEntry(1, view));
	EXPECT_FALSE(s->GetSimHostedEntry(1000, view));
}

// AC 12
TEST(SimHostedCatalogue, UnregisterRemovesTheEntryAndItsHelp)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.a"), SimKind::Query, MakeSimHelp("a")));
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.b"), SimKind::Query, MakeSimHelp("b")));
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.c"), SimKind::Command, MakeSimHelp("c")));

	s->UnregisterSimHosted(StringCRC("cat.b"));

	EXPECT_EQ(2u, s->SimHostedCount());
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("cat.b")));
	// The survivors keep their own help after the shift.
	EXPECT_EQ("a", Desc(*s, "cat.a"));
	EXPECT_EQ("c", Desc(*s, "cat.c"));
	SimHostedEntryView view;
	ASSERT_TRUE(s->GetSimHostedEntry(1, view));
	EXPECT_EQ(StringCRC("cat.c"), view.name);
	EXPECT_EQ(SimKind::Command, view.kind);
	EXPECT_STREQ("c", view.help->description);

	s->UnregisterSimHosted(StringCRC("cat.unknown"));   // no-op
	EXPECT_EQ(2u, s->SimHostedCount());
}

// AC 4: same kind re-registers and replaces the help.
TEST(SimHostedCatalogue, ReRegisterWithSameKindReplacesTheEntry)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.r"), SimKind::Query, MakeSimHelp("old")));
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.r"), SimKind::Query, MakeSimHelp("new")));
	EXPECT_EQ(1u, s->SimHostedCount());
	EXPECT_EQ("new", Desc(*s, "cat.r"));
}

// A kind change is a conflict, not a replacement: the original entry stays untouched.
TEST(SimHostedCatalogue, ReRegisterWithDifferentKindIsRejectedAndKeepsTheOriginal)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.k"), SimKind::Query, MakeSimHelp("orig")));
	LogCapture log("DebugServer");
	EXPECT_FALSE(s->RegisterSimHosted(StringCRC("cat.k"), SimKind::Command, MakeSimHelp("other")));
	EXPECT_EQ(1u, log.Errors("already sim-hosted"));
	EXPECT_EQ("orig", Desc(*s, "cat.k"));
	SimHostedEntryView view;
	ASSERT_TRUE(s->GetSimHostedEntry(0, view));
	EXPECT_EQ(SimKind::Query, view.kind);
}

TEST(SimHostedCatalogue, FullCatalogueRejectsTheSixtyFifthNameAndKeepsTheRest)
{
	auto s = NewServer();
	for (int i = 0; i < 64; ++i)
	{
		const std::string n = "cat.n" + std::to_string(i);
		ASSERT_TRUE(s->RegisterSimHosted(StringCRC(n.c_str()), SimKind::Query, MakeSimHelp(n.c_str()))) << n;
	}
	LogCapture log("DebugServer");
	EXPECT_FALSE(s->RegisterSimHosted(StringCRC("cat.extra"), SimKind::Query, MakeSimHelp("x")));
	EXPECT_EQ(1u, log.Errors("capacity"));
	EXPECT_EQ(64u, s->SimHostedCount());
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("cat.extra")));
	EXPECT_EQ("cat.n63", Desc(*s, "cat.n63"));

	// Re-registering an existing name at capacity is a replacement, not an overflow.
	EXPECT_TRUE(s->RegisterSimHosted(StringCRC("cat.n7"), SimKind::Query, MakeSimHelp("updated")));
	EXPECT_EQ("updated", Desc(*s, "cat.n7"));
}

TEST(SimHostedCatalogue, NameInQueryRegistryIsVetoedAndStoresNoHelp)
{
	auto s = NewServer();
	s->GetQueryRegistry().Register(StringCRC("cat.main"), [](const Json::Value&) { return Json::Value(); });
	EXPECT_FALSE(s->RegisterSimHosted(StringCRC("cat.main"), SimKind::Query, MakeSimHelp()));
	EXPECT_EQ(0u, s->SimHostedCount());
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("cat.main")));
}

// AC 9: sim-hosted names never enter the DiaAPI CommandRegistry.
TEST(SimHostedCatalogue, SimHostedNamesAreNotInTheDiaApiCommandRegistry)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.notindiaapi"), SimKind::Command, MakeSimHelp()));
	EXPECT_EQ(nullptr, Dia::API::GetCommand(StringCRC("cat.notindiaapi")));
}

TEST(SimHostedCatalogue, DigestIgnoresHelp)
{
	auto a = NewServer();
	auto b = NewServer();
	ASSERT_TRUE(a->RegisterSimHosted(StringCRC("cat.d"), SimKind::Query, MakeSimHelp("one")));
	ASSERT_TRUE(b->RegisterSimHosted(StringCRC("cat.d"), SimKind::Query, MakeSimHelp("a very different help")));
	uint32_t ac = 0, ah = 0, bc = 0, bh = 0;
	a->GetSimHostedDigest(ac, ah);
	b->GetSimHostedDigest(bc, bh);
	EXPECT_EQ(ac, bc);
	EXPECT_EQ(ah, bh);
}

TEST(SimHostedCatalogue, ReplaceSwapsTableAndCatalogueTogether)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("old.a"), SimKind::Query, MakeSimHelp("old a")));
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("keep.b"), SimKind::Query, MakeSimHelp("old b")));

	const SimHelpStored keepHelp = MakeSimHelp("fresh b");
	const SimHelpStored newHelp  = MakeSimHelp("fresh c");
	const SimHostedName next[] = { Entry("keep.b", SimKind::Query, &keepHelp), Entry("new.c", SimKind::Command, &newHelp) };
	ASSERT_TRUE(s->ReplaceSimHosted(next, 2));

	EXPECT_EQ(2u, s->SimHostedCount());
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("old.a")));   // removed name lost its help
	EXPECT_EQ("fresh b", Desc(*s, "keep.b"));                       // kept name got the new help
	EXPECT_EQ("fresh c", Desc(*s, "new.c"));                        // new name has help
	SimHostedEntryView view;
	ASSERT_TRUE(s->GetSimHostedEntry(1, view));
	EXPECT_EQ(SimKind::Command, view.kind);
}

TEST(SimHostedCatalogue, ReplaceWithNothingClearsTheCatalogue)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cat.x"), SimKind::Query, MakeSimHelp()));
	ASSERT_TRUE(s->ReplaceSimHosted(nullptr, 0));
	EXPECT_EQ(0u, s->SimHostedCount());
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("cat.x")));
}

TEST(SimHostedCatalogue, ReplaceDuplicateNameLastEntryWinsIncludingItsHelp)
{
	auto s = NewServer();
	const SimHelpStored first = MakeSimHelp("first"), last = MakeSimHelp("last"), other = MakeSimHelp("other");
	const SimHostedName next[] = { Entry("dup", SimKind::Query, &first), Entry("other", SimKind::Query, &other), Entry("dup", SimKind::Command, &last) };
	ASSERT_TRUE(s->ReplaceSimHosted(next, 3));
	EXPECT_EQ(2u, s->SimHostedCount());
	EXPECT_EQ("last", Desc(*s, "dup"));
	EXPECT_EQ("other", Desc(*s, "other"));
	SimHostedEntryView view;
	ASSERT_TRUE(s->GetSimHostedEntry(0, view));
	EXPECT_EQ(SimKind::Command, view.kind);
}

TEST(SimHostedCatalogue, FailedReplaceLeavesTableAndCatalogueUntouched)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("live.a"), SimKind::Query, MakeSimHelp("live a")));
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("live.b"), SimKind::Command, MakeSimHelp("live b")));
	s->GetQueryRegistry().Register(StringCRC("main.q"), [](const Json::Value&) { return Json::Value(); });

	const SimHelpStored h = MakeSimHelp("replacement");
	LogCapture log("DebugServer");

	// 1: a name that exists in the QueryRegistry.
	{
		const SimHostedName bad[] = { Entry("fine", SimKind::Query, &h), Entry("main.q", SimKind::Query, &h) };
		EXPECT_FALSE(s->ReplaceSimHosted(bad, 2));
	}
	// 2: an empty name.
	{
		const SimHostedName bad[] = { Entry("fine", SimKind::Query, &h), Entry("", SimKind::Query, &h) };
		EXPECT_FALSE(s->ReplaceSimHosted(bad, 2));
	}
	// 3: a missing help pointer.
	{
		const SimHostedName bad[] = { Entry("fine", SimKind::Query, &h), Entry("nohelp", SimKind::Query, nullptr) };
		EXPECT_FALSE(s->ReplaceSimHosted(bad, 2));
	}
	// 4: more distinct names than the cap.
	{
		std::vector<SimHostedName> big;
		for (int i = 0; i < 65; ++i)
			big.push_back(Entry(("big.n" + std::to_string(i)).c_str(), SimKind::Query, &h));
		EXPECT_FALSE(s->ReplaceSimHosted(big.data(), static_cast<uint32_t>(big.size())));
	}
	EXPECT_GE(log.Errors(""), 4u);

	EXPECT_EQ(2u, s->SimHostedCount());
	EXPECT_EQ("live a", Desc(*s, "live.a"));
	EXPECT_EQ("live b", Desc(*s, "live.b"));
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("fine")));
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("big.n0")));
}

TEST(SimHostedCatalogue, ReplaceFillsAllSixtyFourSlotsWithTheirOwnHelp)
{
	auto s = NewServer();
	std::vector<SimHelpStored> helps;
	std::vector<SimHostedName> names;
	for (int i = 0; i < 64; ++i)
		helps.push_back(MakeSimHelp(("help " + std::to_string(i)).c_str()));
	for (int i = 0; i < 64; ++i)
		names.push_back(Entry(("full.n" + std::to_string(i)).c_str(), SimKind::Query, &helps[i]));
	ASSERT_TRUE(s->ReplaceSimHosted(names.data(), 64));
	EXPECT_EQ(64u, s->SimHostedCount());
	for (int i = 0; i < 64; ++i)
		EXPECT_EQ("help " + std::to_string(i), Desc(*s, ("full.n" + std::to_string(i)).c_str())) << i;
}

// ---- Exhaustive pass (Task 7) ----

namespace
{
	// A help with every field at its limit and 16 params, the discriminator 'tag' in the description (so entries differ).
	SimHelpStored MaximalHelp(char tag)
	{
		using namespace Dia::DebugServer;
		const std::string desc(kSimHelpMaxDescription, tag), cat(kSimHelpMaxCategory, 'c'), owner(kSimHelpMaxOwner, 'o');
		const std::string pn(kSimHelpMaxParamName, 'n'), pt(kSimHelpMaxParamType, 't'), pd(kSimHelpMaxParamDescription, tag);
		const std::string pu(kSimHelpMaxParamUnit, 'u'), pe(kSimHelpMaxParamEnum, 'e');
		std::vector<Dia::API::ValueParam> params(kSimHelpMaxParams);
		for (auto& p : params)
			p = { pn.c_str(), pt.c_str(), true, pd.c_str(), pu.c_str(), true, -1.0f, 1.0f, pe.c_str() };
		SimHelpStored out;
		std::memset(&out, 0, sizeof(out));
		EXPECT_EQ(CopyHelp(SimHelpInfo{ desc.c_str(), cat.c_str(), owner.c_str(), params.data(), kSimHelpMaxParams, false }, out), SimHelpResult::Ok);
		return out;
	}
}

TEST(SimHostedCatalogue, ReplaceWithSixtyFourMaximalHelpsSucceedsAndASixtyFifthRejectsTheWholeSwap)
{
	auto s = NewServer();
	std::vector<std::unique_ptr<SimHelpStored>> helps;
	std::vector<SimHostedName> names;
	for (int i = 0; i < 65; ++i)
	{
		helps.emplace_back(new SimHelpStored(MaximalHelp(static_cast<char>('A' + i % 26))));
		names.push_back(Entry(("max.n" + std::to_string(i)).c_str(), i % 2 ? SimKind::Command : SimKind::Query, helps.back().get()));
	}

	ASSERT_TRUE(s->ReplaceSimHosted(names.data(), 64));
	ASSERT_EQ(64u, s->SimHostedCount());
	for (int i = 0; i < 64; ++i)
	{
		const SimHelpStored* h = s->FindSimHostedHelp(StringCRC(("max.n" + std::to_string(i)).c_str()));
		ASSERT_NE(nullptr, h) << i;
		EXPECT_EQ(0, std::memcmp(h, helps[i].get(), sizeof(SimHelpStored))) << i;
		EXPECT_NE(h, helps[i].get()) << "the catalogue must hold a copy";
	}

	// 65 distinct names: the whole swap is refused, the previous 64 stay exactly as they were.
	LogCapture log("DebugServer");
	EXPECT_FALSE(s->ReplaceSimHosted(names.data(), 65));
	EXPECT_GE(log.Errors("ReplaceSimHosted"), 1u);
	ASSERT_EQ(64u, s->SimHostedCount());
	for (int i = 0; i < 64; ++i)
	{
		const SimHelpStored* h = s->FindSimHostedHelp(StringCRC(("max.n" + std::to_string(i)).c_str()));
		ASSERT_NE(nullptr, h) << i;
		EXPECT_EQ(0, std::memcmp(h, helps[i].get(), sizeof(SimHelpStored))) << i;
	}
	EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC("max.n64")));
}

TEST(SimHostedCatalogue, RegisterUnregisterChurnAtCapacityKeepsHelpParallelToTheTable)
{
	auto s = NewServer();
	for (int i = 0; i < 64; ++i)
	{
		const std::string n = "churn.n" + std::to_string(i);
		ASSERT_TRUE(s->RegisterSimHosted(StringCRC(n.c_str()), SimKind::Query, MakeSimHelp(n.c_str()))) << n;
	}
	for (int round = 0; round < 3; ++round)
	{
		for (int i = round; i < 64; i += 3)
		{
			const std::string n = "churn.n" + std::to_string(i);
			s->UnregisterSimHosted(StringCRC(n.c_str()));
			EXPECT_EQ(nullptr, s->FindSimHostedHelp(StringCRC(n.c_str()))) << n;
			ASSERT_TRUE(s->RegisterSimHosted(StringCRC(n.c_str()), SimKind::Command, MakeSimHelp((n + ".v" + std::to_string(round)).c_str()))) << n;
		}
	}
	ASSERT_EQ(64u, s->SimHostedCount());
	for (uint64_t i = 0; i < 32; ++i)
	{
		SimHostedEntryView v;
		ASSERT_TRUE(s->GetSimHostedEntry(i, v));
		ASSERT_NE(nullptr, v.help);
		// Every entry's help description starts with its own name: the table and the catalogue never drift apart.
		EXPECT_EQ(0, std::strncmp(v.help->description, v.name.AsChar(), std::strlen(v.name.AsChar()))) << v.name.AsChar() << " -> " << v.help->description;
	}
}

TEST(SimHostedCatalogue, ReplaceWithMixedNullHelpRejectsWholeSwapAndKeepsOldHelp)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("keep.me"), SimKind::Query, MakeSimHelp("kept")));
	SimHelpStored good = MakeSimHelp("new");
	SimHostedName names[3] = { Entry("a.one", SimKind::Query, &good), Entry("a.two", SimKind::Query, &good), Entry("a.three", SimKind::Query, nullptr) };
	LogCapture log("DebugServer");
	EXPECT_FALSE(s->ReplaceSimHosted(names, 3));
	EXPECT_GE(log.Errors("a.three"), 1u);
	EXPECT_EQ(1u, s->SimHostedCount());
	EXPECT_EQ("kept", Desc(*s, "keep.me"));
	EXPECT_EQ("<none>", Desc(*s, "a.one"));
}
