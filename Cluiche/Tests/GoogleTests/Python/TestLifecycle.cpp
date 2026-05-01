////////////////////////////////////////////////////////////////////////////////
// Filename: TestLifecycle.cpp - Google Test for DiaPython Lifecycle
// Feature spec: docs/specs/features/dia/diapython/interpreter-lifecycle.md
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaPython/DiaPython.h>

using namespace Dia::Python;

////////////////////////////////////////////////////////////////////////////////
// DiaPython Interpreter Lifecycle Tests
////////////////////////////////////////////////////////////////////////////////

// Test fixture for lifecycle tests
class DiaPythonLifecycleTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Ensure Python is not initialized at start of each test
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
};

////////////////////////////////////////////////////////////////////////////////
// Acceptance Criteria Tests
////////////////////////////////////////////////////////////////////////////////

// AC1: Python interpreter can be initialized via Dia::Python::Initialize()
TEST_F(DiaPythonLifecycleTest, Initialize_Success)
{
	bool result = Initialize("External/Python311/", "External/Python/", false);

	EXPECT_TRUE(result);
	EXPECT_TRUE(IsInitialized());
}

// AC2: Python interpreter can be shut down via Dia::Python::Shutdown()
TEST_F(DiaPythonLifecycleTest, Shutdown_Success)
{
	Initialize("External/Python311/", "External/Python/", false);

	Shutdown();

	EXPECT_FALSE(IsInitialized());
}

// AC3: Dia::Python::IsInitialized() returns correct state
TEST_F(DiaPythonLifecycleTest, IsInitialized_ReturnsCorrectState)
{
	// Before initialization
	EXPECT_FALSE(IsInitialized());

	// After initialization
	Initialize("External/Python311/", "External/Python/", false);
	EXPECT_TRUE(IsInitialized());

	// After shutdown
	Shutdown();
	EXPECT_FALSE(IsInitialized());
}

// AC4: Calling Initialize() twice logs warning but doesn't crash
TEST_F(DiaPythonLifecycleTest, Initialize_Twice_DoesNotCrash)
{
	bool result1 = Initialize("External/Python311/", "External/Python/", false);
	EXPECT_TRUE(result1);

	// Second call should return true (idempotent) and log warning
	bool result2 = Initialize("External/Python311/", "External/Python/", false);
	EXPECT_TRUE(result2);
	EXPECT_TRUE(IsInitialized());

	// Should still work normally
	Shutdown();
	EXPECT_FALSE(IsInitialized());
}

// AC5: Calling Shutdown() when not initialized logs warning but doesn't crash
TEST_F(DiaPythonLifecycleTest, Shutdown_WhenNotInitialized_DoesNotCrash)
{
	EXPECT_FALSE(IsInitialized());

	// Should not crash, just log warning
	EXPECT_NO_THROW(Shutdown());

	EXPECT_FALSE(IsInitialized());
}

// AC6: Attempting to execute Python code when not initialized returns error code
// (This will be tested in Phase 6: script-execution tests)
// Placeholder test to document the requirement
TEST_F(DiaPythonLifecycleTest, ExecuteCode_WhenNotInitialized_ReturnsError)
{
	EXPECT_FALSE(IsInitialized());
	// TODO: Phase 6 - Test ExecuteString returns error code 3 when not initialized
	// int exitCode = ExecuteString("print('test')");
	// EXPECT_EQ(exitCode, 3);  // ErrorCode::NotInitialized
}

// AC7: Python sys.path is configured correctly (can import standard library)
TEST_F(DiaPythonLifecycleTest, SysPath_ConfiguredCorrectly)
{
	bool result = Initialize("External/Python311/", "External/Python/", false);
	ASSERT_TRUE(result);
	ASSERT_TRUE(IsInitialized());

	// Test that we can import standard library modules
	// This will be fully testable in Phase 6 (script-execution)
	// For now, just verify initialization succeeded with paths

	// TODO: Phase 6 - Execute Python code to verify sys.path contents
	// ExecuteString("import sys; assert 'External/Python311/' in sys.path");
	// ExecuteString("import os");  // Should succeed if stdlib accessible

	SUCCEED();  // Initialization with paths succeeded
}

// AC8: OnPythonInitialized event fires after successful initialization
// (Requires Observer pattern integration - Phase 7)
TEST_F(DiaPythonLifecycleTest, OnPythonInitialized_EventFires)
{
	// TODO: Phase 7 - Register observer and verify event fires
	// Observer observer;
	// RegisterInitializedObserver(&observer);
	// Initialize(...);
	// EXPECT_TRUE(observer.wasCalled);

	SUCCEED();  // Placeholder for future event testing
}

// AC9: OnPythonShutdown event fires before shutdown
// (Requires Observer pattern integration - Phase 7)
TEST_F(DiaPythonLifecycleTest, OnPythonShutdown_EventFires)
{
	// TODO: Phase 7 - Register observer and verify event fires
	// Observer observer;
	// RegisterShutdownObserver(&observer);
	// Initialize(...);
	// Shutdown();
	// EXPECT_TRUE(observer.wasCalled);

	SUCCEED();  // Placeholder for future event testing
}

////////////////////////////////////////////////////////////////////////////////
// Additional Edge Case Tests
////////////////////////////////////////////////////////////////////////////////

// Test initialization with null paths (should handle gracefully)
TEST_F(DiaPythonLifecycleTest, Initialize_WithNullPaths_Succeeds)
{
	bool result = Initialize(nullptr, nullptr, false);

	EXPECT_TRUE(result);
	EXPECT_TRUE(IsInitialized());
}

// Test initialization with empty string paths
TEST_F(DiaPythonLifecycleTest, Initialize_WithEmptyPaths_Succeeds)
{
	bool result = Initialize("", "", false);

	EXPECT_TRUE(result);
	EXPECT_TRUE(IsInitialized());
}

// Test initialization with warning capture enabled
TEST_F(DiaPythonLifecycleTest, Initialize_WithWarningCapture_Succeeds)
{
	bool result = Initialize("External/Python311/", "External/Python/", true);

	EXPECT_TRUE(result);
	EXPECT_TRUE(IsInitialized());
}

// Test multiple init/shutdown cycles
TEST_F(DiaPythonLifecycleTest, MultipleInitShutdownCycles_Work)
{
	for (int i = 0; i < 3; i++)
	{
		bool initResult = Initialize("External/Python311/", "External/Python/", false);
		EXPECT_TRUE(initResult);
		EXPECT_TRUE(IsInitialized());

		Shutdown();
		EXPECT_FALSE(IsInitialized());
	}
}
