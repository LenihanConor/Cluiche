# Google Test Migration Guide

This document provides guidance for migrating tests from the custom UnitTest framework to Google Test.

## Overview

The Cluiche project is migrating from a custom test framework to Google Test (gtest) to enable:
- **CI/CD Integration**: Native XML output for GitHub Actions
- **IDE Integration**: Visual Studio Test Explorer support
- **Modern Features**: Fixtures, parameterized tests, death tests, mocking
- **Industry Standard**: Extensive documentation and community support

## Getting Started

### Project Location

- **New Test Project**: `Cluiche/Tests/GoogleTests/`
- **Test Executable**: `bin/exe/Debug/GoogleTests.exe` (Win32) or `bin/exe/Debug-x64/GoogleTests.exe` (x64)

### Building and Running Tests

**Build in Visual Studio:**
1. Open `Cluiche/Cluiche.sln`
2. Select `GoogleTests` project in Solution Explorer
3. Build → Build GoogleTests (or F7)

**Run from Command Line:**
```bash
# Win32 Debug
cd Cluiche
bin/exe/Debug/GoogleTests.exe

# With XML output (for CI/CD)
bin/exe/Debug/GoogleTests.exe --gtest_output=xml:test-results.xml

# Run specific tests
bin/exe/Debug/GoogleTests.exe --gtest_filter=DynamicArray.*

# List all tests
bin/exe/Debug/GoogleTests.exe --gtest_list_tests
```

**Run in Visual Studio Test Explorer:**
1. Install "Test Adapter for Google Test" extension
2. Build GoogleTests project
3. Open Test → Test Explorer
4. Click "Run All Tests"

---

## Migration Patterns

### Basic Test Conversion

**Old Custom Framework:**
```cpp
#include "UnitTests/Infrastructure/UnitTestMacros.h"

class UnitTestDynamicArray : public UnitTestCoreContainers {
    void DoTest();
};

void UnitTestDynamicArray::DoTest()
{
    UNIT_TEST_BLOCK_START()
        DynamicArray<int> array(3);
        UNIT_TEST_POSITIVE(array.Capacity() == 3, "DynamicArray()");
        UNIT_TEST_POSITIVE(array.Size() == 0, "DynamicArray()");
    UNIT_TEST_BLOCK_END()
    
    mState = kFinished;
}
```

**New Google Test:**
```cpp
#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>

using namespace Dia::Core::Containers;

TEST(DynamicArray, DefaultConstruction_CreatesEmptyArrayWithCapacity)
{
    DynamicArray<int> array(3);
    
    EXPECT_EQ(array.Capacity(), 3);
    EXPECT_EQ(array.Size(), 0);
}
```

### Macro Conversion Table

| Old Custom Framework | Google Test | Notes |
|---------------------|-------------|-------|
| `UNIT_TEST_POSITIVE(cond, msg)` | `EXPECT_TRUE(cond) << msg` | Non-fatal assertion |
| `UNIT_TEST_NEGATIVE(cond, msg)` | `EXPECT_FALSE(cond) << msg` | Non-fatal assertion |
| `UNIT_TEST_BLOCK_START()` | `TEST(Suite, TestName) {` | Define test case |
| `UNIT_TEST_BLOCK_END()` | `}` | End test case |
| `UNIT_TEST_ASSERT_EXPECTED_START()` | `EXPECT_DEATH({` | Test that code asserts |
| `UNIT_TEST_ASSERT_EXPECTED_END()` | `}, "");` | Match any assertion message |
| `mState = kFinished` | *(remove)* | Google Test handles state |

### Assertion Types

**Google Test provides rich assertions:**

```cpp
// Equality
EXPECT_EQ(actual, expected);      // Non-fatal: actual == expected
ASSERT_EQ(actual, expected);      // Fatal: stops test if fails

// Boolean
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);

// Comparison
EXPECT_LT(val1, val2);           // val1 < val2
EXPECT_LE(val1, val2);           // val1 <= val2
EXPECT_GT(val1, val2);           // val1 > val2
EXPECT_GE(val1, val2);           // val1 >= val2

// Floating point
EXPECT_FLOAT_EQ(val1, val2);    // Floating point equality
EXPECT_NEAR(val1, val2, eps);   // Within epsilon

// String
EXPECT_STREQ(str1, str2);       // C-string equality
EXPECT_STRNE(str1, str2);       // C-string inequality

// Exceptions
EXPECT_THROW(stmt, exception);   // Statement throws exception
EXPECT_NO_THROW(stmt);          // Statement doesn't throw

// Custom messages
EXPECT_TRUE(cond) << "Custom failure message: " << variable;
```

### Test Organization

**Naming Convention:**
- Test Suite Name = Class/Component Name (e.g., `DynamicArray`)
- Test Name = Behavior/Scenario (e.g., `Add_AppendsElementAndIncreasesSize`)

```cpp
// Format: TEST(SuiteName, TestName)
TEST(DynamicArray, DefaultConstruction_CreatesEmptyArrayWithCapacity) { }
TEST(DynamicArray, Add_AppendsElementAndIncreasesSize) { }
TEST(DynamicArray, Remove_DecreasesSize) { }
```

**File Organization:**
- One file per class: `TestDynamicArray.cpp`
- Group related tests: All `DynamicArray` tests in one file
- Location: `GoogleTests/Core/Containers/TestDynamicArray.cpp`

---

## Advanced Features

### Test Fixtures

Use fixtures for common setup/teardown:

```cpp
class DynamicArrayFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup before each test
        array = new DynamicArray<int>(10);
    }
    
    void TearDown() override {
        // Common teardown after each test
        delete array;
    }
    
    // Shared test data
    DynamicArray<int>* array;
};

TEST_F(DynamicArrayFixture, Add_AppendsElement)
{
    array->Add(42);
    EXPECT_EQ(array->Size(), 1);
    EXPECT_EQ(array->At(0), 42);
}

TEST_F(DynamicArrayFixture, Clear_EmptiesArray)
{
    array->Add(1);
    array->Add(2);
    array->Clear();
    EXPECT_TRUE(array->IsEmpty());
}
```

### Parameterized Tests

Test same logic with multiple inputs:

```cpp
struct BitwiseTestParams {
    unsigned char a, b, expected_or, expected_and;
};

class BitArray8BitwiseTest : public ::testing::TestWithParam<BitwiseTestParams> {};

TEST_P(BitArray8BitwiseTest, BitwiseOperations)
{
    auto params = GetParam();
    BitArray8 bitflag1(params.a);
    BitArray8 bitflag2(params.b);
    
    BitArray8 result_or = bitflag1 | bitflag2;
    BitArray8 result_and = bitflag1 & bitflag2;
    
    EXPECT_EQ(result_or.GetAllBits(), params.expected_or);
    EXPECT_EQ(result_and.GetAllBits(), params.expected_and);
}

INSTANTIATE_TEST_SUITE_P(
    BitwiseOps,
    BitArray8BitwiseTest,
    ::testing::Values(
        BitwiseTestParams{1, 2, 3, 0},          // 01 | 10 = 11, & = 00
        BitwiseTestParams{0x0F, 0xF0, 0xFF, 0}  // Low | High nibbles
    )
);
```

### Death Tests

Verify that code asserts or crashes:

```cpp
#ifdef DEBUG
TEST(DynamicArrayDeathTest, At_OutOfBounds_Asserts)
{
    DynamicArray<int> array(3);
    array.Add(1);
    
    // Expect assertion when accessing out of bounds
    EXPECT_DEATH({
        volatile int x = array.At(10);  // Out of bounds
        (void)x;
    }, "");  // Match any assertion message
}
#endif
```

---

## Migration Checklist

When converting a test file:

- [ ] Update includes: `#include <gtest/gtest.h>`
- [ ] Remove custom framework includes: `UnitTestMacros.h`, `UnitTestInterface.h`
- [ ] Convert `DoTest()` method to `TEST()` macros
- [ ] Split large test methods into focused `TEST()` cases
- [ ] Convert `UNIT_TEST_POSITIVE/NEGATIVE` to `EXPECT_TRUE/FALSE`
- [ ] Remove `UNIT_TEST_BLOCK_START()/END()`
- [ ] Convert expected assertions to `EXPECT_DEATH` blocks (#ifdef DEBUG)
- [ ] Remove `mState = kFinished`
- [ ] Add descriptive test names
- [ ] Consider using fixtures for repeated setup
- [ ] Add custom failure messages where helpful

---

## Common Patterns

### Testing Containers

```cpp
TEST(DynamicArray, Add_FillsToCapacity)
{
    DynamicArray<int> array(3);
    
    array.Add(1);
    array.Add(2);
    array.Add(3);
    
    EXPECT_TRUE(array.IsFull());
    EXPECT_EQ(array.Size(), 3);
    EXPECT_EQ(array.Capacity(), 3);
}
```

### Testing Math with Floating Point

```cpp
TEST(Vector2D, Normalize_CreatesUnitVector)
{
    Vector2D v(3.0f, 4.0f);  // Length = 5.0
    v.Normalize();
    
    float length = v.Length();
    EXPECT_NEAR(length, 1.0f, 0.0001f);  // Within epsilon
}
```

### Testing Callbacks/Events

```cpp
TEST(Delegate, Invoke_CallsAllSubscribers)
{
    int callCount = 0;
    auto callback = [&callCount]() { callCount++; };
    
    Delegate<void()> delegate;
    delegate.Add(callback);
    delegate.Add(callback);
    
    delegate.Invoke();
    
    EXPECT_EQ(callCount, 2);
}
```

---

## Tips and Best Practices

1. **One Behavior Per Test**: Each `TEST()` should verify one specific behavior
2. **Descriptive Names**: Use `MethodName_Scenario_ExpectedBehavior` format
3. **Arrange-Act-Assert**: Organize tests with clear setup, action, and verification
4. **Use EXPECT vs ASSERT**: Use `EXPECT_*` (non-fatal) for most assertions, `ASSERT_*` (fatal) only when subsequent code would crash
5. **Avoid Test Interdependence**: Each test should run independently
6. **Fast Tests**: Keep unit tests fast (< 100ms each)
7. **Readable Failures**: Add context to assertions: `EXPECT_TRUE(x) << "Expected x to be true because..."`

---

## Resources

- **Google Test Primer**: https://google.github.io/googletest/primer.html
- **Google Test Documentation**: https://google.github.io/googletest/
- **Assertions Reference**: https://google.github.io/googletest/reference/assertions.html
- **Advanced Features**: https://google.github.io/googletest/advanced.html

---

## Getting Help

- Check existing converted tests in `GoogleTests/Core/Containers/`
- Review `TestDynamicArray.cpp` and `TestBitArray.cpp` as examples
- Ask questions in project discussions or code reviews

---

*Last Updated: 2026-04-11*
*Migration Status: Phase 1 Complete (Foundation)*
