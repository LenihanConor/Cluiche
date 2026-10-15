////////////////////////////////////////////////////////////////////////////////
// Filename: TestPaletteIndex.cpp
// Description: Unit tests for PaletteIndex and ResolvePaletteAction (AC-1..AC-7).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-palette.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/PaletteIndex.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdio>
#include <cstring>

using namespace Dia::VisualDebugger;
using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;

namespace
{
	class StubPaletteDomain : public IDebugDomain
	{
	public:
		StringCRC GetDomainId() const override { return StringCRC("PaletteStubDomain"); }
		const char* GetDisplayName() const override { return "Palette Stub"; }
		const char* GetDescription() const override { return "Stub domain for palette tests"; }
		StringCRC GetGroup() const override { return StringCRC("CoreDebug"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(); }
		bool HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value&) override {}
		void OnCommand(StringCRC, const Json::Value&) override {}
	};

	bool RegisterTestCommand(const char* name, const char* description, const char* category, const char* example)
	{
		Dia::API::CommandInfo info;
		info.name = StringCRC(name);
		info.description = description;
		info.category = StringCRC(category);
		info.owner = "TestPaletteIndex";
		info.version = "1.0.0";
		info.example = example;
		info.callback = [](const Dia::API::CommandArgs&) { return 0; };
		return Dia::API::RegisterCommand(info);
	}

	int FindByName(const PaletteIndex& idx, const char* name)
	{
		for (int i = 0; i < idx.GetItemCount(); ++i)
		{
			if (std::strcmp(idx.GetItem(i).name, name) == 0) return i;
		}
		return -1;
	}
}

class PaletteIndexTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!Dia::API::IsInitialized())
		{
			Dia::API::Initialize();
			mInitializedHere = true;
		}
		ASSERT_TRUE(RegisterTestCommand("bus_traffic.start", "Start bus traffic capture", "bus", "bus_traffic.start --rate=10"));
		ASSERT_TRUE(RegisterTestCommand("bus_traffic.stop", "Stop capture", "bus", ""));
		ASSERT_TRUE(RegisterTestCommand("xx_bus_other", "Other thing", "misc", "unrelated example"));
		ASSERT_TRUE(RegisterTestCommand("palette_zeta", "Sixth", "misc", ""));
		mDomains.Register(mDomain);
	}

	void TearDown() override
	{
		mDomains.Clear();
		Dia::API::UnregisterCommand(StringCRC("bus_traffic.start"));
		Dia::API::UnregisterCommand(StringCRC("bus_traffic.stop"));
		Dia::API::UnregisterCommand(StringCRC("xx_bus_other"));
		Dia::API::UnregisterCommand(StringCRC("palette_zeta"));
		if (mInitializedHere)
		{
			Dia::API::Shutdown();
		}
	}

	bool mInitializedHere = false;
	StubPaletteDomain mDomain;
	DiaDebugDomainRegistry mDomains;
};

TEST_F(PaletteIndexTest, AC1_BuildListsCommandsAndDomainsWithFields)
{
	PaletteIndex idx;
	idx.Build(mDomains);

	const int c = FindByName(idx, "bus_traffic.start");
	ASSERT_GE(c, 0);
	const PaletteItem& cmd = idx.GetItem(c);
	EXPECT_EQ(cmd.type, PaletteItemType::Command);
	EXPECT_STREQ(cmd.description, "Start bus traffic capture");
	EXPECT_STREQ(cmd.category, "bus");
	EXPECT_STREQ(cmd.example, "bus_traffic.start --rate=10");

	const int d = FindByName(idx, "Palette Stub");
	ASSERT_GE(d, 0);
	const PaletteItem& dom = idx.GetItem(d);
	EXPECT_EQ(dom.type, PaletteItemType::Domain);
	EXPECT_STREQ(dom.description, "Stub domain for palette tests");
	EXPECT_STREQ(dom.category, "CoreDebug");
	EXPECT_STREQ(dom.domainId, "PaletteStubDomain");

	EXPECT_EQ(idx.CountByType(PaletteItemType::Domain), 1);
	EXPECT_GE(idx.CountByType(PaletteItemType::Command), 4);
	EXPECT_EQ(idx.GetItemCount(), idx.CountByType(PaletteItemType::Command) + 1);
}

TEST_F(PaletteIndexTest, AC2_EmptyQueryReturnsEverythingAndNoiseReturnsNothing)
{
	PaletteIndex idx;
	idx.Build(mDomains);
	Dia::Core::Containers::DynamicArrayC<int, 50> out;

	const int total = idx.Search("", 3, out);
	EXPECT_EQ(total, idx.GetItemCount());
	EXPECT_EQ(static_cast<int>(out.Size()), total < 50 ? total : 50);
	for (unsigned i = 1; i < out.Size(); ++i)
	{
		EXPECT_LE(_stricmp(idx.GetItem(out[i - 1]).name, idx.GetItem(out[i]).name), 0);
	}

	EXPECT_EQ(idx.Search("zzzz", 3, out), 0);
	EXPECT_EQ(out.Size(), 0u);
}

TEST_F(PaletteIndexTest, AC3_RankingPrefixContainsSubsequence)
{
	PaletteIndex idx;
	idx.Build(mDomains);
	Dia::Core::Containers::DynamicArrayC<int, 50> out;

	ASSERT_GE(idx.Search("bus", 1, out), 3);
	EXPECT_EQ(std::strncmp(idx.GetItem(out[0]).name, "bus_traffic.", 12), 0);
	EXPECT_EQ(std::strncmp(idx.GetItem(out[1]).name, "bus_traffic.", 12), 0);
	EXPECT_STREQ(idx.GetItem(out[2]).name, "xx_bus_other");

	ASSERT_GE(idx.Search("bstrfc", 1, out), 1);
	bool found = false;
	for (unsigned i = 0; i < out.Size(); ++i)
	{
		if (std::strcmp(idx.GetItem(out[i]).name, "bus_traffic.start") == 0) found = true;
	}
	EXPECT_TRUE(found);
}

TEST_F(PaletteIndexTest, AC4_TypeFilterRestrictsWithoutReranking)
{
	PaletteIndex idx;
	idx.Build(mDomains);
	Dia::Core::Containers::DynamicArrayC<int, 50> both, cmds, doms;

	idx.Search("", 3, both);
	const int nCmds = idx.Search("", 1, cmds);
	const int nDoms = idx.Search("", 2, doms);
	EXPECT_EQ(nCmds, idx.CountByType(PaletteItemType::Command));
	EXPECT_EQ(nDoms, 1);
	for (unsigned i = 0; i < cmds.Size(); ++i)
	{
		EXPECT_EQ(idx.GetItem(cmds[i]).type, PaletteItemType::Command);
	}
	EXPECT_EQ(idx.GetItem(doms[0]).type, PaletteItemType::Domain);

	// Commands keep their relative order from the unfiltered ranking.
	unsigned k = 0;
	for (unsigned i = 0; i < both.Size() && k < cmds.Size(); ++i)
	{
		if (idx.GetItem(both[i]).type == PaletteItemType::Command)
		{
			EXPECT_EQ(both[i], cmds[k]);
			++k;
		}
	}
}

TEST_F(PaletteIndexTest, AC5_UnregisteredCommandAbsentAfterRebuild)
{
	PaletteIndex idx;
	idx.Build(mDomains);
	EXPECT_GE(FindByName(idx, "palette_zeta"), 0);

	ASSERT_TRUE(Dia::API::UnregisterCommand(StringCRC("palette_zeta")));
	idx.Build(mDomains);
	EXPECT_EQ(FindByName(idx, "palette_zeta"), -1);
}

TEST_F(PaletteIndexTest, AC6_ResultsCappedAtFiftyTotalIsTrueCount)
{
	char names[80][32];
	for (int i = 0; i < 80; ++i)
	{
		std::snprintf(names[i], sizeof(names[i]), "palcap_cmd_%02d", i);
		ASSERT_TRUE(RegisterTestCommand(names[i], "cap test", "cap", ""));
	}

	PaletteIndex idx;
	idx.Build(mDomains);
	Dia::Core::Containers::DynamicArrayC<int, 50> out;
	EXPECT_EQ(idx.Search("palcap_cmd", 1, out), 80);
	EXPECT_EQ(out.Size(), 50u);

	for (int i = 0; i < 80; ++i)
	{
		Dia::API::UnregisterCommand(StringCRC(names[i]));
	}
}

TEST_F(PaletteIndexTest, AC7_ResolveActions)
{
	PaletteIndex idx;
	idx.Build(mDomains);

	PaletteAction a = ResolvePaletteAction(idx.GetItem(FindByName(idx, "bus_traffic.start")));
	EXPECT_EQ(a.kind, PaletteAction::FillConsole);
	EXPECT_STREQ(a.text, "bus_traffic.start --rate=10");

	a = ResolvePaletteAction(idx.GetItem(FindByName(idx, "bus_traffic.stop")));
	EXPECT_EQ(a.kind, PaletteAction::FillConsole);
	EXPECT_STREQ(a.text, "bus_traffic.stop");

	a = ResolvePaletteAction(idx.GetItem(FindByName(idx, "xx_bus_other")));
	EXPECT_EQ(a.kind, PaletteAction::FillConsole);
	EXPECT_STREQ(a.text, "xx_bus_other");

	a = ResolvePaletteAction(idx.GetItem(FindByName(idx, "Palette Stub")));
	EXPECT_EQ(a.kind, PaletteAction::RevealDomain);
	// The panel page keys domain cards by display name (DebugPanelPageModule roots the JSON at
	// GetDisplayName()), so the reveal key is the display name, not GetDomainId().
	EXPECT_STREQ(a.text, "Palette Stub");
}

#endif // DIA_DEBUG
