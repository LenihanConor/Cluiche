#include <gtest/gtest.h>
#include <DiaValueSchema/CodeSideCollector.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimHostedCatalogue.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <memory>
#include <vector>
#include <string>

// Sim-Hosted Command Help, Task 4 (AC 7): the code-side collector includes sim-hosted names read from an injected
// ISimHostedCatalogue and treats them exactly like DiaAPI commands.

using namespace Dia::ValueSchema;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;
using Dia::DebugServer::CopyHelp;
using Dia::DebugServer::DebugServer;
using Dia::DebugServer::SimHelpInfo;
using Dia::DebugServer::SimHelpStored;
using Dia::DebugServer::SimKind;

namespace
{
	SimHelpStored Help(const char* description, const Dia::API::ValueParam* params, unsigned count, bool noArgs = false)
	{
		SimHelpInfo info;
		info.description = description;
		info.category = "cells";
		info.owner = "DiaCellLayers";
		info.params = params;
		info.paramCount = count;
		info.noArgs = noArgs;
		SimHelpStored out;
		CopyHelp(info, out);
		return out;
	}

	const ValueInfo* FindCmd(const ValueSchemaRegistry& r, const char* key)
	{
		return r.Find(StringCRC("dia.command"), StringCRC(key));
	}

	bool Listed(const ValueList<UndescribedItem>& list, const char* name)
	{
		for (unsigned i = 0; i < list.Size(); ++i)
		{
			if (std::strcmp(list[i].mName.AsCStr(), name) == 0) { return true; }
		}
		return false;
	}

	class CodeSideSimHostedTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			if (!Dia::API::IsInitialized()) { Dia::API::Initialize(); mInitializedHere = true; }
			mServer = std::make_unique<DebugServer>();
		}
		void TearDown() override
		{
			Dia::API::UnregisterCommand(StringCRC("csim.dup"));
			if (mInitializedHere) { Dia::API::Shutdown(); }
		}

		bool mInitializedHere = false;
		std::unique_ptr<DebugServer> mServer;
	};
}

TEST_F(CodeSideSimHostedTest, SimNameBecomesACommandEntryWithOwnerCategoryAndArgs)
{
	static const Dia::API::ValueParam params[] = {
		{ "x", "int", true, "the column" },
		{ "mode", "enum", false, "how", nullptr, false, 0.0f, 0.0f, "a|b" },
	};
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.cells"), SimKind::Command, Help("Select cells", params, 2)));

	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);

	const ValueInfo* cmd = FindCmd(reg, "csim.cells");
	ASSERT_NE(nullptr, cmd);
	EXPECT_EQ(ValueKind::Command, cmd->mKind);
	EXPECT_STREQ("Select cells", cmd->mDescription.AsCStr());
	EXPECT_EQ("DiaCellLayers", cmd->mUi["owner"].asString());
	EXPECT_EQ("cells", cmd->mUi["category"].asString());
	ASSERT_EQ(2u, cmd->mRelated.Size());
	EXPECT_STREQ("csim.cells.x", cmd->mRelated[0].AsCStr());

	const ValueInfo* arg = FindCmd(reg, "csim.cells.x");
	ASSERT_NE(nullptr, arg);
	EXPECT_EQ(ValueKind::Arg, arg->mKind);
	EXPECT_EQ(ValueType::Int, arg->mType);
	EXPECT_TRUE(arg->mRequired);
	const ValueInfo* mode = FindCmd(reg, "csim.cells.mode");
	ASSERT_NE(nullptr, mode);
	EXPECT_EQ(ValueType::Enum, mode->mType);
	EXPECT_EQ(2u, mode->mEnumValues.Size());
	EXPECT_FALSE(Listed(rep.mUndescribedCommands, "csim.cells"));
	EXPECT_FALSE(Listed(rep.mUndescribedParams, "csim.cells.x"));
}

TEST_F(CodeSideSimHostedTest, SimNameWithoutParamsOrNoArgsIsListedUndescribedLikeADiaApiOne)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.bare"), SimKind::Command, Help("Bare", nullptr, 0, false)));
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.noargs"), SimKind::Command, Help("Takes none", nullptr, 0, true)));

	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);

	EXPECT_NE(nullptr, FindCmd(reg, "csim.bare"));
	EXPECT_TRUE(Listed(rep.mUndescribedCommands, "csim.bare"));
	EXPECT_NE(nullptr, FindCmd(reg, "csim.noargs"));
	EXPECT_FALSE(Listed(rep.mUndescribedCommands, "csim.noargs"));
}

TEST_F(CodeSideSimHostedTest, SimParamWithoutDescriptionIsAnUndescribedParam)
{
	static const Dia::API::ValueParam params[] = { { "x", "int", true, "" } };
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.nodesc"), SimKind::Command, Help("Has a bare param", params, 1)));

	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);
	EXPECT_TRUE(Listed(rep.mUndescribedParams, "csim.nodesc.x"));
}

TEST_F(CodeSideSimHostedTest, NameInBothIsCollectedOnceFromTheDiaApiEntryWithAWarning)
{
	Dia::API::CommandInfo info;
	info.name = StringCRC("csim.dup");
	info.description = "from the api";
	info.category = StringCRC("apicat");
	info.owner = "TestCodeSideSimHosted";
	info.version = "1.0.0";
	info.example = "";
	info.noArgs = true;
	info.callback = [](const Dia::API::CommandArgs&) { return 0; };
	ASSERT_TRUE(Dia::API::RegisterCommand(info));
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.dup"), SimKind::Command, Help("from the sim", nullptr, 0, true)));

	LogCapture log("VSC");
	LiveCodeSideSource live;
	ValueSchemaRegistry baseReg;
	// Baseline: other tests in the same process can leave global metrics behind that collide on their own, so the
	// duplicate count is compared against a collection without the sim catalogue.
	const unsigned baseDuplicates = CodeSideCollector().Collect(live, baseReg).mDuplicateCount;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);

	const ValueInfo* cmd = FindCmd(reg, "csim.dup");
	ASSERT_NE(nullptr, cmd);
	EXPECT_STREQ("from the api", cmd->mDescription.AsCStr());
	EXPECT_EQ(baseDuplicates, rep.mDuplicateCount);	// never reached the registry: skipped before it
	EXPECT_EQ(1u, log.Warnings("csim.dup"));
}

TEST_F(CodeSideSimHostedTest, EmptyOrAbsentCatalogueChangesNothing)
{
	LiveCodeSideSource plain;
	ValueSchemaRegistry regPlain;
	const CodeSideReport a = CodeSideCollector().Collect(plain, regPlain);

	LiveCodeSideSource withEmpty;
	withEmpty.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry regEmpty;
	const CodeSideReport b = CodeSideCollector().Collect(withEmpty, regEmpty);
	EXPECT_EQ(a.mCommandsCollected, b.mCommandsCollected);
	EXPECT_EQ(a.mArgsCollected, b.mArgsCollected);
	EXPECT_EQ(regPlain.Count(), regEmpty.Count());
}

TEST_F(CodeSideSimHostedTest, UnregisteredNameIsAbsentFromTheNextCollection)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.bare"), SimKind::Command, Help("Bare", nullptr, 0, true)));
	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	{
		ValueSchemaRegistry reg;
		CodeSideCollector().Collect(live, reg);
		EXPECT_NE(nullptr, FindCmd(reg, "csim.bare"));
	}
	mServer->UnregisterSimHosted(StringCRC("csim.bare"));
	ValueSchemaRegistry reg;
	CodeSideCollector().Collect(live, reg);
	EXPECT_EQ(nullptr, FindCmd(reg, "csim.bare"));
}

TEST_F(CodeSideSimHostedTest, CommandGenerationRisesWhenSimNamesAreRegisteredLate)
{
	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	const unsigned before = live.CommandGeneration();
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.late"), SimKind::Command, Help("Late", nullptr, 0, true)));
	EXPECT_GT(live.CommandGeneration(), before);
}

TEST_F(CodeSideSimHostedTest, SimNamesStayOutOfTheDiaApiRegistry)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.bare"), SimKind::Command, Help("Bare", nullptr, 0, true)));
	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	CodeSideCollector().Collect(live, reg);
	EXPECT_EQ(nullptr, Dia::API::GetCommand(StringCRC("csim.bare")));
}

// ---- Exhaustive pass (Task 7) ----

namespace
{
	// Rejections the collector reports without any sim catalogue (other tests can leave odd global metrics behind).
	unsigned BaselineRejected()
	{
		LiveCodeSideSource live;
		ValueSchemaRegistry reg;
		return CodeSideCollector().Collect(live, reg).mRejectedCount;
	}

	SimHelpStored SixteenParamHelp(const std::string& description)
	{
		static std::vector<std::string> names;
		if (names.empty()) { for (int i = 0; i < 16; ++i) names.push_back("p" + std::to_string(i)); }
		std::vector<Dia::API::ValueParam> params(16);
		for (int i = 0; i < 16; ++i)
			params[i] = { names[i].c_str(), i % 4 == 3 ? "enum" : "float", i % 2 == 0, "a parameter with a reasonably long description text", "m", true, -1.0f, 1.0f, i % 4 == 3 ? "a|b|c" : nullptr };
		SimHelpInfo info;
		info.description = description.c_str();
		info.category = "cells";
		info.owner = "DiaCellLayers";
		info.params = params.data();
		info.paramCount = 16;
		SimHelpStored out;
		EXPECT_EQ(CopyHelp(info, out), Dia::DebugServer::SimHelpResult::Ok);
		return out;
	}
}

TEST_F(CodeSideSimHostedTest, ThirtyTwoSimNamesWithSixteenParamsEachAreAllCollectedWithEveryArg)
{
	for (int i = 0; i < 32; ++i)
		ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC(("csim.big" + std::to_string(i)).c_str()), SimKind::Command, SixteenParamHelp("help " + std::to_string(i)))) << i;
	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);
	EXPECT_EQ(BaselineRejected(), rep.mRejectedCount);
	for (int i = 0; i < 32; ++i)
	{
		const std::string n = "csim.big" + std::to_string(i);
		const ValueInfo* cmd = FindCmd(reg, n.c_str());
		ASSERT_NE(nullptr, cmd) << n;
		EXPECT_EQ(16u, cmd->mRelated.Size()) << n;
		EXPECT_NE(nullptr, FindCmd(reg, (n + ".p0").c_str())) << n;
		EXPECT_NE(nullptr, FindCmd(reg, (n + ".p15").c_str())) << n;
		EXPECT_FALSE(Listed(rep.mUndescribedParams, (n + ".p15").c_str())) << n;
	}
}

// A help with every field at its sim limit (enum list as 8 values of 15 chars, the longest list the collector's 63-byte
// value cap allows) is collected completely.
TEST_F(CodeSideSimHostedTest, MaximalValidHelpIsCollectedCompletely)
{
	using namespace Dia::DebugServer;
	const std::string desc(kSimHelpMaxDescription, 'd'), cat(kSimHelpMaxCategory, 'c'), owner(kSimHelpMaxOwner, 'o');
	const std::string pn(kSimHelpMaxParamName, 'n'), pd(kSimHelpMaxParamDescription, 'p');
	std::string pe;
	for (int i = 0; i < 8; ++i) { if (i) { pe += '|'; } pe += std::string(15, static_cast<char>('a' + i)); }
	ASSERT_LE(pe.size(), static_cast<size_t>(kSimHelpMaxParamEnum));
	std::vector<Dia::API::ValueParam> params(1);
	params[0] = { pn.c_str(), "enum", true, pd.c_str(), "u", true, -1.0f, 1.0f, pe.c_str() };
	SimHelpStored stored;
	ASSERT_EQ(CopyHelp(SimHelpInfo{ desc.c_str(), cat.c_str(), owner.c_str(), params.data(), 1, false }, stored), SimHelpResult::Ok);
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.max"), SimKind::Command, stored));

	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);
	EXPECT_EQ(BaselineRejected(), rep.mRejectedCount);
	const ValueInfo* cmd = FindCmd(reg, "csim.max");
	ASSERT_NE(nullptr, cmd);
	EXPECT_EQ(desc, std::string(cmd->mDescription.AsCStr()));
	ASSERT_EQ(1u, cmd->mRelated.Size());
	const ValueInfo* arg = FindCmd(reg, (std::string("csim.max.") + pn).c_str());
	ASSERT_NE(nullptr, arg);
	EXPECT_EQ(8u, arg->mEnumValues.Size());
}

// Sim limits are looser than the collector's: one enum value over 63 bytes passes the sim host but the collector rejects that
// arg loudly (counted + Error), exactly as it does for a DiaAPI command. The command itself is still collected.
TEST_F(CodeSideSimHostedTest, EnumValueOverTheCollectorCapIsRejectedLoudlyLikeADiaApiOne)
{
	const std::string longValue(70, 'v');
	Dia::API::ValueParam p = { "mode", "enum", true, "how", nullptr, false, 0.f, 0.f, longValue.c_str() };
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.longenum"), SimKind::Command, Help("d", &p, 1)));
	LogCapture log("VSC");
	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);
	EXPECT_EQ(BaselineRejected() + 1u, rep.mRejectedCount);
	EXPECT_GE(log.Errors("csim.longenum.mode"), 1u);
	EXPECT_NE(nullptr, FindCmd(reg, "csim.longenum"));
	EXPECT_EQ(nullptr, FindCmd(reg, "csim.longenum.mode"));
}

TEST_F(CodeSideSimHostedTest, CollectedRegistryOutlivesTheCatalogueEntries)
{
	for (int i = 0; i < 4; ++i)
		ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC(("csim.snap" + std::to_string(i)).c_str()), SimKind::Command, SixteenParamHelp("snapshot " + std::to_string(i)))) << i;
	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	CodeSideCollector().Collect(live, reg);
	Dia::DebugServer::SimHostedName none;
	ASSERT_TRUE(mServer->ReplaceSimHosted(&none, 0));	// clears the catalogue under the collected registry
	for (int i = 0; i < 4; ++i)
	{
		const std::string n = "csim.snap" + std::to_string(i);
		const ValueInfo* cmd = FindCmd(reg, n.c_str());
		ASSERT_NE(nullptr, cmd) << n;
		EXPECT_EQ("snapshot " + std::to_string(i), std::string(cmd->mDescription.AsCStr()));
		const ValueInfo* arg = FindCmd(reg, (n + ".p3").c_str());
		ASSERT_NE(nullptr, arg) << n;
		EXPECT_EQ(ValueType::Enum, arg->mType);
	}
}

TEST_F(CodeSideSimHostedTest, NameAlsoRegisteredAsAJsonCommandIsCollectedOnceWithAWarning)
{
	Dia::API::CommandInfoJson info;
	info.name = StringCRC("csim.dup");
	info.description = "from the json api";
	info.category = StringCRC("apicat");
	info.owner = "TestCodeSideSimHosted";
	info.noArgs = true;
	info.callback = [](const Json::Value&) { return Json::Value(Json::objectValue); };
	ASSERT_TRUE(Dia::API::RegisterCommandJson(info));
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.dup"), SimKind::Command, Help("from the sim", nullptr, 0, true)));

	LogCapture log("VSC");
	LiveCodeSideSource liveBase;	// no catalogue: duplicates other tests leave behind in the global metric registry
	ValueSchemaRegistry baseReg;
	const unsigned baseDuplicates = CodeSideCollector().Collect(liveBase, baseReg).mDuplicateCount;
	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);
	Dia::API::UnregisterCommandJson(StringCRC("csim.dup"));
	const ValueInfo* cmd = FindCmd(reg, "csim.dup");
	ASSERT_NE(nullptr, cmd);
	EXPECT_STREQ("from the json api", cmd->mDescription.AsCStr());
	EXPECT_EQ(baseDuplicates, rep.mDuplicateCount);
	EXPECT_EQ(1u, log.Warnings("csim.dup"));
}

TEST_F(CodeSideSimHostedTest, CommandGenerationFallsWhenSimNamesAreRemovedAndTracksReplace)
{
	LiveCodeSideSource live;
	live.SetSimHostedCatalogue(mServer.get());
	const unsigned base = live.CommandGeneration();
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.g1"), SimKind::Command, Help("g1", nullptr, 0, true)));
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("csim.g2"), SimKind::Command, Help("g2", nullptr, 0, true)));
	EXPECT_EQ(base + 2u, live.CommandGeneration());
	mServer->UnregisterSimHosted(StringCRC("csim.g1"));
	EXPECT_EQ(base + 1u, live.CommandGeneration());
	LiveCodeSideSource without;
	EXPECT_EQ(without.CommandGeneration(), base);
}
