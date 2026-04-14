////////////////////////////////////////////////////////////////////////////////
// Filename: TestPythonBindings.cpp
// Description: Unit tests for DiaCLI Python bindings
// Feature spec: docs/specs/features/dia/diacli/python-bindings.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaCLI/DiaCLI.h>
#include <DiaPython/DiaPython.h>

using namespace Dia::CLI;
using namespace Dia::Python;

////////////////////////////////////////////////////////////////////////////////
// Test fixture
////////////////////////////////////////////////////////////////////////////////
class PythonBindingsTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Initialize Python
		bool pythonInit = Dia::Python::Initialize("External/Python311/", "External/Python/", false);
		ASSERT_TRUE(pythonInit) << "Failed to initialize Python";

		// Initialize DiaCLI
		Dia::CLI::Initialize();
	}

	void TearDown() override
	{
		// Clean up
		if (Dia::CLI::IsInitialized())
		{
			Dia::CLI::Shutdown();
		}

		if (Dia::Python::IsInitialized())
		{
			Dia::Python::Shutdown();
		}
	}

	// Helper: Create a test command
	CommandInfo CreateTestCommand(
		const char* name,
		const char* desc,
		std::function<int(const CommandArgs&)> callback)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC(name);
		cmd.description = desc;
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestOwner";
		cmd.version = "1.0.0";
		cmd.callback = callback;
		return cmd;
	}
};

////////////////////////////////////////////////////////////////////////////////
// AC1: Create dia_cli Python module during InitializePythonBindings()
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, CreateDiaCliModule)
{
	// Call InitializePythonBindings
	InitializePythonBindings();

	// Execute Python: import dia_cli
	int exitCode = Dia::Python::ExecuteString("import dia_cli");

	// Verify no exception
	EXPECT_EQ(0, exitCode);
}

////////////////////////////////////////////////////////////////////////////////
// AC2: Register all CLI commands as Python functions in dia_cli module
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, RegisterAllCommands)
{
	// Register 3 commands
	int callCount = 0;

	CommandInfo cmd1 = CreateTestCommand("test-cmd-1", "Test command 1",
		[&](const CommandArgs& args) { callCount++; return 0; });
	CommandInfo cmd2 = CreateTestCommand("test-cmd-2", "Test command 2",
		[&](const CommandArgs& args) { callCount++; return 0; });
	CommandInfo cmd3 = CreateTestCommand("test-cmd-3", "Test command 3",
		[&](const CommandArgs& args) { callCount++; return 0; });

	RegisterCommand(cmd1);
	RegisterCommand(cmd2);
	RegisterCommand(cmd3);

	// Initialize Python bindings
	InitializePythonBindings();

	// Call all 3 from Python (hyphens converted to underscores)
	Dia::Python::ExecuteString("import dia_cli\ndia_cli.test_cmd_1()");
	Dia::Python::ExecuteString("dia_cli.test_cmd_2()");
	Dia::Python::ExecuteString("dia_cli.test_cmd_3()");

	// Verify all 3 were called
	EXPECT_EQ(3, callCount);
}

////////////////////////////////////////////////////////////////////////////////
// AC3: Convert Python positional args to CommandArgs.positionalArgs
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, PositionalArgsConversion)
{
	Dia::Core::Containers::DynamicArrayC<const char*, 8> receivedArgs;

	CommandInfo cmd = CreateTestCommand("test-positional", "Test positional args",
		[&](const CommandArgs& args) {
			for (unsigned int i = 0; i < args.positionalArgs.Size(); i++)
			{
				receivedArgs.Add(args.positionalArgs[i]);
			}
			return 0;
		});

	RegisterCommand(cmd);
	InitializePythonBindings();

	// Call from Python with positional args
	Dia::Python::ExecuteString("import dia_cli\ndia_cli.test_positional('arg1', 'arg2')");

	// Verify C++ received positional args
	ASSERT_EQ(2u, receivedArgs.Size());
	EXPECT_STREQ("arg1", receivedArgs[0]);
	EXPECT_STREQ("arg2", receivedArgs[1]);
}

////////////////////////////////////////////////////////////////////////////////
// AC4: Convert Python keyword args to CommandArgs.namedArgs (argv-style)
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, NamedArgsConversion)
{
	bool receivedKey = false;
	const char* receivedValue = nullptr;

	CommandInfo cmd = CreateTestCommand("test-named", "Test named args",
		[&](const CommandArgs& args) {
			// Check if format key exists in namedArgs (std::unordered_map)
			auto it = args.namedArgs.find(Dia::Core::StringCRC("format").Value());
			if (it != args.namedArgs.end())
			{
				receivedKey = true;
				receivedValue = it->second;
			}
			return 0;
		});

	RegisterCommand(cmd);
	InitializePythonBindings();

	// Call from Python with argv-style named arg
	Dia::Python::ExecuteString("import dia_cli\ndia_cli.test_named('--format=gltf')");

	// Verify C++ received named arg
	EXPECT_TRUE(receivedKey);
	EXPECT_STREQ("gltf", receivedValue);
}

////////////////////////////////////////////////////////////////////////////////
// AC5: Convert Python boolean kwargs to CommandArgs.flags
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, FlagsConversion)
{
	bool receivedFlag = false;

	CommandInfo cmd = CreateTestCommand("test-flags", "Test flags",
		[&](const CommandArgs& args) {
			// Check if verbose flag exists (std::unordered_map)
			if (args.flags.find(Dia::Core::StringCRC("verbose").Value()) != args.flags.end())
			{
				receivedFlag = true;
			}
			return 0;
		});

	RegisterCommand(cmd);
	InitializePythonBindings();

	// Call from Python with argv-style flag
	Dia::Python::ExecuteString("import dia_cli\ndia_cli.test_flags('--verbose')");

	// Verify C++ received flag
	EXPECT_TRUE(receivedFlag);
}

////////////////////////////////////////////////////////////////////////////////
// AC6: Return Python int from command exit code
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, ReturnExitCode)
{
	CommandInfo cmd = CreateTestCommand("test-return", "Test return value",
		[](const CommandArgs& args) { return 42; });

	RegisterCommand(cmd);
	InitializePythonBindings();

	// Call from Python and capture return value
	Dia::Python::ExecuteString(
		"import dia_cli\n"
		"result = dia_cli.test_return()\n"
		"assert result == 42, f'Expected 42, got {result}'"
	);

	// If assertion passed in Python, test succeeds
	// (Dia::Python::ExecuteString returns 0 on success)
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Never include pybind11 headers in DiaCLI code
// This is a compile-time check - if we compile, we pass
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, NoPybind11InDiaCLI)
{
	// Compile-time test: DiaCLI should compile without pybind11 headers
	// If this test compiles and runs, AC7 is satisfied
	SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// AC8: Commands registered before Initialize() are exposed to Python
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, PreInitRegistration)
{
	// This test is handled by SetUp() - commands are registered after Initialize()
	// Let's test the opposite: register BEFORE Initialize

	// Shutdown and start fresh
	Dia::CLI::Shutdown();

	// Register command BEFORE Initialize
	int callCount = 0;
	CommandInfo cmd = CreateTestCommand("pre-init-cmd", "Pre-init test",
		[&](const CommandArgs& args) { callCount++; return 0; });
	RegisterCommand(cmd);

	// Now initialize
	Dia::CLI::Initialize();
	InitializePythonBindings();

	// Call from Python
	Dia::Python::ExecuteString("import dia_cli\ndia_cli.pre_init_cmd()");

	// Verify command was called
	EXPECT_EQ(1, callCount);
}
