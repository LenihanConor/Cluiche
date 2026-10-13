////////////////////////////////////////////////////////////////////////////////
// Filename: TestCommandRegistry.cpp
// Description: Unit tests for DiaAPI command registry
// Feature spec: docs/specs/features/dia/diacli/command-registry.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaCore/Core/Log.h>
#include <DiaCore/Core/Assert.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>
#include <vector>

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

	// Test underscore (should succeed)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC("test_command");
		cmd.description = "Test";
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestSystem";
		cmd.callback = TestCallback;
		EXPECT_TRUE(RegisterCommand(cmd));
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

////////////////////////////////////////////////////////////////////////////////
// ExecuteCommand: dispatches to correct command and returns exit code
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ExecuteCommandSuccess)
{
	Initialize();

	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("exec-test");
	cmd.description = "Execution test";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = TestCallback;
	RegisterCommand(cmd);

	CommandArgs args;
	int exitCode = ExecuteCommand(Dia::Core::StringCRC("exec-test"), args);
	EXPECT_EQ(0, exitCode);
}

////////////////////////////////////////////////////////////////////////////////
// ExecuteCommand: returns 3 for unknown command
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ExecuteCommandNotFound)
{
	Initialize();

	CommandArgs args;
	int exitCode = ExecuteCommand(Dia::Core::StringCRC("nonexistent"), args);
	EXPECT_EQ(3, exitCode);
}

////////////////////////////////////////////////////////////////////////////////
// ExecuteCommand: fires OnCommandExecuting and OnCommandExecuted events
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ExecuteCommandFiresEvents)
{
	Initialize();

	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("event-test");
	cmd.description = "Event test";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = TestCallback;
	RegisterCommand(cmd);

	// Attach observers
	class ExecutingObserver : public Observer<CommandExecutingEvent>
	{
	public:
		int count = 0;
		void Notify(const CommandExecutingEvent& event) override { count++; }
	};

	class ExecutedObserver : public Observer<CommandExecutedEvent>
	{
	public:
		int count = 0;
		int lastExitCode = -1;
		void Notify(const CommandExecutedEvent& event) override
		{
			count++;
			lastExitCode = event.exitCode;
		}
	};

	ExecutingObserver executingObs;
	ExecutedObserver executedObs;
	GetCommandExecutingSubject().Attach(&executingObs);
	GetCommandExecutedSubject().Attach(&executedObs);

	CommandArgs args;
	ExecuteCommand(Dia::Core::StringCRC("event-test"), args);

	EXPECT_EQ(1, executingObs.count);
	EXPECT_EQ(1, executedObs.count);
	EXPECT_EQ(0, executedObs.lastExitCode);

	GetCommandExecutingSubject().Detach(&executingObs);
	GetCommandExecutedSubject().Detach(&executedObs);
}

////////////////////////////////////////////////////////////////////////////////
// ExecuteCommand: fires OnCommandError for non-zero exit code
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ExecuteCommandFiresErrorEvent)
{
	Initialize();

	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("fail-event-test");
	cmd.description = "Fail event test";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = FailCallback;
	RegisterCommand(cmd);

	class ErrorObserver : public Observer<CommandErrorEvent>
	{
	public:
		int count = 0;
		int lastExitCode = 0;
		void Notify(const CommandErrorEvent& event) override
		{
			count++;
			lastExitCode = event.exitCode;
		}
	};

	ErrorObserver errorObs;
	GetCommandErrorSubject().Attach(&errorObs);

	CommandArgs args;
	int exitCode = ExecuteCommand(Dia::Core::StringCRC("fail-event-test"), args);

	EXPECT_EQ(1, exitCode);
	EXPECT_EQ(1, errorObs.count);
	EXPECT_EQ(1, errorObs.lastExitCode);

	GetCommandErrorSubject().Detach(&errorObs);
}

////////////////////////////////////////////////////////////////////////////////
// RunCLI: dispatches to correct command via argv parsing
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RunCLISuccess)
{
	Initialize();

	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC("run-test");
	cmd.description = "RunCLI test";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = TestCallback;
	RegisterCommand(cmd);

	char* argv[] = { const_cast<char*>("DiaAPI"), const_cast<char*>("run-test") };
	int exitCode = RunCLI(2, argv);
	EXPECT_EQ(0, exitCode);
}

////////////////////////////////////////////////////////////////////////////////
// RunCLI: returns error for missing command
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RunCLINoCommand)
{
	Initialize();

	char* argv[] = { const_cast<char*>("DiaAPI") };
	int exitCode = RunCLI(1, argv);
	EXPECT_EQ(2, exitCode);
}

////////////////////////////////////////////////////////////////////////////////
// RunCLI: returns 3 for unknown command
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RunCLIUnknownCommand)
{
	Initialize();

	char* argv[] = { const_cast<char*>("DiaAPI"), const_cast<char*>("nonexistent") };
	int exitCode = RunCLI(2, argv);
	EXPECT_EQ(3, exitCode);
}

////////////////////////////////////////////////////////////////////////////////
// command-unregister helpers
////////////////////////////////////////////////////////////////////////////////
static CommandInfo MakeUnregisterCmd(const char* name, CommandCallback cb)
{
	CommandInfo cmd;
	cmd.name = Dia::Core::StringCRC(name);
	cmd.description = "Unregister test";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = cb;
	return cmd;
}

static CommandInfoJson MakeUnregisterJsonCmd(const char* name, CommandCallbackJson cb)
{
	CommandInfoJson cmd;
	cmd.name = Dia::Core::StringCRC(name);
	cmd.description = "Unregister JSON test";
	cmd.category = Dia::Core::StringCRC("test");
	cmd.owner = "TestSystem";
	cmd.callback = cb;
	return cmd;
}

////////////////////////////////////////////////////////////////////////////////
// AC-1: UnregisterCommand removes a registered command
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, UnregisterCommandRemovesCommand)
{
	Initialize();
	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("unreg-one", TestCallback)));

	EXPECT_TRUE(UnregisterCommand(Dia::Core::StringCRC("unreg-one")));
	EXPECT_EQ(nullptr, GetCommand(Dia::Core::StringCRC("unreg-one")));

	CommandArgs args;
	EXPECT_EQ(3, ExecuteCommand(Dia::Core::StringCRC("unreg-one"), args));
}

////////////////////////////////////////////////////////////////////////////////
// AC-2: UnregisterCommand returns false for unknown name
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, UnregisterCommandUnknownReturnsFalse)
{
	Initialize();
	EXPECT_FALSE(UnregisterCommand(Dia::Core::StringCRC("never-registered")));
}

////////////////////////////////////////////////////////////////////////////////
// AC-3: A name can be re-registered after unregistering; new callback runs
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, UnregisterCommandAllowsReRegister)
{
	Initialize();
	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("unreg-again", TestCallback)));
	ASSERT_TRUE(UnregisterCommand(Dia::Core::StringCRC("unreg-again")));

	EXPECT_TRUE(RegisterCommand(MakeUnregisterCmd("unreg-again", FailCallback)));

	CommandArgs args;
	EXPECT_EQ(1, ExecuteCommand(Dia::Core::StringCRC("unreg-again"), args));
}

////////////////////////////////////////////////////////////////////////////////
// AC-4: A pending (pre-Initialize) command can be unregistered
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, UnregisterPendingCommand)
{
	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("unreg-pending", TestCallback)));

	EXPECT_TRUE(UnregisterCommand(Dia::Core::StringCRC("unreg-pending")));

	Initialize();
	EXPECT_EQ(nullptr, GetCommand(Dia::Core::StringCRC("unreg-pending")));
}

////////////////////////////////////////////////////////////////////////////////
// AC-5: UnregisterCommandJson mirrors AC-1
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, UnregisterCommandJsonRemovesCommand)
{
	Initialize();
	ASSERT_TRUE(RegisterCommandJson(MakeUnregisterJsonCmd("unreg-json", [](const Json::Value&) { return Json::Value(1); })));

	EXPECT_TRUE(UnregisterCommandJson(Dia::Core::StringCRC("unreg-json")));

	Json::Value response = ExecuteCommandJson(Dia::Core::StringCRC("unreg-json"), Json::Value());
	EXPECT_FALSE(response["success"].asBool());
	EXPECT_EQ("command not found", response["error"].asString());
}

////////////////////////////////////////////////////////////////////////////////
// AC-5: UnregisterCommandJson mirrors AC-2
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, UnregisterCommandJsonUnknownReturnsFalse)
{
	Initialize();
	EXPECT_FALSE(UnregisterCommandJson(Dia::Core::StringCRC("never-registered-json")));
}

////////////////////////////////////////////////////////////////////////////////
// AC-5: UnregisterCommandJson mirrors AC-3
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, UnregisterCommandJsonAllowsReRegister)
{
	Initialize();
	ASSERT_TRUE(RegisterCommandJson(MakeUnregisterJsonCmd("unreg-json-again", [](const Json::Value&) { return Json::Value(1); })));
	ASSERT_TRUE(UnregisterCommandJson(Dia::Core::StringCRC("unreg-json-again")));

	EXPECT_TRUE(RegisterCommandJson(MakeUnregisterJsonCmd("unreg-json-again", [](const Json::Value&) { return Json::Value(2); })));

	Json::Value response = ExecuteCommandJson(Dia::Core::StringCRC("unreg-json-again"), Json::Value());
	EXPECT_TRUE(response["success"].asBool());
	EXPECT_EQ(2, response["data"].asInt());
}

////////////////////////////////////////////////////////////////////////////////
// command-unregister task 3: thread safety (AC-7 to AC-9)
////////////////////////////////////////////////////////////////////////////////
namespace
{
	std::atomic<int> gRegistryAssertCount{0};

	void RecordingRegistryAssertHandler(const char*, const char*, int, const char*, ...)
	{
		gRegistryAssertCount.fetch_add(1);
	}

	// AC-8 state is file-static (not lambda captures) so the callback never reads
	// state owned by the CommandInfo that Unregister deletes.
	std::promise<void>* gBlockingEntered = nullptr;
	std::shared_future<void> gBlockingRelease;

	int BlockingCallback(const CommandArgs&)
	{
		gBlockingEntered->set_value();
		gBlockingRelease.wait_for(std::chrono::seconds(5));
		return 0;
	}
}

////////////////////////////////////////////////////////////////////////////////
// AC-7: concurrent register/unregister with concurrent execute/list
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, ConcurrentRegisterUnregisterExecuteList)
{
	Initialize();
	std::atomic<int> stableCalls{0};
	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("stress-stable",
		[&stableCalls](const CommandArgs&) { stableCalls.fetch_add(1); return 0; })));
	ASSERT_TRUE(RegisterCommandJson(MakeUnregisterJsonCmd("stress-stable-json",
		[](const Json::Value&) { return Json::Value(7); })));

	const int kWriters = 4;
	const int kReaders = 4;
	const int kIterations = 1000;

	std::atomic<int> writersDone{0};
	std::atomic<int> failures{0};
	std::atomic<int> readerExecutes{0};

	std::vector<std::thread> threads;
	for (int t = 0; t < kWriters; ++t)
	{
		threads.emplace_back([t, &writersDone, &failures]()
		{
			char name[64];
			for (int i = 0; i < kIterations; ++i)
			{
				snprintf(name, sizeof(name), "stress-w%d-%d", t, i);
				Dia::Core::StringCRC crc(name);
				if (!RegisterCommand(MakeUnregisterCmd(name, [](const CommandArgs&) { return 0; })))
					failures.fetch_add(1);
				if (!RegisterCommandJson(MakeUnregisterJsonCmd(name, [](const Json::Value&) { return Json::Value(1); })))
					failures.fetch_add(1);
				if (!UnregisterCommand(crc))
					failures.fetch_add(1);
				if (!UnregisterCommandJson(crc))
					failures.fetch_add(1);
			}
			writersDone.fetch_add(1);
		});
	}
	for (int r = 0; r < kReaders; ++r)
	{
		threads.emplace_back([&]()
		{
			CommandArgs args;
			while (writersDone.load() < kWriters)
			{
				if (ExecuteCommand(Dia::Core::StringCRC("stress-stable"), args) != 0)
					failures.fetch_add(1);
				Json::Value response = ExecuteCommandJson(Dia::Core::StringCRC("stress-stable-json"), Json::Value());
				if (!response["success"].asBool() || response["data"].asInt() != 7)
					failures.fetch_add(1);
				auto list = ListCommands();
				if (list.Size() < 1 || list.Size() > static_cast<unsigned int>(1 + kWriters))
					failures.fetch_add(1);
				readerExecutes.fetch_add(1);
			}
		});
	}
	for (auto& th : threads)
		th.join();

	EXPECT_EQ(0, failures.load());
	EXPECT_EQ(readerExecutes.load(), stableCalls.load());

	auto finalList = ListCommands();
	ASSERT_EQ(1u, finalList.Size());
	EXPECT_EQ(Dia::Core::StringCRC("stress-stable"), finalList[0]->name);

	char name[64];
	for (int t = 0; t < kWriters; ++t)
	{
		for (int i = 0; i < kIterations; ++i)
		{
			snprintf(name, sizeof(name), "stress-w%d-%d", t, i);
			EXPECT_EQ(nullptr, GetCommand(Dia::Core::StringCRC(name)));
			EXPECT_FALSE(UnregisterCommandJson(Dia::Core::StringCRC(name)));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// AC-8: UnregisterCommand waits for an in-flight callback to return
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, UnregisterWaitsForInFlightCallback)
{
	Initialize();
	std::promise<void> entered;
	std::promise<void> release;
	gBlockingEntered = &entered;
	gBlockingRelease = release.get_future().share();

	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("blocking-cmd", BlockingCallback)));

	std::future<int> exec = std::async(std::launch::async, []()
	{
		CommandArgs args;
		return ExecuteCommand(Dia::Core::StringCRC("blocking-cmd"), args);
	});
	ASSERT_EQ(std::future_status::ready, entered.get_future().wait_for(std::chrono::seconds(5)));

	std::future<bool> unreg = std::async(std::launch::async, []()
	{
		return UnregisterCommand(Dia::Core::StringCRC("blocking-cmd"));
	});

	// While the callback is blocked, Unregister must not return.
	EXPECT_EQ(std::future_status::timeout, unreg.wait_for(std::chrono::milliseconds(150)));

	release.set_value();
	ASSERT_EQ(std::future_status::ready, exec.wait_for(std::chrono::seconds(5)));
	EXPECT_EQ(0, exec.get());
	ASSERT_EQ(std::future_status::ready, unreg.wait_for(std::chrono::seconds(5)));
	EXPECT_TRUE(unreg.get());
	EXPECT_EQ(nullptr, GetCommand(Dia::Core::StringCRC("blocking-cmd")));

	gBlockingEntered = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// AC-9: a callback that registers/unregisters is rejected (assert in Debug,
// false in all builds) without touching the lock, so it cannot deadlock.
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, RegisterFromInsideCallbackIsRejected)
{
	Initialize();
	auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
	Dia::Core::g_pAssertFunc = RecordingRegistryAssertHandler;
	gRegistryAssertCount.store(0);

	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("reentrant-victim", TestCallback)));
	ASSERT_TRUE(RegisterCommandJson(MakeUnregisterJsonCmd("reentrant-victim-json",
		[](const Json::Value&) { return Json::Value(1); })));

	bool results[4] = { true, true, true, true };
	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("reentrant-cli", [&results](const CommandArgs&)
	{
		results[0] = RegisterCommand(MakeUnregisterCmd("reentrant-new", TestCallback));
		results[1] = UnregisterCommand(Dia::Core::StringCRC("reentrant-victim"));
		return 0;
	})));
	ASSERT_TRUE(RegisterCommandJson(MakeUnregisterJsonCmd("reentrant-json", [&results](const Json::Value&)
	{
		results[2] = RegisterCommandJson(MakeUnregisterJsonCmd("reentrant-new-json",
			[](const Json::Value&) { return Json::Value(1); }));
		results[3] = UnregisterCommandJson(Dia::Core::StringCRC("reentrant-victim-json"));
		return Json::Value(0);
	})));

	CommandArgs args;
	EXPECT_EQ(0, ExecuteCommand(Dia::Core::StringCRC("reentrant-cli"), args));
	EXPECT_TRUE(ExecuteCommandJson(Dia::Core::StringCRC("reentrant-json"), Json::Value())["success"].asBool());

	Dia::Core::g_pAssertFunc = prevAssertFunc;

	for (bool r : results)
		EXPECT_FALSE(r);
#ifdef DEBUG
	EXPECT_EQ(4, gRegistryAssertCount.load());
#endif
	// Registry unchanged by the rejected calls
	EXPECT_EQ(nullptr, GetCommand(Dia::Core::StringCRC("reentrant-new")));
	EXPECT_NE(nullptr, GetCommand(Dia::Core::StringCRC("reentrant-victim")));
	EXPECT_TRUE(ExecuteCommandJson(Dia::Core::StringCRC("reentrant-victim-json"), Json::Value())["success"].asBool());
	EXPECT_FALSE(ExecuteCommandJson(Dia::Core::StringCRC("reentrant-new-json"), Json::Value())["success"].asBool());

	// Outside a callback, registration works again
	EXPECT_TRUE(RegisterCommand(MakeUnregisterCmd("reentrant-new", TestCallback)));
}

////////////////////////////////////////////////////////////////////////////////
// Nested ExecuteCommand from inside a callback works, including while a writer
// on another thread is queued for the exclusive lock (no recursive shared lock).
////////////////////////////////////////////////////////////////////////////////
TEST_F(CommandRegistryTest, NestedExecuteFromCallback)
{
	Initialize();
	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("nested-inner", [](const CommandArgs&) { return 5; })));
	ASSERT_TRUE(RegisterCommandJson(MakeUnregisterJsonCmd("nested-inner-json",
		[](const Json::Value&) { return Json::Value(9); })));

	auto entered = std::make_shared<std::promise<void>>();
	auto proceed = std::make_shared<std::promise<void>>();
	std::shared_future<void> proceedFuture = proceed->get_future().share();

	ASSERT_TRUE(RegisterCommand(MakeUnregisterCmd("nested-outer", [entered, proceedFuture](const CommandArgs&)
	{
		entered->set_value();
		proceedFuture.wait_for(std::chrono::seconds(5));
		CommandArgs inner;
		int code = ExecuteCommand(Dia::Core::StringCRC("nested-inner"), inner);
		Json::Value json = ExecuteCommandJson(Dia::Core::StringCRC("nested-inner-json"), Json::Value());
		return code * 10 + json["data"].asInt();
	})));

	std::future<int> exec = std::async(std::launch::async, []()
	{
		CommandArgs args;
		return ExecuteCommand(Dia::Core::StringCRC("nested-outer"), args);
	});
	ASSERT_EQ(std::future_status::ready, entered->get_future().wait_for(std::chrono::seconds(5)));

	// Queue a writer behind the in-flight callback, then let the callback nest.
	std::future<bool> writer = std::async(std::launch::async, []()
	{
		return RegisterCommand(MakeUnregisterCmd("nested-writer", TestCallback));
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	proceed->set_value();

	ASSERT_EQ(std::future_status::ready, exec.wait_for(std::chrono::seconds(5))) << "nested execute deadlocked";
	EXPECT_EQ(59, exec.get());
	ASSERT_EQ(std::future_status::ready, writer.wait_for(std::chrono::seconds(5)));
	EXPECT_TRUE(writer.get());
}
