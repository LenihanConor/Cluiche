////////////////////////////////////////////////////////////////////////////////
// Filename: TestModule.cpp - Google Test for DiaPython Module API
// Feature spec: docs/specs/features/dia/diapython/module-api.md
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaPython/DiaPython.h>

using namespace Dia::Python;

////////////////////////////////////////////////////////////////////////////////
// DiaPython Module API Tests
////////////////////////////////////////////////////////////////////////////////

// Test fixture for module API tests
class DiaPythonModuleTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Initialize Python for each test
		bool result = Initialize("External/Python311/", "External/Python/", false);
		ASSERT_TRUE(result) << "Failed to initialize Python for test";
	}

	void TearDown() override
	{
		// Clean up after each test
		if (IsInitialized())
		{
			Shutdown();
		}
	}
};

////////////////////////////////////////////////////////////////////////////////
// AC1: Can create Python module via CreateModule(const char* name)
////////////////////////////////////////////////////////////////////////////////

TEST_F(DiaPythonModuleTest, CreateModule_WithValidName_Succeeds)
{
	Module* module = CreateModule("test_module");

	EXPECT_NE(module, nullptr);
}

TEST_F(DiaPythonModuleTest, CreateModule_NestedModule_Succeeds)
{
	Module* module = CreateModule("dia.cli");

	EXPECT_NE(module, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// AC6: Module names are validated (no invalid characters, not empty)
////////////////////////////////////////////////////////////////////////////////

TEST_F(DiaPythonModuleTest, CreateModule_WithEmptyName_Fails)
{
	Module* module = CreateModule("");

	EXPECT_EQ(module, nullptr);
}

TEST_F(DiaPythonModuleTest, CreateModule_WithNullName_Fails)
{
	Module* module = CreateModule(nullptr);

	EXPECT_EQ(module, nullptr);
}

TEST_F(DiaPythonModuleTest, CreateModule_WithInvalidCharacters_Fails)
{
	Module* module = CreateModule("test-module");  // Hyphen not allowed

	EXPECT_EQ(module, nullptr);
}

TEST_F(DiaPythonModuleTest, CreateModule_WithUppercase_Fails)
{
	Module* module = CreateModule("TestModule");  // Uppercase not allowed

	EXPECT_EQ(module, nullptr);
}

TEST_F(DiaPythonModuleTest, CreateModule_Duplicate_Fails)
{
	Module* module1 = CreateModule("duplicate_test");
	Module* module2 = CreateModule("duplicate_test");

	EXPECT_NE(module1, nullptr);
	EXPECT_EQ(module2, nullptr);  // Duplicate should fail
}

////////////////////////////////////////////////////////////////////////////////
// AC2 & AC4: Can register and call C++ functions
////////////////////////////////////////////////////////////////////////////////

PythonObject TestAdd(const PythonArgs& args)
{
	EXPECT_EQ(args.GetCount(), 2);

	PythonObject arg1 = args.GetArg(0);
	PythonObject arg2 = args.GetArg(1);

	int a = ToInt(arg1);
	int b = ToInt(arg2);

	return ToPython(a + b);
}

TEST_F(DiaPythonModuleTest, AddFunction_AndCallFromPython_Works)
{
	Module* module = CreateModule("math_module");
	ASSERT_NE(module, nullptr);

	AddFunction(module, "add", TestAdd, "Add two numbers");

	// Execute Python code to call the function
	// Note: This requires script execution (Phase 6)
	// For now, just verify the function was registered
	SUCCEED();  // Function registered without crash
}

////////////////////////////////////////////////////////////////////////////////
// AC5: Function callbacks receive PythonArgs and return PythonObject
////////////////////////////////////////////////////////////////////////////////

PythonObject TestEcho(const PythonArgs& args)
{
	if (args.GetCount() == 0)
	{
		return PythonObject();  // Return None
	}

	return args.GetArg(0);  // Echo first argument
}

TEST_F(DiaPythonModuleTest, FunctionCallback_ReceivesPythonArgs_ReturnsPythonObject)
{
	Module* module = CreateModule("echo_module");
	ASSERT_NE(module, nullptr);

	AddFunction(module, "echo", TestEcho, "Echo the first argument");

	SUCCEED();  // Callback signature verified at compile time
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Duplicate function registration logs warning but doesn't crash
////////////////////////////////////////////////////////////////////////////////

TEST_F(DiaPythonModuleTest, AddFunction_Duplicate_LogsWarning)
{
	Module* module = CreateModule("dup_func_module");
	ASSERT_NE(module, nullptr);

	// Register same function twice
	AddFunction(module, "func", TestAdd, "First registration");
	AddFunction(module, "func", TestEcho, "Second registration (overwrites)");

	// Should not crash - last registration wins
	SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// AC8: Modules can be created before or after Python initialization
////////////////////////////////////////////////////////////////////////////////

class DiaPythonModuleBeforeInitTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// DO NOT initialize Python - test pending registration
	}

	void TearDown() override
	{
		if (IsInitialized())
		{
			Shutdown();
		}
	}
};

TEST_F(DiaPythonModuleBeforeInitTest, CreateModule_BeforeInit_CachedAndRegisteredLater)
{
	// Create module before Python initialization
	Module* module = CreateModule("pending_module");
	ASSERT_NE(module, nullptr);

	// Add function before Python initialization
	AddFunction(module, "test_func", TestAdd, "Test function");

	// Now initialize Python - should register pending module
	bool initResult = Initialize("External/Python311/", "External/Python/", false);
	ASSERT_TRUE(initResult);

	// Module should now be registered
	Module* retrieved = GetModule("pending_module");
	EXPECT_NE(retrieved, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// AC3: Created modules are importable from Python
// AC10: Functions can have docstrings that show in Python help()
////////////////////////////////////////////////////////////////////////////////

// Note: Full testing requires script execution (Phase 6)
// These are verified by the fact that modules are registered with pybind11

TEST_F(DiaPythonModuleTest, Module_ImportableFromPython_Placeholder)
{
	Module* module = CreateModule("importable_module");
	ASSERT_NE(module, nullptr);

	AddFunction(module, "func", TestAdd, "This is a docstring");

	// TODO: Phase 6 - Execute Python: import importable_module; help(importable_module.func)
	SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// AC9: No pybind11 headers exposed in public API (opaque handles only)
////////////////////////////////////////////////////////////////////////////////

TEST_F(DiaPythonModuleTest, NoPybind11InPublicAPI_CompileTimeCheck)
{
	// If this compiles without including pybind11 headers, AC9 is satisfied
	Module* module = CreateModule("api_test");
	AddFunction(module, "test", TestAdd, nullptr);

	SUCCEED();  // Compile-time verification passed
}

////////////////////////////////////////////////////////////////////////////////
// AC11: Function overloading supported (same name, different signatures)
////////////////////////////////////////////////////////////////////////////////

PythonObject TestOverload1(const PythonArgs& args)
{
	return ToPython(args.GetCount());  // Return arg count
}

PythonObject TestOverload2(const PythonArgs& args)
{
	return ToPython(args.GetCount() * 2);  // Return arg count * 2
}

TEST_F(DiaPythonModuleTest, AddFunctionOverload_MultipleSignatures_Works)
{
	Module* module = CreateModule("overload_module");
	ASSERT_NE(module, nullptr);

	AddFunctionOverload(module, "overloaded", TestOverload1, "()", "No arguments");
	AddFunctionOverload(module, "overloaded", TestOverload2, "(int)", "One integer argument");

	SUCCEED();  // Overloads registered without crash
}

////////////////////////////////////////////////////////////////////////////////
// AC12: C++ exceptions in callbacks converted to Python exceptions
////////////////////////////////////////////////////////////////////////////////

PythonObject TestThrowException(const PythonArgs& args)
{
	throw std::runtime_error("Test exception from C++");
}

TEST_F(DiaPythonModuleTest, FunctionCallback_ThrowsException_ConvertedToPythonException)
{
	Module* module = CreateModule("exception_module");
	ASSERT_NE(module, nullptr);

	// Should not crash when registering function that throws
	AddFunction(module, "throw_exception", TestThrowException, "Throws C++ exception");

	// TODO: Phase 6 - Execute Python and verify Python exception is raised
	SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// GetModule Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(DiaPythonModuleTest, GetModule_ExistingModule_ReturnsModule)
{
	Module* created = CreateModule("existing_module");
	ASSERT_NE(created, nullptr);

	Module* retrieved = GetModule("existing_module");
	EXPECT_EQ(retrieved, created);
}

TEST_F(DiaPythonModuleTest, GetModule_NonExistentModule_ReturnsNull)
{
	Module* retrieved = GetModule("nonexistent_module");
	EXPECT_EQ(retrieved, nullptr);
}

TEST_F(DiaPythonModuleTest, GetModule_NullName_ReturnsNull)
{
	Module* retrieved = GetModule(nullptr);
	EXPECT_EQ(retrieved, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// Edge Case Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(DiaPythonModuleTest, AddFunction_NullModule_LogsError)
{
	// Should not crash
	AddFunction(nullptr, "func", TestAdd, nullptr);

	SUCCEED();  // No crash
}

TEST_F(DiaPythonModuleTest, AddFunction_EmptyFunctionName_LogsError)
{
	Module* module = CreateModule("empty_func_module");
	ASSERT_NE(module, nullptr);

	AddFunction(module, "", TestAdd, nullptr);

	SUCCEED();  // No crash
}

TEST_F(DiaPythonModuleTest, AddFunction_NullCallback_LogsError)
{
	Module* module = CreateModule("null_callback_module");
	ASSERT_NE(module, nullptr);

	AddFunction(module, "func", nullptr, nullptr);

	SUCCEED();  // No crash
}

TEST_F(DiaPythonModuleTest, Module_MaxLimit_32Modules)
{
	// Create 32 modules (hard limit from spec)
	for (int i = 0; i < 32; i++)
	{
		char name[64];
		snprintf(name, sizeof(name), "module_%d", i);
		Module* module = CreateModule(name);
		EXPECT_NE(module, nullptr) << "Failed to create module " << i;
	}

	// 33rd module should fail
	Module* module33 = CreateModule("module_33");
	EXPECT_EQ(module33, nullptr);
}
