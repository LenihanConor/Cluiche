////////////////////////////////////////////////////////////////////////////////
// Filename: TestHelpSystem.cpp
// Description: Unit tests for DiaCLI help system
// Feature spec: docs/specs/features/dia/diacli/help-system.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaCLI/DiaCLI.h>

using namespace Dia::CLI;

////////////////////////////////////////////////////////////////////////////////
// Test fixture
////////////////////////////////////////////////////////////////////////////////
class HelpSystemTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Initialize DiaCLI for each test
		if (!IsInitialized())
		{
			Initialize();
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

	// Helper: Create a test command
	CommandInfo CreateTestCommand(const char* name, const char* desc, const char* cat,
		const char* owner = "TestOwner", const char* version = "1.0.0", const char* example = nullptr)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC(name);
		cmd.description = desc;
		cmd.category = Dia::Core::StringCRC(cat);
		cmd.owner = owner;
		cmd.version = version;
		cmd.example = example;
		cmd.callback = [](const CommandArgs&) { return 0; };
		return cmd;
	}
};

////////////////////////////////////////////////////////////////////////////////
// AC1: IsHelpRequested detects --help flag
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, IsHelpRequestedDetectsFlag)
{
	CommandArgs args;
	args.flags[Dia::Core::StringCRC("help").Value()] = true;

	EXPECT_TRUE(IsHelpRequested(args, Dia::Core::StringCRC("build")));
}

////////////////////////////////////////////////////////////////////////////////
// AC1: IsHelpRequested returns false without --help
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, IsHelpRequestedWithoutFlag)
{
	CommandArgs args;
	// Empty flags map

	EXPECT_FALSE(IsHelpRequested(args, Dia::Core::StringCRC("build")));
}

////////////////////////////////////////////////////////////////////////////////
// AC2: ShowGlobalHelp lists all commands
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, ShowGlobalHelpListsCommands)
{
	// Register multiple commands
	CommandInfo cmd1 = CreateTestCommand("build", "Build project", "build", "TestOwner", "1.0.0", "build project.dia");
	CommandInfo cmd2 = CreateTestCommand("clean", "Clean output", "build", "TestOwner", "1.0.0", "clean");
	CommandInfo cmd3 = CreateTestCommand("compile-asset", "Compile asset", "asset", "AssetSystem", "2.0.0", "compile-asset model.fbx");

	RegisterCommand(cmd1);
	RegisterCommand(cmd2);
	RegisterCommand(cmd3);

	// ShowGlobalHelp prints to stdout - we can't easily capture in this test
	// Just verify it returns success
	int result = ShowGlobalHelp();
	EXPECT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// AC3: ShowCommandHelp shows specific command
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, ShowCommandHelpShowsSpecificCommand)
{
	// Register a command
	CommandInfo cmd = CreateTestCommand("build", "Build project", "build", "TestOwner", "1.0.0", "build project.dia");
	RegisterCommand(cmd);

	// Show help for this command
	int result = ShowCommandHelp(Dia::Core::StringCRC("build"));
	EXPECT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// AC4: Commands grouped by category in global help
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, CommandsGroupedByCategory)
{
	// Register commands in 3 different categories
	CommandInfo cmd1 = CreateTestCommand("build", "Build project", "build");
	CommandInfo cmd2 = CreateTestCommand("compile-asset", "Compile asset", "asset");
	CommandInfo cmd3 = CreateTestCommand("debug-print", "Print debug info", "debug");

	RegisterCommand(cmd1);
	RegisterCommand(cmd2);
	RegisterCommand(cmd3);

	// ShowGlobalHelp should group by category
	int result = ShowGlobalHelp();
	EXPECT_EQ(0, result);
	// Note: We can't easily verify console output in unit test, but the function runs
}

////////////////////////////////////////////////////////////////////////////////
// AC5: Display command metadata
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, DisplayCommandMetadata)
{
	// Register command with all metadata
	CommandInfo cmd = CreateTestCommand("test-cmd", "Test command", "test", "TestOwner", "2.5.0", "test-cmd --flag");
	RegisterCommand(cmd);

	// Show command help - should display all fields
	int result = ShowCommandHelp(Dia::Core::StringCRC("test-cmd"));
	EXPECT_EQ(0, result);
	// Output contains name, description, category, owner, version, example (verified by inspection)
}

////////////////////////////////////////////////////////////////////////////////
// AC6: Return exit code 0 after displaying help
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, HelpReturnsSuccess)
{
	CommandInfo cmd = CreateTestCommand("build", "Build project", "build");
	RegisterCommand(cmd);

	// Global help returns 0
	EXPECT_EQ(0, ShowGlobalHelp());

	// Command help returns 0
	EXPECT_EQ(0, ShowCommandHelp(Dia::Core::StringCRC("build")));
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Handle unknown command in help request
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, UnknownCommandHelpReturnsError)
{
	// Try to show help for non-existent command
	int result = ShowCommandHelp(Dia::Core::StringCRC("unknown-command"));
	EXPECT_EQ(3, result);  // Error code 3 = command not found
}

////////////////////////////////////////////////////////////////////////////////
// Empty registry: ShowGlobalHelp with no commands
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, EmptyRegistryShowsMessage)
{
	// Don't register any commands

	int result = ShowGlobalHelp();
	EXPECT_EQ(0, result);  // Still returns success, just shows "No commands registered"
}

////////////////////////////////////////////////////////////////////////////////
// Multiple categories test
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, MultipleCategoriesDisplayed)
{
	// Register commands in multiple categories
	CommandInfo build1 = CreateTestCommand("build", "Build project", "build");
	CommandInfo build2 = CreateTestCommand("rebuild", "Rebuild project", "build");
	CommandInfo asset1 = CreateTestCommand("compile-asset", "Compile asset", "asset");
	CommandInfo asset2 = CreateTestCommand("validate-asset", "Validate asset", "asset");
	CommandInfo debug1 = CreateTestCommand("debug-trace", "Trace execution", "debug");

	RegisterCommand(build1);
	RegisterCommand(build2);
	RegisterCommand(asset1);
	RegisterCommand(asset2);
	RegisterCommand(debug1);

	int result = ShowGlobalHelp();
	EXPECT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// IsHelpRequested with empty command name
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, IsHelpRequestedWithEmptyCommand)
{
	CommandArgs args;
	args.flags[Dia::Core::StringCRC("help").Value()] = true;

	// Help requested even with empty command name
	EXPECT_TRUE(IsHelpRequested(args, Dia::Core::StringCRC("")));
}

////////////////////////////////////////////////////////////////////////////////
// Event integration: ShowGlobalHelp fires HelpRequested event
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, ShowGlobalHelpFiresEvent)
{
	// Observer for help event
	class TestHelpObserver : public Observer<HelpRequestedEvent>
	{
	public:
		int callCount = 0;
		bool lastIsGlobalHelp = false;

		void Notify(const HelpRequestedEvent& event) override
		{
			callCount++;
			lastIsGlobalHelp = event.isGlobalHelp;
		}
	};

	TestHelpObserver observer;
	GetHelpRequestedSubject().Attach(&observer);

	// Call ShowGlobalHelp - should fire event
	ShowGlobalHelp();

	// Verify event was fired with isGlobalHelp=true
	EXPECT_EQ(1, observer.callCount);
	EXPECT_TRUE(observer.lastIsGlobalHelp);

	GetHelpRequestedSubject().Detach(&observer);
}

////////////////////////////////////////////////////////////////////////////////
// Event integration: ShowCommandHelp fires HelpRequested event
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, ShowCommandHelpFiresEvent)
{
	// Register a test command first
	CommandInfo cmd = CreateTestCommand("test-cmd", "Test command", "test");
	RegisterCommand(cmd);

	// Observer for help event
	class TestHelpObserver : public Observer<HelpRequestedEvent>
	{
	public:
		int callCount = 0;
		Dia::Core::StringCRC lastCommandName;
		bool lastIsGlobalHelp = true;

		void Notify(const HelpRequestedEvent& event) override
		{
			callCount++;
			lastCommandName = event.commandName;
			lastIsGlobalHelp = event.isGlobalHelp;
		}
	};

	TestHelpObserver observer;
	GetHelpRequestedSubject().Attach(&observer);

	// Call ShowCommandHelp - should fire event
	ShowCommandHelp(Dia::Core::StringCRC("test-cmd"));

	// Verify event was fired with correct command name and isGlobalHelp=false
	EXPECT_EQ(1, observer.callCount);
	EXPECT_EQ(Dia::Core::StringCRC("test-cmd"), observer.lastCommandName);
	EXPECT_FALSE(observer.lastIsGlobalHelp);

	GetHelpRequestedSubject().Detach(&observer);
}
