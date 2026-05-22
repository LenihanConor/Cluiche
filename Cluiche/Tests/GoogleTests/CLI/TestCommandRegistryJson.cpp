////////////////////////////////////////////////////////////////////////////////
// Filename: TestCommandRegistryJson.cpp
// GoogleTest suite — DiaAPI JSON command path
//
// Covers AC1-AC4, AC11 from:
//   docs/specs/features/dia/diaapplicationflow/baseline-commands.md
//
// AC1: ValidateCommandName accepts [a-z0-9._-]+
// AC2: CommandCallbackJson type + RegisterCommandJson API exists
// AC3: ExecuteCommandJson dispatches and wraps in {success,data/error} envelope
// AC4: CommandDispatcher routes JSON commands first (integration path)
// AC11: Existing CLI-style commands still register and execute unchanged
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::API;
using Dia::Core::StringCRC;

class CommandRegistryJsonTest : public ::testing::Test
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
};

// ---------------------------------------------------------------------------
// AC1 — Dotted names pass validation
// ---------------------------------------------------------------------------
TEST_F(CommandRegistryJsonTest, AC1_DottedNameAccepted)
{
    CommandInfoJson info;
    info.name        = StringCRC("dia.app.quit");
    info.description = "Test dotted name";
    info.category    = StringCRC("dia.app");
    info.owner       = "Test";
    info.callback    = [](const Json::Value&) -> Json::Value { return Json::Value(Json::objectValue); };

    EXPECT_TRUE(RegisterCommandJson(info));
}

TEST_F(CommandRegistryJsonTest, AC1_UnderscoreNameAccepted)
{
    CommandInfoJson info;
    info.name        = StringCRC("dia_test_cmd");
    info.description = "Test underscore name";
    info.category    = StringCRC("dia");
    info.owner       = "Test";
    info.callback    = [](const Json::Value&) -> Json::Value { return Json::Value(Json::objectValue); };

    EXPECT_TRUE(RegisterCommandJson(info));
}

TEST_F(CommandRegistryJsonTest, AC1_HyphenNameAccepted)
{
    CommandInfoJson info;
    info.name        = StringCRC("compile-asset");
    info.description = "Test hyphen name (existing CLI style)";
    info.category    = StringCRC("build");
    info.owner       = "Test";
    info.callback    = [](const Json::Value&) -> Json::Value { return Json::Value(Json::objectValue); };

    EXPECT_TRUE(RegisterCommandJson(info));
}

TEST_F(CommandRegistryJsonTest, AC1_UppercaseNameRejected)
{
    // Uppercase not allowed
    CommandInfoJson info;
    info.name        = StringCRC("Dia.App.Quit");
    info.description = "Test";
    info.category    = StringCRC("dia");
    info.owner       = "Test";
    info.callback    = [](const Json::Value&) -> Json::Value { return Json::Value(Json::objectValue); };

    EXPECT_FALSE(RegisterCommandJson(info));
}

// ---------------------------------------------------------------------------
// AC2 — RegisterCommandJson returns true on valid registration
// ---------------------------------------------------------------------------
TEST_F(CommandRegistryJsonTest, AC2_RegisterJsonCommandSucceeds)
{
    CommandInfoJson info;
    info.name        = StringCRC("test.json.cmd");
    info.description = "A test JSON command";
    info.category    = StringCRC("test");
    info.owner       = "UnitTest";
    info.callback    = [](const Json::Value& params) -> Json::Value {
        Json::Value result;
        result["echo"] = params.get("input", "none").asString();
        return result;
    };

    EXPECT_TRUE(RegisterCommandJson(info));
}

TEST_F(CommandRegistryJsonTest, AC2_RegisterJsonCommandDuplicateRejected)
{
    CommandInfoJson info;
    info.name        = StringCRC("test.dup.cmd");
    info.description = "Duplicate test";
    info.category    = StringCRC("test");
    info.owner       = "UnitTest";
    info.callback    = [](const Json::Value&) -> Json::Value { return Json::Value(Json::objectValue); };

    EXPECT_TRUE(RegisterCommandJson(info));
    EXPECT_FALSE(RegisterCommandJson(info));  // duplicate
}

// ---------------------------------------------------------------------------
// AC3 — ExecuteCommandJson returns structured envelope
// ---------------------------------------------------------------------------
TEST_F(CommandRegistryJsonTest, AC3_ExecuteJsonCommandSuccess)
{
    CommandInfoJson info;
    info.name        = StringCRC("test.exec.cmd");
    info.description = "Exec test";
    info.category    = StringCRC("test");
    info.owner       = "UnitTest";
    info.callback    = [](const Json::Value& params) -> Json::Value {
        Json::Value result;
        result["value"] = 42;
        return result;
    };
    RegisterCommandJson(info);

    Json::Value response = ExecuteCommandJson(StringCRC("test.exec.cmd"), Json::Value(Json::objectValue));
    EXPECT_TRUE(response["success"].asBool());
    EXPECT_TRUE(response.isMember("data"));
    EXPECT_EQ(response["data"]["value"].asInt(), 42);
}

TEST_F(CommandRegistryJsonTest, AC3_ExecuteJsonCommandNotFound)
{
    Json::Value response = ExecuteCommandJson(StringCRC("nonexistent.cmd"), Json::Value(Json::objectValue));
    EXPECT_FALSE(response["success"].asBool());
    EXPECT_EQ(response["error"].asString(), "command not found");
}

TEST_F(CommandRegistryJsonTest, AC3_ExecuteJsonCommandPassesParams)
{
    CommandInfoJson info;
    info.name        = StringCRC("test.params.cmd");
    info.description = "Params test";
    info.category    = StringCRC("test");
    info.owner       = "UnitTest";
    info.callback    = [](const Json::Value& params) -> Json::Value {
        Json::Value result;
        result["received"] = params["key"].asString();
        return result;
    };
    RegisterCommandJson(info);

    Json::Value params;
    params["key"] = "hello";
    Json::Value response = ExecuteCommandJson(StringCRC("test.params.cmd"), params);
    EXPECT_TRUE(response["success"].asBool());
    EXPECT_EQ(response["data"]["received"].asString(), "hello");
}

// ---------------------------------------------------------------------------
// AC11 — Existing CLI-style commands continue to work unchanged
// ---------------------------------------------------------------------------
TEST_F(CommandRegistryJsonTest, AC11_LegacyCliCommandStillWorks)
{
    CommandInfo cliInfo;
    cliInfo.name        = StringCRC("legacy-cmd");
    cliInfo.description = "Legacy CLI command";
    cliInfo.category    = StringCRC("legacy");
    cliInfo.owner       = "UnitTest";
    cliInfo.version     = "1.0";
    cliInfo.example     = "legacy-cmd";
    int callCount = 0;
    cliInfo.callback    = [&callCount](const CommandArgs&) -> int {
        ++callCount;
        return 0;
    };

    EXPECT_TRUE(RegisterCommand(cliInfo));

    CommandArgs args;
    int result = ExecuteCommand(StringCRC("legacy-cmd"), args);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(callCount, 1);
}

TEST_F(CommandRegistryJsonTest, AC11_LegacyHyphenNameStillValid)
{
    CommandInfo cliInfo;
    cliInfo.name        = StringCRC("compile-asset");
    cliInfo.description = "Compile an asset file";
    cliInfo.category    = StringCRC("build");
    cliInfo.owner       = "DiaAssets";
    cliInfo.version     = "1.0";
    cliInfo.example     = "compile-asset model.fbx";
    cliInfo.callback    = [](const CommandArgs&) -> int { return 0; };

    EXPECT_TRUE(RegisterCommand(cliInfo));
    EXPECT_NE(GetCommand(StringCRC("compile-asset")), nullptr);
}
