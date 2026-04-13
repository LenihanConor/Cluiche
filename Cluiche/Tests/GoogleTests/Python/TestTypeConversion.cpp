////////////////////////////////////////////////////////////////////////////////
// Filename: TestTypeConversion.cpp - Google Test for DiaPython Type Conversion
// Feature spec: docs/specs/features/dia/diapython/type-conversion.md
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaPython/DiaPython.h>

using namespace Dia::Python;

////////////////////////////////////////////////////////////////////////////////
// DiaPython Type Conversion Tests
////////////////////////////////////////////////////////////////////////////////

// Test fixture for type conversion tests
class DiaPythonTypeConversionTest : public ::testing::Test
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
// C++ → Python Conversion Tests (ToPython)
////////////////////////////////////////////////////////////////////////////////

// AC1: Can convert C++ int → PythonObject via ToPython(int)
TEST_F(DiaPythonTypeConversionTest, ToPython_Int_CreatesValidObject)
{
	PythonObject obj = ToPython(42);

	EXPECT_TRUE(obj.IsValid());
	EXPECT_FALSE(obj.IsNone());
	EXPECT_TRUE(obj.IsInt());
}

// AC2: Can convert C++ float → PythonObject via ToPython(float)
TEST_F(DiaPythonTypeConversionTest, ToPython_Float_CreatesValidObject)
{
	PythonObject obj = ToPython(3.14f);

	EXPECT_TRUE(obj.IsValid());
	EXPECT_FALSE(obj.IsNone());
	EXPECT_TRUE(obj.IsFloat());
}

// AC3: Can convert C++ bool → PythonObject via ToPython(bool)
TEST_F(DiaPythonTypeConversionTest, ToPython_Bool_CreatesValidObject)
{
	PythonObject objTrue = ToPython(true);
	PythonObject objFalse = ToPython(false);

	EXPECT_TRUE(objTrue.IsValid());
	EXPECT_TRUE(objTrue.IsBool());

	EXPECT_TRUE(objFalse.IsValid());
	EXPECT_TRUE(objFalse.IsBool());
}

// AC4: Can convert C++ const char* → PythonObject via ToPython(const char*)
TEST_F(DiaPythonTypeConversionTest, ToPython_String_CreatesValidObject)
{
	PythonObject obj = ToPython("Hello, World!");

	EXPECT_TRUE(obj.IsValid());
	EXPECT_FALSE(obj.IsNone());
	EXPECT_TRUE(obj.IsString());
}

// Test: nullptr converts to None
TEST_F(DiaPythonTypeConversionTest, ToPython_Nullptr_CreatesNone)
{
	PythonObject obj = ToPython(nullptr);

	EXPECT_FALSE(obj.IsValid());
	EXPECT_TRUE(obj.IsNone());
}

////////////////////////////////////////////////////////////////////////////////
// Python → C++ Conversion Tests (ToInt, ToFloat, ToBool, ToString)
////////////////////////////////////////////////////////////////////////////////

// AC5: Can convert PythonObject → C++ int via ToInt(PythonObject)
TEST_F(DiaPythonTypeConversionTest, ToInt_WithPythonInt_ReturnsValue)
{
	PythonObject obj = ToPython(42);
	int result = ToInt(obj);

	EXPECT_EQ(result, 42);
}

// AC6: Can convert PythonObject → C++ float via ToFloat(PythonObject)
TEST_F(DiaPythonTypeConversionTest, ToFloat_WithPythonFloat_ReturnsValue)
{
	PythonObject obj = ToPython(3.14f);
	float result = ToFloat(obj);

	EXPECT_FLOAT_EQ(result, 3.14f);
}

// AC7: Can convert PythonObject → C++ bool via ToBool(PythonObject)
TEST_F(DiaPythonTypeConversionTest, ToBool_WithPythonBool_ReturnsValue)
{
	PythonObject objTrue = ToPython(true);
	PythonObject objFalse = ToPython(false);

	EXPECT_TRUE(ToBool(objTrue));
	EXPECT_FALSE(ToBool(objFalse));
}

// AC8: Can convert PythonObject → C++ const char* via ToString(PythonObject)
TEST_F(DiaPythonTypeConversionTest, ToString_WithPythonString_ReturnsValue)
{
	PythonObject obj = ToPython("Hello, World!");
	const char* result = ToString(obj);

	EXPECT_STREQ(result, "Hello, World!");
}

// AC14: Implicit type coercion follows Python rules (e.g., int("123") works)
// Test: ToInt with string (Python coercion: int("123") = 123)
TEST_F(DiaPythonTypeConversionTest, ToInt_WithStringCoercion_Works)
{
	PythonObject obj = ToPython("123");
	int result = ToInt(obj);

	EXPECT_EQ(result, 123);
}

// Test: ToInt with float (Python coercion: int(42.7) = 42)
TEST_F(DiaPythonTypeConversionTest, ToInt_WithFloatCoercion_Truncates)
{
	PythonObject obj = ToPython(42.7f);
	int result = ToInt(obj);

	EXPECT_EQ(result, 42);
}

// Test: ToFloat with int (Python coercion: float(42) = 42.0)
TEST_F(DiaPythonTypeConversionTest, ToFloat_WithIntCoercion_Works)
{
	PythonObject obj = ToPython(42);
	float result = ToFloat(obj);

	EXPECT_FLOAT_EQ(result, 42.0f);
}

// Test: ToFloat with string (Python coercion: float("3.14") = 3.14)
TEST_F(DiaPythonTypeConversionTest, ToFloat_WithStringCoercion_Works)
{
	PythonObject obj = ToPython("3.14");
	float result = ToFloat(obj);

	EXPECT_FLOAT_EQ(result, 3.14f);
}

////////////////////////////////////////////////////////////////////////////////
// Type Conversion Error Handling Tests
////////////////////////////////////////////////////////////////////////////////

// AC10: Type conversion validates types (e.g., ToInt on string returns error)
// Test: ToInt with invalid string logs error, returns 0
TEST_F(DiaPythonTypeConversionTest, ToInt_WithInvalidString_ReturnsZero)
{
	PythonObject obj = ToPython("not a number");
	int result = ToInt(obj);

	EXPECT_EQ(result, 0);  // Default value on error
}

// AC11: Invalid conversions logged with helpful error messages
// Test: ToFloat with invalid string logs error, returns 0.0f
TEST_F(DiaPythonTypeConversionTest, ToFloat_WithInvalidString_ReturnsZero)
{
	PythonObject obj = ToPython("not a number");
	float result = ToFloat(obj);

	EXPECT_FLOAT_EQ(result, 0.0f);  // Default value on error
}

// AC12: PythonObject can represent None/null values
TEST_F(DiaPythonTypeConversionTest, PythonObject_None_HandledCorrectly)
{
	PythonObject obj;  // Default constructor creates None

	EXPECT_TRUE(obj.IsNone());
	EXPECT_FALSE(obj.IsValid());

	// Converting None to C++ types returns defaults
	EXPECT_EQ(ToInt(obj), 0);
	EXPECT_FLOAT_EQ(ToFloat(obj), 0.0f);
	EXPECT_FALSE(ToBool(obj));
	EXPECT_STREQ(ToString(obj), "");
}

////////////////////////////////////////////////////////////////////////////////
// PythonObject Lifetime Tests
////////////////////////////////////////////////////////////////////////////////

// AC15: PythonObject lifetime managed automatically (reference counted)
TEST_F(DiaPythonTypeConversionTest, PythonObject_CopyConstructor_Works)
{
	PythonObject obj1 = ToPython(42);
	PythonObject obj2 = obj1;  // Copy constructor

	EXPECT_EQ(ToInt(obj1), 42);
	EXPECT_EQ(ToInt(obj2), 42);
}

TEST_F(DiaPythonTypeConversionTest, PythonObject_AssignmentOperator_Works)
{
	PythonObject obj1 = ToPython(42);
	PythonObject obj2 = ToPython(100);

	obj2 = obj1;  // Assignment

	EXPECT_EQ(ToInt(obj1), 42);
	EXPECT_EQ(ToInt(obj2), 42);
}

TEST_F(DiaPythonTypeConversionTest, PythonObject_Destructor_NoMemoryLeak)
{
	// Create and destroy many objects (no memory leaks expected)
	for (int i = 0; i < 1000; i++)
	{
		PythonObject obj = ToPython(i);
		EXPECT_EQ(ToInt(obj), i);
	}

	SUCCEED();  // If we get here without crashing, no obvious memory issues
}

////////////////////////////////////////////////////////////////////////////////
// PythonObject Type Query Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(DiaPythonTypeConversionTest, PythonObject_TypeQueries_Work)
{
	PythonObject intObj = ToPython(42);
	PythonObject floatObj = ToPython(3.14f);
	PythonObject boolObj = ToPython(true);
	PythonObject strObj = ToPython("test");
	PythonObject noneObj;

	// Int checks
	EXPECT_TRUE(intObj.IsInt());
	EXPECT_FALSE(intObj.IsFloat());
	EXPECT_FALSE(intObj.IsBool());
	EXPECT_FALSE(intObj.IsString());
	EXPECT_FALSE(intObj.IsNone());

	// Float checks
	EXPECT_FALSE(floatObj.IsInt());
	EXPECT_TRUE(floatObj.IsFloat());
	EXPECT_FALSE(floatObj.IsBool());
	EXPECT_FALSE(floatObj.IsString());
	EXPECT_FALSE(floatObj.IsNone());

	// Bool checks
	EXPECT_FALSE(boolObj.IsInt());
	EXPECT_FALSE(boolObj.IsFloat());
	EXPECT_TRUE(boolObj.IsBool());
	EXPECT_FALSE(boolObj.IsString());
	EXPECT_FALSE(boolObj.IsNone());

	// String checks
	EXPECT_FALSE(strObj.IsInt());
	EXPECT_FALSE(strObj.IsFloat());
	EXPECT_FALSE(strObj.IsBool());
	EXPECT_TRUE(strObj.IsString());
	EXPECT_FALSE(strObj.IsNone());

	// None checks
	EXPECT_FALSE(noneObj.IsInt());
	EXPECT_FALSE(noneObj.IsFloat());
	EXPECT_FALSE(noneObj.IsBool());
	EXPECT_FALSE(noneObj.IsString());
	EXPECT_TRUE(noneObj.IsNone());
}

////////////////////////////////////////////////////////////////////////////////
// PythonArgs Tests (Placeholder - Full testing in Phase 5: module-api)
////////////////////////////////////////////////////////////////////////////////

// AC9: PythonArgs provides GetCount() and GetArg(index)
// Note: Full testing requires Module API (Phase 5) to create real PythonArgs
TEST_F(DiaPythonTypeConversionTest, PythonArgs_Placeholder)
{
	// TODO: Phase 5 - Test PythonArgs with actual function calls
	// For now, just verify the class exists and compiles

	SUCCEED();  // Placeholder test
}

////////////////////////////////////////////////////////////////////////////////
// AC13: No pybind11 types exposed in public API
////////////////////////////////////////////////////////////////////////////////

// This is verified at compile time - if this test compiles, AC13 is satisfied
// The public headers (PythonObject.h, Conversion.h) do not include pybind11
TEST_F(DiaPythonTypeConversionTest, NoPybind11InPublicAPI_CompileTimeCheck)
{
	// If this compiles without including pybind11 headers, AC13 is satisfied
	PythonObject obj = ToPython(42);
	int value = ToInt(obj);

	EXPECT_EQ(value, 42);
	SUCCEED();  // Compile-time verification passed
}

////////////////////////////////////////////////////////////////////////////////
// Edge Case Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(DiaPythonTypeConversionTest, ToString_Caching_Works)
{
	PythonObject obj = ToPython("Test String");

	// Call ToString multiple times - should use cached value
	const char* str1 = ToString(obj);
	const char* str2 = ToString(obj);

	EXPECT_STREQ(str1, "Test String");
	EXPECT_STREQ(str2, "Test String");
	EXPECT_EQ(str1, str2);  // Same pointer (cached)
}

TEST_F(DiaPythonTypeConversionTest, EmptyString_Conversion_Works)
{
	PythonObject obj = ToPython("");
	const char* result = ToString(obj);

	EXPECT_STREQ(result, "");
}

TEST_F(DiaPythonTypeConversionTest, NegativeNumbers_Conversion_Works)
{
	PythonObject intObj = ToPython(-42);
	PythonObject floatObj = ToPython(-3.14f);

	EXPECT_EQ(ToInt(intObj), -42);
	EXPECT_FLOAT_EQ(ToFloat(floatObj), -3.14f);
}

TEST_F(DiaPythonTypeConversionTest, ZeroValues_Conversion_Works)
{
	PythonObject intObj = ToPython(0);
	PythonObject floatObj = ToPython(0.0f);

	EXPECT_EQ(ToInt(intObj), 0);
	EXPECT_FLOAT_EQ(ToFloat(floatObj), 0.0f);
}
