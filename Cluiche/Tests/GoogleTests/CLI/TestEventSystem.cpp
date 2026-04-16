////////////////////////////////////////////////////////////////////////////////
// Filename: TestEventSystem.cpp
// Description: Unit tests for DiaAPI event system
// Feature spec: docs/specs/features/dia/diacli/event-system.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/DiaAPI.h>

using namespace Dia::API;

////////////////////////////////////////////////////////////////////////////////
// Global helper variables (accessible from observer classes and test fixture)
////////////////////////////////////////////////////////////////////////////////
static int gEventCount = 0;
static void* gLastEvent = nullptr;

////////////////////////////////////////////////////////////////////////////////
// Test fixture
////////////////////////////////////////////////////////////////////////////////
class EventSystemTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Reset event counters
		gEventCount = 0;
		gLastEvent = nullptr;
	}

	void TearDown() override
	{
	}
};

////////////////////////////////////////////////////////////////////////////////
// Test Observers
////////////////////////////////////////////////////////////////////////////////

// Observer for CommandRegisteredEvent
class CommandRegisteredObserver : public Observer<CommandRegisteredEvent>
{
public:
	void Notify(const CommandRegisteredEvent& event) override
	{
		gEventCount++;
		lastCommandName = event.commandName;
		lastDescription = event.description;
	}

	Dia::Core::StringCRC lastCommandName;
	const char* lastDescription = nullptr;
};

// Observer for CommandExecutingEvent
class CommandExecutingObserver : public Observer<CommandExecutingEvent>
{
public:
	void Notify(const CommandExecutingEvent& event) override
	{
		gEventCount++;
		lastCommandName = event.commandName;
		lastArgs = event.args;
	}

	Dia::Core::StringCRC lastCommandName;
	const CommandArgs* lastArgs = nullptr;
};

// Observer for CommandExecutedEvent
class CommandExecutedObserver : public Observer<CommandExecutedEvent>
{
public:
	void Notify(const CommandExecutedEvent& event) override
	{
		gEventCount++;
		lastCommandName = event.commandName;
		lastExitCode = event.exitCode;
		lastDuration = event.durationSeconds;
	}

	Dia::Core::StringCRC lastCommandName;
	int lastExitCode = 0;
	float lastDuration = 0.0f;
};

// Observer for CommandErrorEvent
class CommandErrorObserver : public Observer<CommandErrorEvent>
{
public:
	void Notify(const CommandErrorEvent& event) override
	{
		gEventCount++;
		lastCommandName = event.commandName;
		lastErrorMessage = event.errorMessage;
		lastExitCode = event.exitCode;
	}

	Dia::Core::StringCRC lastCommandName;
	const char* lastErrorMessage = nullptr;
	int lastExitCode = 0;
};

// Observer for HelpRequestedEvent
class HelpRequestedObserver : public Observer<HelpRequestedEvent>
{
public:
	void Notify(const HelpRequestedEvent& event) override
	{
		gEventCount++;
		lastCommandName = event.commandName;
		lastIsGlobalHelp = event.isGlobalHelp;
	}

	Dia::Core::StringCRC lastCommandName;
	bool lastIsGlobalHelp = false;
};

////////////////////////////////////////////////////////////////////////////////
// AC1: Fire OnCommandRegistered event when RegisterCommand() succeeds
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, CommandRegisteredEventFires)
{
	CommandRegisteredObserver observer;
	GetCommandRegisteredSubject().Attach(&observer);

	// Fire event
	Internal::FireCommandRegistered(Dia::Core::StringCRC("test-cmd"), "Test command description");

	// Verify observer was called
	EXPECT_EQ(1, gEventCount);
	EXPECT_EQ(Dia::Core::StringCRC("test-cmd"), observer.lastCommandName);
	EXPECT_STREQ("Test command description", observer.lastDescription);

	GetCommandRegisteredSubject().Detach(&observer);
}

////////////////////////////////////////////////////////////////////////////////
// AC2: Fire OnCommandExecuting event before command callback is invoked
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, CommandExecutingEventFires)
{
	CommandExecutingObserver observer;
	GetCommandExecutingSubject().Attach(&observer);

	// Create test args
	CommandArgs args;
	args.positionalArgs.Add("arg1");

	// Fire event
	Internal::FireCommandExecuting(Dia::Core::StringCRC("build"), &args);

	// Verify observer was called
	EXPECT_EQ(1, gEventCount);
	EXPECT_EQ(Dia::Core::StringCRC("build"), observer.lastCommandName);
	EXPECT_EQ(&args, observer.lastArgs);

	GetCommandExecutingSubject().Detach(&observer);
}

////////////////////////////////////////////////////////////////////////////////
// AC3: Fire OnCommandExecuted event after command callback completes
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, CommandExecutedEventFires)
{
	CommandExecutedObserver observer;
	GetCommandExecutedSubject().Attach(&observer);

	// Fire event
	Internal::FireCommandExecuted(Dia::Core::StringCRC("build"), 0, 1.25f);

	// Verify observer was called
	EXPECT_EQ(1, gEventCount);
	EXPECT_EQ(Dia::Core::StringCRC("build"), observer.lastCommandName);
	EXPECT_EQ(0, observer.lastExitCode);
	EXPECT_FLOAT_EQ(1.25f, observer.lastDuration);

	GetCommandExecutedSubject().Detach(&observer);
}

////////////////////////////////////////////////////////////////////////////////
// AC4: Fire OnCommandError event when command throws or returns error
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, CommandErrorEventFires)
{
	CommandErrorObserver observer;
	GetCommandErrorSubject().Attach(&observer);

	// Fire event
	Internal::FireCommandError(Dia::Core::StringCRC("build"), "File not found", 2);

	// Verify observer was called
	EXPECT_EQ(1, gEventCount);
	EXPECT_EQ(Dia::Core::StringCRC("build"), observer.lastCommandName);
	EXPECT_STREQ("File not found", observer.lastErrorMessage);
	EXPECT_EQ(2, observer.lastExitCode);

	GetCommandErrorSubject().Detach(&observer);
}

////////////////////////////////////////////////////////////////////////////////
// AC5: Fire OnHelpRequested event when help is displayed
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, HelpRequestedEventFires)
{
	HelpRequestedObserver observer;
	GetHelpRequestedSubject().Attach(&observer);

	// Fire event for global help
	Internal::FireHelpRequested(Dia::Core::StringCRC(""), true);

	// Verify observer was called
	EXPECT_EQ(1, gEventCount);
	EXPECT_TRUE(observer.lastIsGlobalHelp);

	// Fire event for command-specific help
	gEventCount = 0;
	Internal::FireHelpRequested(Dia::Core::StringCRC("build"), false);

	EXPECT_EQ(1, gEventCount);
	EXPECT_EQ(Dia::Core::StringCRC("build"), observer.lastCommandName);
	EXPECT_FALSE(observer.lastIsGlobalHelp);

	GetHelpRequestedSubject().Detach(&observer);
}

////////////////////////////////////////////////////////////////////////////////
// AC6: Multiple observers can subscribe to same event
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, MultipleObservers)
{
	CommandExecutedObserver observer1;
	CommandExecutedObserver observer2;
	CommandExecutedObserver observer3;

	GetCommandExecutedSubject().Attach(&observer1);
	GetCommandExecutedSubject().Attach(&observer2);
	GetCommandExecutedSubject().Attach(&observer3);

	// Fire event
	Internal::FireCommandExecuted(Dia::Core::StringCRC("test"), 0, 0.5f);

	// Verify all 3 observers were called
	EXPECT_EQ(3, gEventCount);

	GetCommandExecutedSubject().Detach(&observer1);
	GetCommandExecutedSubject().Detach(&observer2);
	GetCommandExecutedSubject().Detach(&observer3);
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Event payloads include relevant data
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, EventPayloadsContainCorrectData)
{
	// Test CommandRegisteredEvent
	{
		CommandRegisteredObserver observer;
		GetCommandRegisteredSubject().Attach(&observer);

		Internal::FireCommandRegistered(Dia::Core::StringCRC("my-command"), "My description");

		EXPECT_EQ(Dia::Core::StringCRC("my-command"), observer.lastCommandName);
		EXPECT_STREQ("My description", observer.lastDescription);

		GetCommandRegisteredSubject().Detach(&observer);
	}

	// Test CommandExecutedEvent with non-zero exit code
	{
		CommandExecutedObserver observer;
		GetCommandExecutedSubject().Attach(&observer);

		Internal::FireCommandExecuted(Dia::Core::StringCRC("failing-cmd"), 1, 2.5f);

		EXPECT_EQ(Dia::Core::StringCRC("failing-cmd"), observer.lastCommandName);
		EXPECT_EQ(1, observer.lastExitCode);
		EXPECT_FLOAT_EQ(2.5f, observer.lastDuration);

		GetCommandExecutedSubject().Detach(&observer);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Observer detach test
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, ObserverDetach)
{
	CommandExecutedObserver observer;
	GetCommandExecutedSubject().Attach(&observer);

	// Fire event - observer should be called
	Internal::FireCommandExecuted(Dia::Core::StringCRC("test"), 0, 0.1f);
	EXPECT_EQ(1, gEventCount);

	// Detach observer
	GetCommandExecutedSubject().Detach(&observer);

	// Fire event again - observer should NOT be called
	gEventCount = 0;
	Internal::FireCommandExecuted(Dia::Core::StringCRC("test"), 0, 0.1f);
	EXPECT_EQ(0, gEventCount);
}

////////////////////////////////////////////////////////////////////////////////
// Test firing events with no observers (should not crash)
////////////////////////////////////////////////////////////////////////////////
TEST_F(EventSystemTest, FireEventWithNoObservers)
{
	// Should not crash
	Internal::FireCommandRegistered(Dia::Core::StringCRC("test"), "desc");
	Internal::FireCommandExecuting(Dia::Core::StringCRC("test"), nullptr);
	Internal::FireCommandExecuted(Dia::Core::StringCRC("test"), 0, 0.1f);
	Internal::FireCommandError(Dia::Core::StringCRC("test"), "error", 1);
	Internal::FireHelpRequested(Dia::Core::StringCRC("test"), false);

	// No observers attached, so no events should have fired
	EXPECT_EQ(0, gEventCount);
}
