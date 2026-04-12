# Writing Tests with Google Test

This guide provides practical examples and patterns for writing effective tests in the Cluiche project using Google Test.

## Table of Contents
1. [Basic Test Structure](#basic-test-structure)
2. [Testing Math Components](#testing-math-components)
3. [Testing Containers](#testing-containers)
4. [Floating-Point Comparisons](#floating-point-comparisons)
5. [Death Tests (Assertions)](#death-tests-assertions)
6. [Test Organization](#test-organization)
7. [Common Pitfalls](#common-pitfalls)

---

## Basic Test Structure

### Simple Test

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

### Test Naming Convention

Use the pattern: `TEST(ComponentName, MethodName_Scenario_ExpectedBehavior)`

**Examples:**
- `TEST(Vector3D, Normalize_CreatesUnitVector)`
- `TEST(DynamicArray, Add_AppendsElementAndIncreasesSize)`
- `TEST(BitArray8, ConstructWithZero_AllBitsOff)`

---

## Testing Math Components

### Vector Tests with Floating-Point Comparison

```cpp
#include <gtest/gtest.h>
#include <DiaMaths/Vector/Vector3D.h>

using namespace Dia::Maths;

// Define epsilon for floating-point comparisons
constexpr float kEpsilon = 0.0001f;

TEST(Vector3D, Normalize_CreatesUnitVector)
{
    Vector3D v(3.0f, 4.0f, 0.0f);
    v.Normalize();
    
    float magnitude = v.Magnitude();
    
    // Use EXPECT_NEAR for floating-point comparisons
    EXPECT_NEAR(magnitude, 1.0f, kEpsilon);
}
```

### Matrix Tests with Transformation Verification

```cpp
TEST(Matrix33, FromTranslation_CreatesTranslationMatrix)
{
    Vector2D translation(10.0f, 20.0f);
    Matrix33 m = Matrix33::FromTranslation(translation);
    
    // Verify specific elements
    EXPECT_FLOAT_EQ(m(0, 2), 10.0f);  // Translation X
    EXPECT_FLOAT_EQ(m(1, 2), 20.0f);  // Translation Y
    
    // Verify it's still identity-like on diagonal
    EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 1.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
}
```

### Testing with Known Mathematical Properties

```cpp
TEST(Vector3D, Dot_ReturnsMagnitudeSquaredForSelf)
{
    Vector3D v(3.0f, 4.0f, 0.0f);
    
    float dotSelf = v.Dot(v);
    float squareMag = v.SquareMagnitude();
    
    // Verify mathematical property: v·v = |v|²
    EXPECT_NEAR(dotSelf, squareMag, kEpsilon);
}
```

---

## Testing Containers

### Construction and State Tests

```cpp
TEST(DynamicArray, ConstructFromCArray_CopiesElements)
{
    int cArray[3] = {1, 2, 3};
    DynamicArray<int> array(&cArray[0], 3);
    
    // Verify state
    EXPECT_EQ(array.Capacity(), 3);
    EXPECT_EQ(array.Size(), 3);
    EXPECT_TRUE(array.IsFull());
    
    // Verify contents
    EXPECT_EQ(array.At(0), 1);
    EXPECT_EQ(array.At(1), 2);
    EXPECT_EQ(array.At(2), 3);
}
```

### Mutation Tests

```cpp
TEST(DynamicArray, Remove_DecreasesSize)
{
    int cArray[3] = {1, 2, 3};
    DynamicArray<int> array(&cArray[0], 3);
    
    array.Remove();
    EXPECT_EQ(array.Size(), 2);
    EXPECT_FALSE(array.IsFull());
    
    array.Remove();
    EXPECT_EQ(array.Size(), 1);
    
    array.Remove();
    EXPECT_TRUE(array.IsEmpty());
}
```

### Copy Independence Tests

```cpp
TEST(DynamicArray, CopyConstructor_CreatesIndependentCopy)
{
    int cArray[3] = {1, 2, 3};
    DynamicArray<int> original(&cArray[0], 3);
    DynamicArray<int> copy(original);
    
    // Modify original
    const_cast<int&>(original.At(0)) = 99;
    
    // Verify copy is unchanged
    EXPECT_EQ(copy.At(0), 1) << "Copy should be independent of original";
}
```

---

## Floating-Point Comparisons

### When to Use Each Assertion

```cpp
// EXPECT_FLOAT_EQ - For exact equality (use sparingly)
EXPECT_FLOAT_EQ(v.x, 1.0f);

// EXPECT_NEAR - For approximate equality (preferred)
EXPECT_NEAR(magnitude, 1.0f, kEpsilon);

// EXPECT_DOUBLE_EQ / EXPECT_NEAR - For double precision
double precise_value = calculatePrecise();
EXPECT_NEAR(precise_value, 3.141592653589793, 1e-10);
```

### Epsilon Selection

```cpp
// General purpose (good for most tests)
constexpr float kEpsilon = 0.0001f;

// High precision (for accumulated operations)
constexpr float kTightEpsilon = 0.000001f;

// Loose tolerance (for complex calculations)
constexpr float kLooseEpsilon = 0.01f;
```

---

## Death Tests (Assertions)

### Testing Debug Assertions

```cpp
#ifdef DEBUG
TEST(DynamicArrayDeathTest, At_OutOfBounds_Asserts)
{
    DynamicArray<int> array(3);
    array.Add(1);
    
    // Expect assertion when accessing out of bounds
    EXPECT_DEATH({
        volatile int x = array.At(10);  // Out of bounds
        (void)x;  // Suppress unused variable warning
    }, "");  // Match any assertion message
}
#endif
```

**Important Notes:**
- Death tests only work in Debug builds (assertions disabled in Release)
- Always use `#ifdef DEBUG` guard
- Use `volatile` to prevent compiler optimization
- Cast to `(void)` to suppress warnings about unused variables

---

## Test Organization

### File Structure

```
Cluiche/Tests/GoogleTests/
├── Core/
│   └── Containers/
│       ├── TestDynamicArray.cpp
│       └── TestBitArray.cpp
└── Maths/
    ├── TestVector3D.cpp
    ├── TestVector4D.cpp
    ├── TestMatrix33.cpp
    └── TestVectorUtils.cpp
```

### Test Suite Organization

```cpp
// Group related tests under the same test suite name
TEST(Vector3D, DefaultConstruction_InitializesToZero) { }
TEST(Vector3D, ComponentConstruction_InitializesCorrectly) { }
TEST(Vector3D, Normalize_CreatesUnitVector) { }

// Use descriptive section comments
// ==============================================================================
// Construction Tests
// ==============================================================================

TEST(Vector3D, DefaultConstruction_InitializesToZero) { }

// ==============================================================================
// Arithmetic Operator Tests
// ==============================================================================

TEST(Vector3D, Addition_AddsComponentwise) { }
```

---

## Common Pitfalls

### 1. API Assumptions

**Problem:** Assuming API exists without verification
```cpp
// ❌ WRONG - Assumes Clear() exists
array.Clear();  // Compile error: Clear() doesn't exist

// ✅ CORRECT - Check API first
array.RemoveAll();  // Actual method name
```

**Solution:** Always check header files for actual API before writing tests.

### 2. Unimplemented Operators

**Problem:** Using declared but unimplemented operators
```cpp
// ❌ WRONG - operator- is declared but not implemented
Vector3D result = -v;  // Linker error

// ✅ CORRECT - Use implemented methods
Vector3D result = v.AsInverse();
```

**Solution:** Verify implementation in .cpp file, not just declaration in header.

### 3. Floating-Point Exact Equality

**Problem:** Using exact equality for floating-point
```cpp
// ❌ WRONG - May fail due to floating-point precision
EXPECT_EQ(v.Magnitude(), 1.0f);

// ✅ CORRECT - Use epsilon tolerance
EXPECT_NEAR(v.Magnitude(), 1.0f, kEpsilon);
```

### 4. Missing Custom Messages

**Problem:** Unclear failure messages
```cpp
// ❌ LESS HELPFUL
EXPECT_TRUE(array.IsEmpty());

// ✅ MORE HELPFUL - Add context
EXPECT_TRUE(array.IsEmpty()) << "Array should be empty after RemoveAll()";
```

---

## Best Practices Summary

1. **Test One Thing:** Each test should verify one specific behavior
2. **Descriptive Names:** Use `MethodName_Scenario_ExpectedBehavior` format
3. **Arrange-Act-Assert:** Organize tests with clear setup, action, and verification
4. **Use EXPECT over ASSERT:** `EXPECT_*` continues on failure, `ASSERT_*` stops (use ASSERT only when subsequent code would crash)
5. **Verify API First:** Check headers AND implementation before writing tests
6. **Document Limitations:** Comment out tests for unimplemented features with explanation
7. **Floating-Point Tolerance:** Always use `EXPECT_NEAR` for floating-point comparisons
8. **Meaningful Messages:** Add custom failure messages with `<<` operator

---

## Example: Complete Test File Structure

```cpp
// TestVector3D.cpp - Google Test unit tests for Vector3D
//
// Tests the Dia::Maths::Vector3D class for 3D vector operations

#include <gtest/gtest.h>
#include <DiaMaths/Vector/Vector3D.h>

using namespace Dia::Maths;

constexpr float kEpsilon = 0.0001f;

// ==============================================================================
// Construction Tests
// ==============================================================================

TEST(Vector3D, DefaultConstruction_InitializesToZero)
{
    Vector3D v;

    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vector3D, ComponentConstruction_InitializesCorrectly)
{
    Vector3D v(1.0f, 2.0f, 3.0f);

    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

// ==============================================================================
// Normalization Tests
// ==============================================================================

TEST(Vector3D, Normalize_CreatesUnitVector)
{
    Vector3D v(3.0f, 4.0f, 0.0f);
    v.Normalize();

    float magnitude = v.Magnitude();

    EXPECT_NEAR(magnitude, 1.0f, kEpsilon);
}

TEST(Vector3D, AsNormal_ReturnsNormalizedCopy)
{
    Vector3D v(3.0f, 4.0f, 0.0f);
    Vector3D normal = v.AsNormal();

    // Original unchanged
    EXPECT_FLOAT_EQ(v.x, 3.0f);

    // Normal has unit length
    EXPECT_NEAR(normal.Magnitude(), 1.0f, kEpsilon);
}
```

---

*Last Updated: 2026-04-11*
*For migration from custom framework, see: [google-test-migration.md](google-test-migration.md)*
