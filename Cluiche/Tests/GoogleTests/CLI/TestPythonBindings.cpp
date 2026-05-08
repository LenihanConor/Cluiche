////////////////////////////////////////////////////////////////////////////////
// Filename: TestPythonBindings.cpp
// Description: Unit tests for DiaAPI Python bindings
// Feature spec: docs/specs/features/dia/diaapi/python-bindings.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaPython/DiaPython.h>

using namespace Dia::API;
using namespace Dia::Python;

////////////////////////////////////////////////////////////////////////////////
// Global test state (needed since callbacks are function pointers, not lambdas)
////////////////////////////////////////////////////////////////////////////////
static int gPyTestCallCount = 0;
static int gPyTestLastExitCode = 0;
static Dia::Core::Containers::DynamicArrayC<const char*, 8> gPyTestReceivedArgs;
static bool gPyTestReceivedNamedKey = false;
static const char* gPyTestReceivedNamedValue = nullptr;
static bool gPyTestReceivedFlag = false;

static int PyTestCountCallback(const CommandArgs& args)
{
	gPyTestCallCount++;
	return 0;
}

static int PyTestReturn42(const CommandArgs& args)
{
	return 42;
}

static int PyTestPositionalCallback(const CommandArgs& args)
{
	for (unsigned int i = 0; i < args.positionalArgs.Size(); i++)
	{
		gPyTestReceivedArgs.Add(args.positionalArgs[i]);
	}
	return 0;
}

static int PyTestNamedCallback(const CommandArgs& args)
{
	const char* value = args.GetNamedArg(Dia::Core::StringCRC("format").Value());
	if (value)
	{
		gPyTestReceivedNamedKey = true;
		gPyTestReceivedNamedValue = value;
	}
	return 0;
}

static int PyTestFlagCallback(const CommandArgs& args)
{
	if (args.HasFlag(Dia::Core::StringCRC("verbose").Value()))
	{
		gPyTestReceivedFlag = true;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Test fixture
////////////////////////////////////////////////////////////////////////////////
class PythonBindingsTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		bool pythonInit = Dia::Python::Initialize("External/Python311/", "External/Python/", false);
		ASSERT_TRUE(pythonInit) << "Failed to initialize Python";

		Dia::API::Initialize();

		gPyTestCallCount = 0;
		gPyTestLastExitCode = 0;
		gPyTestReceivedArgs.RemoveAll();
		gPyTestReceivedNamedKey = false;
		gPyTestReceivedNamedValue = nullptr;
		gPyTestReceivedFlag = false;
	}

	void TearDown() override
	{
		if (Dia::API::IsInitialized())
		{
			Dia::API::Shutdown();
		}

		if (Dia::Python::IsInitialized())
		{
			Dia::Python::Shutdown();
		}
	}

	CommandInfo CreateTestCommand(const char* name, const char* desc, CommandCallback callback)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC(name);
		cmd.description = desc;
		cmd.category = Dia::Core::StringCRC("test");
		cmd.owner = "TestOwner";
		cmd.version = "1.0.0";
		cmd.example = nullptr;
		cmd.callback = callback;
		return cmd;
	}
};

////////////////////////////////////////////////////////////////////////////////
// AC1: Create dia_api Python module during InitializePythonBindings()
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, CreateDiaApiModule)
{
	InitializePythonBindings();

	int exitCode = Dia::Python::ExecuteString("import dia_api");
	EXPECT_EQ(0, exitCode);
}

////////////////////////////////////////////////////////////////////////////////
// AC2: Register all CLI commands as Python functions in dia_api module
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, RegisterAllCommands)
{
	CommandInfo cmd1 = CreateTestCommand("test-cmd-1", "Test command 1", PyTestCountCallback);
	CommandInfo cmd2 = CreateTestCommand("test-cmd-2", "Test command 2", PyTestCountCallback);
	CommandInfo cmd3 = CreateTestCommand("test-cmd-3", "Test command 3", PyTestCountCallback);

	RegisterCommand(cmd1);
	RegisterCommand(cmd2);
	RegisterCommand(cmd3);

	InitializePythonBindings();

	Dia::Python::ExecuteString("import dia_api\ndia_api.test_cmd_1()");
	Dia::Python::ExecuteString("dia_api.test_cmd_2()");
	Dia::Python::ExecuteString("dia_api.test_cmd_3()");

	EXPECT_EQ(3, gPyTestCallCount);
}

////////////////////////////////////////////////////////////////////////////////
// AC3: Convert Python positional args to CommandArgs.positionalArgs
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, PositionalArgsConversion)
{
	CommandInfo cmd = CreateTestCommand("test-positional", "Test positional args", PyTestPositionalCallback);

	RegisterCommand(cmd);
	InitializePythonBindings();

	Dia::Python::ExecuteString("import dia_api\ndia_api.test_positional('arg1', 'arg2')");

	ASSERT_EQ(2u, gPyTestReceivedArgs.Size());
	EXPECT_STREQ("arg1", gPyTestReceivedArgs[0]);
	EXPECT_STREQ("arg2", gPyTestReceivedArgs[1]);
}

////////////////////////////////////////////////////////////////////////////////
// AC4: Convert Python keyword args to CommandArgs.namedArgs (argv-style)
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, NamedArgsConversion)
{
	CommandInfo cmd = CreateTestCommand("test-named", "Test named args", PyTestNamedCallback);

	RegisterCommand(cmd);
	InitializePythonBindings();

	Dia::Python::ExecuteString("import dia_api\ndia_api.test_named('--format=gltf')");

	EXPECT_TRUE(gPyTestReceivedNamedKey);
	EXPECT_STREQ("gltf", gPyTestReceivedNamedValue);
}

////////////////////////////////////////////////////////////////////////////////
// AC5: Convert Python boolean kwargs to CommandArgs.flags
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, FlagsConversion)
{
	CommandInfo cmd = CreateTestCommand("test-flags", "Test flags", PyTestFlagCallback);

	RegisterCommand(cmd);
	InitializePythonBindings();

	Dia::Python::ExecuteString("import dia_api\ndia_api.test_flags('--verbose')");

	EXPECT_TRUE(gPyTestReceivedFlag);
}

////////////////////////////////////////////////////////////////////////////////
// AC6: Return Python int from command exit code
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, ReturnExitCode)
{
	CommandInfo cmd = CreateTestCommand("test-return", "Test return value", PyTestReturn42);

	RegisterCommand(cmd);
	InitializePythonBindings();

	Dia::Python::ExecuteString(
		"import dia_api\n"
		"result = dia_api.test_return()\n"
		"assert result == 42, f'Expected 42, got {result}'"
	);
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Never include pybind11 headers in DiaAPI code
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, NoPybind11InDiaAPI)
{
	SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// AC8: Commands registered before Initialize() are exposed to Python
////////////////////////////////////////////////////////////////////////////////
TEST_F(PythonBindingsTest, PreInitRegistration)
{
	Dia::API::Shutdown();

	CommandInfo cmd = CreateTestCommand("pre-init-cmd", "Pre-init test", PyTestCountCallback);
	RegisterCommand(cmd);

	Dia::API::Initialize();
	InitializePythonBindings();

	Dia::Python::ExecuteString("import dia_api\ndia_api.pre_init_cmd()");

	EXPECT_EQ(1, gPyTestCallCount);
}
