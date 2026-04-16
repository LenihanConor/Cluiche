////////////////////////////////////////////////////////////////////////////////
// Filename: TestCommandRegistry.cpp
// Description: Unit tests for DiaAPI command registry
// Feature spec: docs/specs/features/dia/diacli/command-registry.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaCore/Core/Log.h>

using namespace Dia::API;

////////////////////////////////////////////////////////////////////////////////
// Test fixture
////////////////////////////////////////////////////////////////////////////////
class CommandRegistryTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Start fresh for each test
		if (IsInitialized())
		{
			Shutdown();
		}
	}

	void TearDown() override
	{
		// Clean up after each test
		if (IsInitialized())
		{
			Shutdown();
		}
	}

	// Helper: Create a simple test command callback
	static int TestCallback(const CommandArgs& args)
	{
		return 0;
	}

	// Helper: Create a command that returns non-zero
	static int FailCallback(const CommandArgs& args)
	{
		return 1;
	}
};

////////////////////////////////////////////////////////////////////////////////
// AC1: Initialize() starts command registry successfully
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, InitializeSuccess)
{
	EXPECT_FALSE(IsInitialized());
	Initialize();
	EXPECT_TRUE(IsInitialized());
}

////////////////////////////////////////////////////////////////////////////////
// AC1: Initialize() is idempotent (safe to call twice)
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, InitializeIdempotent)
{
	Initialize();
	EXPECT_TRUE(IsInitialized());

	// Should not crash or cause issues
	Initialize();
	EXPECT_TRUE(IsInitialized());
}

////////////////////////////////////////////////////////////////////////////////
// AC1: Shutdown() stops command registry successfully
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ShutdownSuccess)
{
	Initialize();
	EXPECT_TRUE(IsInitialized());

	Shutdown();
	EXPECT_FALSE(IsInitialized());
}

////////////////////////////////////////////////////////////////////////////////
// AC1: Shutdown() is safe when not initialized
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ShutdownWhenNotInitialized)
{
	EXPECT_FALSE(IsInitialized());

	// Should not crash
	Shutdown();
	EXPECT_FALSE(IsInitialized());
}

////////////////////////////////////////////////////////////////////////////////
// AC2: RegisterCommand() accepts valid command info
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RegisterValidCommand)
{
	Initialize();

	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("test-command");
	cmd.description = "A test command";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.version = "1.0.0";
	cmd.example = "test-command --flag";
	cmd.callback = TestCallback;

	EXPECT_TRUE(RegisterCommand(cmd));
}

////////////////////////////////////////////////////////////////////////////////
// AC2: RegisterCommand() rejects invalid command names
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RegisterInvalidCommandName)
{
	Initialize();

	// Test uppercase (should fail)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("TestCommand");
		cmd.description = "Test";
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestSystem";
		cmd.callback = TestCallback;
		EXPECT_FALSE(RegisterCommand(cmd));
	}

	// Test underscore (should fail)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("test_command");
		cmd.description = "Test";
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestSystem";
		cmd.callback = TestCallback;
		EXPECT_FALSE(RegisterCommand(cmd));
	}

	// Test space (should fail)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("test command");
		cmd.description = "Test";
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestSystem";
		cmd.callback = TestCallback;
		EXPECT_FALSE(RegisterCommand(cmd));
	}

	// Valid: lowercase + hyphens + digits (should succeed)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("test-command-123");
		cmd.description = "Test";
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestSystem";
		cmd.callback = TestCallback;
		EXPECT_TRUE(RegisterCommand(cmd));
	}
}

////////////////////////////////////////////////////////////////////////////////
// AC2: RegisterCommand() rejects null/empty required fields
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RegisterNullFields)
{
	Initialize();

	// Null description
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("test-cmd");
		cmd.description = nullptr;
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestSystem";
		cmd.callback = TestCallback;
		EXPECT_FALSE(RegisterCommand(cmd));
	}

	// Empty description
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("test-cmd");
		cmd.description = "";
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestSystem";
		cmd.callback = TestCallback;
		EXPECT_FALSE(RegisterCommand(cmd));
	}

	// Null owner
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("test-cmd");
		cmd.description = "Test";
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = nullptr;
		cmd.callback = TestCallback;
		EXPECT_FALSE(RegisterCommand(cmd));
	}

	// Null callback
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("test-cmd");
		cmd.description = "Test";
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestSystem";
		cmd.callback = nullptr;
		EXPECT_FALSE(RegisterCommand(cmd));
	}
}

////////////////////////////////////////////////////////////////////////////////
// AC3: RegisterCommand() detects duplicate command names
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RegisterDuplicateCommand)
{
	Initialize();

	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("test-command");
	cmd.description = "Test";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = TestCallback;

	// First registration should succeed
	EXPECT_TRUE(RegisterCommand(cmd));

	// Duplicate should fail
	EXPECT_FALSE(RegisterCommand(cmd));
}

////////////////////////////////////////////////////////////////////////////////
// AC4: RegisterCommand() supports pre-initialization registration
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RegisterBeforeInitialize)
{
	// Register before Initialize()
	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("early-command");
	cmd.description = "Registered before init";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = TestCallback;

	EXPECT_TRUE(RegisterCommand(cmd));

	// Initialize should process pending registration
	Initialize();

	// Command should be available
	const CommandInfo* found = GetCommand(Dia::Core::StringCRC("early-command"));
	EXPECT_NE(nullptr, found);
	EXPECT_STREQ("Registered before init", found->description);
}

////////////////////////////////////////////////////////////////////////////////
// AC4: Pre-initialization registrations also detect duplicates
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RegisterDuplicateBeforeInitialize)
{
	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("early-dup");
	cmd.description = "Test";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = TestCallback;

	// First registration should succeed
	EXPECT_TRUE(RegisterCommand(cmd));

	// Duplicate before Initialize should fail
	EXPECT_FALSE(RegisterCommand(cmd));
}

////////////////////////////////////////////////////////////////////////////////
// AC5: GetCommand() retrieves registered commands by name
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, GetCommandSuccess)
{
	Initialize();

	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("find-me");
	cmd.description = "Find this command";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.version = "1.0.0";
	cmd.example = "find-me --flag";
	cmd.callback = TestCallback;

	RegisterCommand(cmd);

	const CommandInfo* found = GetCommand(Dia::Core::StringCRC("find-me"));
	ASSERT_NE(nullptr, found);
	EXPECT_EQ(cmd.name, found->name);
	EXPECT_STREQ(cmd.description, found->description);
	EXPECT_EQ(cmd.category, found->category);
	EXPECT_STREQ(cmd.owner, found->owner);
	EXPECT_STREQ(cmd.version, found->version);
	EXPECT_STREQ(cmd.example, found->example);
}

////////////////////////////////////////////////////////////////////////////////
// AC5: GetCommand() returns nullptr for unknown commands
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, GetCommandNotFound)
{
	Initialize();

	const CommandInfo* found = GetCommand(Dia::Core::StringCRC("does-not-exist"));
	EXPECT_EQ(nullptr, found);
}

////////////////////////////////////////////////////////////////////////////////
// AC6: ListCommands() returns all registered commands
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ListAllCommands)
{
	Initialize();

	// Register multiple commands
	CommandInfo cmd1;
	cmd1.name = Dia::Core::StringCRC("cmd-one");
	cmd1.description = "First";
	cmd1.category = Dia::Core::StringCRC("test");
	cmd1.owner = "TestSystem";
	cmd1.callback = TestCallback;
	RegisterCommand(cmd1);

	CommandInfo cmd2;
	cmd2.name = Dia::Core::StringCRC("cmd-two");
	cmd2.description = "Second";
	cmd2.category = Dia::Core::StringCRC("test");
	cmd2.owner = "TestSystem";
	cmd2.callback = TestCallback;
	RegisterCommand(cmd2);

	CommandInfo cmd3;
	cmd3.name = Dia::Core::StringCRC("cmd-three");
	cmd3.description = "Third";
	cmd3.category = Dia::Core::StringCRC("other");
	cmd3.owner = "TestSystem";
	cmd3.callback = TestCallback;
	RegisterCommand(cmd3);

	auto commands = ListCommands();
	EXPECT_EQ(3u, commands.Size());
}

////////////////////////////////////////////////////////////////////////////////
// AC6: ListCommands() returns empty array when no commands registered
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ListCommandsEmpty)
{
	Initialize();

	auto commands = ListCommands();
	EXPECT_EQ(0u, commands.Size());
}

////////////////////////////////////////////////////////////////////////////////
// AC7: GetCommandsByCategory() filters by category
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, GetCommandsByCategory)
{
	Initialize();

	// Register commands in different categories
	CommandInfo buildCmd;
	buildCmd.name = Dia::Core::StringCRC("build-project");
	buildCmd.description = "Build project";
	buildCmd.category = Dia::Core::StringCRC("build");
	buildCmd.owner = "TestSystem";
	buildCmd.callback = TestCallback;
	RegisterCommand(buildCmd);

	CommandInfo assetCmd;
	assetCmd.name = Dia::Core::StringCRC("compile-asset");
	assetCmd.description = "Compile asset";
	assetCmd.category = Dia::Core::StringCRC("asset");
	assetCmd.owner = "TestSystem";
	assetCmd.callback = TestCallback;
	RegisterCommand(assetCmd);

	CommandInfo buildCmd2;
	buildCmd2.name = Dia::Core::StringCRC("clean-project");
	buildCmd2.description = "Clean project";
	buildCmd2.category = Dia::Core::StringCRC("build");
	buildCmd2.owner = "TestSystem";
	buildCmd2.callback = TestCallback;
	RegisterCommand(buildCmd2);

	// Query "build" category
	auto buildCommands = GetCommandsByCategory(Dia::Core::StringCRC("build"));
	EXPECT_EQ(2u, buildCommands.Size());

	// Query "asset" category
	auto assetCommands = GetCommandsByCategory(Dia::Core::StringCRC("asset"));
	EXPECT_EQ(1u, assetCommands.Size());
	EXPECT_EQ(Dia::Core::StringCRC("compile-asset"), assetCommands[0]->name);
}

////////////////////////////////////////////////////////////////////////////////
// AC7: GetCommandsByCategory() returns empty array for unknown category
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, GetCommandsByCategoryNotFound)
{
	Initialize();

	auto commands = GetCommandsByCategory(Dia::Core::StringCRC("nonexistent"));
	EXPECT_EQ(0u, commands.Size());
}

////////////////////////////////////////////////////////////////////////////////
// AC8: Command callbacks execute correctly
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, CommandCallbackExecution)
{
	Initialize();

	// Register command with success callback
	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("success-cmd");
	cmd.description = "Test success";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = TestCallback;
	RegisterCommand(cmd);

	// Register command with failure callback
	CommandInfo failCmd;
	failCmd.name = Dia::Core::StringCRC("fail-cmd");
	failCmd.description = "Test failure";
	failCmd.category = Dia::Core::StringCRC("test");
	failCmd.owner = "TestSystem";
	failCmd.callback = FailCallback;
	RegisterCommand(failCmd);

	// Execute callbacks
	const CommandInfo* successCmd = GetCommand(Dia::Core::StringCRC("success-cmd"));
	ASSERT_NE(nullptr, successCmd);
	CommandArgs args;  // Empty args
	EXPECT_EQ(0, successCmd->callback(args));

	const CommandInfo* failureCmd = GetCommand(Dia::Core::StringCRC("fail-cmd"));
	ASSERT_NE(nullptr, failureCmd);
	EXPECT_EQ(1, failureCmd->callback(args));
}
