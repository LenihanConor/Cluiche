#include <gtest/gtest.h>
#include <DiaValueSchema/CodeSideCollector.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimHostedCatalogue.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <memory>
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
	live.SetSimHostedCatalogue(mServer.get());
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);

	const ValueInfo* cmd = FindCmd(reg, "csim.dup");
	ASSERT_NE(nullptr, cmd);
	EXPECT_STREQ("from the api", cmd->mDescription.AsCStr());
	EXPECT_EQ(0u, rep.mDuplicateCount);	// never reached the registry: skipped before it
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
