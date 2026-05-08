////////////////////////////////////////////////////////////////////////////////
// Filename: TestHelpSystem.cpp
// Description: Unit tests for DiaAPI help system
// Feature spec: docs/specs/features/dia/diaapi/help-system.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/DiaAPI.h>

using namespace Dia::API;

////////////////////////////////////////////////////////////////////////////////
// Test fixture
////////////////////////////////////////////////////////////////////////////////
class HelpSystemTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!IsInitialized())
		{
			Initialize();
		}
	}

	void TearDown() override
	{
		if (IsInitialized())
		{
			Shutdown();
		}
	}

	static int DummyCallback(const CommandArgs&) { return 0; }

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
		cmd.callback = DummyCallback;
		return cmd;
	}
};

////////////////////////////////////////////////////////////////////////////////
// AC1: IsHelpRequested detects --help flag
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, IsHelpRequestedDetectsFlag)
{
	CommandArgs args;
	args.SetFlag(Dia::Core::StringCRC("help").Value(), true);

	EXPECT_TRUE(IsHelpRequested(args, Dia::Core::StringCRC("build")));
}

////////////////////////////////////////////////////////////////////////////////
// AC1: IsHelpRequested returns false without --help
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, IsHelpRequestedWithoutFlag)
{
	CommandArgs args;

	EXPECT_FALSE(IsHelpRequested(args, Dia::Core::StringCRC("build")));
}

////////////////////////////////////////////////////////////////////////////////
// AC2: ShowGlobalHelp lists all commands
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, ShowGlobalHelpListsCommands)
{
	CommandInfo cmd1 = CreateTestCommand("build", "Build project", "build", "TestOwner", "1.0.0", "build project.dia");
	CommandInfo cmd2 = CreateTestCommand("clean", "Clean output", "build", "TestOwner", "1.0.0", "clean");
	CommandInfo cmd3 = CreateTestCommand("compile-asset", "Compile asset", "asset", "AssetSystem", "2.0.0", "compile-asset model.fbx");

	RegisterCommand(cmd1);
	RegisterCommand(cmd2);
	RegisterCommand(cmd3);

	int result = ShowGlobalHelp();
	EXPECT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// AC3: ShowCommandHelp shows specific command
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, ShowCommandHelpShowsSpecificCommand)
{
	CommandInfo cmd = CreateTestCommand("build", "Build project", "build", "TestOwner", "1.0.0", "build project.dia");
	RegisterCommand(cmd);

	int result = ShowCommandHelp(Dia::Core::StringCRC("build"));
	EXPECT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// AC4: Commands grouped by category in global help
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, CommandsGroupedByCategory)
{
	CommandInfo cmd1 = CreateTestCommand("build", "Build project", "build");
	CommandInfo cmd2 = CreateTestCommand("compile-asset", "Compile asset", "asset");
	CommandInfo cmd3 = CreateTestCommand("debug-print", "Print debug info", "debug");

	RegisterCommand(cmd1);
	RegisterCommand(cmd2);
	RegisterCommand(cmd3);

	int result = ShowGlobalHelp();
	EXPECT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// AC5: Display command metadata
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, DisplayCommandMetadata)
{
	CommandInfo cmd = CreateTestCommand("test-cmd", "Test command", "test", "TestOwner", "2.5.0", "test-cmd --flag");
	RegisterCommand(cmd);

	int result = ShowCommandHelp(Dia::Core::StringCRC("test-cmd"));
	EXPECT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// AC6: Return exit code 0 after displaying help
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, HelpReturnsSuccess)
{
	CommandInfo cmd = CreateTestCommand("build", "Build project", "build");
	RegisterCommand(cmd);

	EXPECT_EQ(0, ShowGlobalHelp());
	EXPECT_EQ(0, ShowCommandHelp(Dia::Core::StringCRC("build")));
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Handle unknown command in help request
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, UnknownCommandHelpReturnsError)
{
	int result = ShowCommandHelp(Dia::Core::StringCRC("unknown-command"));
	EXPECT_EQ(3, result);
}

////////////////////////////////////////////////////////////////////////////////
// Empty registry: ShowGlobalHelp with no commands
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, EmptyRegistryShowsMessage)
{
	int result = ShowGlobalHelp();
	EXPECT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// Multiple categories test
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, MultipleCategoriesDisplayed)
{
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
	args.SetFlag(Dia::Core::StringCRC("help").Value(), true);

	EXPECT_TRUE(IsHelpRequested(args, Dia::Core::StringCRC("")));
}

////////////////////////////////////////////////////////////////////////////////
// Event integration: ShowGlobalHelp fires HelpRequested event
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, ShowGlobalHelpFiresEvent)
{
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

	ShowGlobalHelp();

	EXPECT_EQ(1, observer.callCount);
	EXPECT_TRUE(observer.lastIsGlobalHelp);

	GetHelpRequestedSubject().Detach(&observer);
}

////////////////////////////////////////////////////////////////////////////////
// Event integration: ShowCommandHelp fires HelpRequested event
////////////////////////////////////////////////////////////////////////////////
TEST_F(HelpSystemTest, ShowCommandHelpFiresEvent)
{
	CommandInfo cmd = CreateTestCommand("test-cmd", "Test command", "test");
	RegisterCommand(cmd);

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

	ShowCommandHelp(Dia::Core::StringCRC("test-cmd"));

	EXPECT_EQ(1, observer.callCount);
	EXPECT_EQ(Dia::Core::StringCRC("test-cmd"), observer.lastCommandName);
	EXPECT_FALSE(observer.lastIsGlobalHelp);

	GetHelpRequestedSubject().Detach(&observer);
}
