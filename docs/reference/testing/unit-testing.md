# Unit Testing

**Last Updated:** 2026-04-01

Unit testing approach and examples for Cluiche and Dia engine.

---

## Overview

Unit tests verify individual components in isolation.

**Scope:**
- Functions, classes, templates
- Single responsibility testing
- No external dependencies (mocked if needed)
- Fast execution (< 1ms per test)

**Related Documents:**
- **[→ Testing Strategy](test.md)** - Overall testing approach
- **[→ Integration Testing](integration-testing.md)** - Cross-component testing
- **[→ Test Coverage Targets](test-coverage-targets.md)** - Coverage goals

---

## Unit Testing Framework

### Current: In-Engine Tests

**Location:** `Cluiche/Levels/UnitTestLevel/`

**Approach:**
- Tests embedded in UnitTestLevel
- DIA_ASSERT for validation
- Manual test execution

**Example:**
```cpp
void TestDynamicArray()
{
    Dia::Core::Containers::DynamicArray<int> array;
    
    // Test initial state
    DIA_ASSERT(array.Size() == 0, "Array should be empty");
    DIA_ASSERT(array.IsEmpty(), "Array should be empty");
    
    // Test add
    array.Add(10);
    DIA_ASSERT(array.Size() == 1, "Array should have 1 element");
    DIA_ASSERT(array[0] == 10, "First element should be 10");
    
    // Test remove
    array.RemoveAt(0);
    DIA_ASSERT(array.Size() == 0, "Array should be empty after remove");
    
    DIA_LOG("TestDynamicArray: PASSED");
}
```

---

### Future: Google Test

**Recommended for future integration**

**Example:**
```cpp
#include <gtest/gtest.h>
#include "DiaCore/Containers/Arrays/DynamicArray.h"

TEST(DynamicArrayTest, InitiallyEmpty)
{
    Dia::Core::Containers::DynamicArray<int> array;
    EXPECT_EQ(array.Size(), 0);
    EXPECT_TRUE(array.IsEmpty());
}

TEST(DynamicArrayTest, AddElement)
{
    Dia::Core::Containers::DynamicArray<int> array;
    array.Add(10);
    EXPECT_EQ(array.Size(), 1);
    EXPECT_EQ(array[0], 10);
}

TEST(DynamicArrayTest, RemoveElement)
{
    Dia::Core::Containers::DynamicArray<int> array;
    array.Add(10);
    array.RemoveAt(0);
    EXPECT_EQ(array.Size(), 0);
}
```

---

## Testing Strategies

### Test Structure: Arrange-Act-Assert

**Pattern:**
1. **Arrange** - Set up test data
2. **Act** - Execute the code under test
3. **Assert** - Verify the result

**Example:**
```cpp
void TestVectorAddition()
{
    // Arrange
    Dia::Maths::Vector2D a(1.0f, 2.0f);
    Dia::Maths::Vector2D b(3.0f, 4.0f);
    
    // Act
    Dia::Maths::Vector2D result = a + b;
    
    // Assert
    DIA_ASSERT(result.x == 4.0f, "x should be 4");
    DIA_ASSERT(result.y == 6.0f, "y should be 6");
}
```

---

### Edge Cases

**Test boundary conditions:**
- Empty collections
- Single element
- Maximum size
- Null pointers
- Zero values
- Negative values

**Example:**
```cpp
void TestDynamicArrayEdgeCases()
{
    Dia::Core::Containers::DynamicArray<int> array;
    
    // Empty array
    DIA_ASSERT(array.Size() == 0, "Empty array size");
    DIA_ASSERT(array.IsEmpty(), "Empty array isEmpty");
    
    // Single element
    array.Add(42);
    DIA_ASSERT(array.Size() == 1, "Single element size");
    DIA_ASSERT(!array.IsEmpty(), "Single element not empty");
    
    // Clear
    array.Clear();
    DIA_ASSERT(array.Size() == 0, "Cleared array size");
    
    // Large array
    for (int i = 0; i < 10000; ++i)
    {
        array.Add(i);
    }
    DIA_ASSERT(array.Size() == 10000, "Large array size");
}
```

---

### Error Handling

**Test failure paths:**
- Invalid input
- Out-of-bounds access
- Null pointer checks
- Assertions fire correctly

**Example:**
```cpp
void TestVectorNormalizationZeroVector()
{
    Dia::Maths::Vector2D zero(0.0f, 0.0f);
    
    // Normalizing zero vector should assert or return zero
    // (Check implementation behavior)
    
    #ifdef DEBUG
    // Expect assertion in Debug builds
    // (Can't directly test assertion without test framework)
    #else
    // In Release, may return zero or undefined
    Dia::Maths::Vector2D result = zero.Normalize();
    // Document expected behavior
    #endif
}
```

---

## Unit Test Examples by Subsystem

### DiaCore: Containers

#### DynamicArray Tests

```cpp
void TestDynamicArrayOperations()
{
    using namespace Dia::Core::Containers;
    DynamicArray<int> array;
    
    // Add
    array.Add(1);
    array.Add(2);
    array.Add(3);
    DIA_ASSERT(array.Size() == 3, "Size after adds");
    
    // Access
    DIA_ASSERT(array[0] == 1, "First element");
    DIA_ASSERT(array[1] == 2, "Second element");
    DIA_ASSERT(array[2] == 3, "Third element");
    
    // Remove
    array.RemoveAt(1);  // Remove middle element
    DIA_ASSERT(array.Size() == 2, "Size after remove");
    DIA_ASSERT(array[0] == 1, "First element after remove");
    DIA_ASSERT(array[1] == 3, "Second element after remove");
    
    // Contains
    DIA_ASSERT(array.Contains(1), "Contains 1");
    DIA_ASSERT(array.Contains(3), "Contains 3");
    DIA_ASSERT(!array.Contains(2), "Does not contain 2");
    
    // Clear
    array.Clear();
    DIA_ASSERT(array.Size() == 0, "Size after clear");
    DIA_ASSERT(array.IsEmpty(), "Empty after clear");
}
```

#### HashTable Tests

```cpp
void TestHashTableOperations()
{
    using namespace Dia::Core::Containers;
    HashTable<StringCRC, int> table;
    
    // Insert
    StringCRC key1 = StringCRC("key1");
    StringCRC key2 = StringCRC("key2");
    
    table.Insert(key1, 100);
    table.Insert(key2, 200);
    DIA_ASSERT(table.Size() == 2, "Size after inserts");
    
    // Find
    int* value1 = table.Find(key1);
    DIA_ASSERT(value1 != nullptr, "Found key1");
    DIA_ASSERT(*value1 == 100, "Value of key1");
    
    int* value2 = table.Find(key2);
    DIA_ASSERT(value2 != nullptr, "Found key2");
    DIA_ASSERT(*value2 == 200, "Value of key2");
    
    // Not found
    StringCRC keyMissing = StringCRC("missing");
    int* valueMissing = table.Find(keyMissing);
    DIA_ASSERT(valueMissing == nullptr, "Missing key returns null");
    
    // Remove
    table.Remove(key1);
    DIA_ASSERT(table.Size() == 1, "Size after remove");
    DIA_ASSERT(table.Find(key1) == nullptr, "Key1 removed");
}
```

---

### DiaMaths: Vectors

#### Vector2D Tests

```cpp
void TestVector2DOperations()
{
    using namespace Dia::Maths;
    
    // Arithmetic
    Vector2D a(1.0f, 2.0f);
    Vector2D b(3.0f, 4.0f);
    
    Vector2D sum = a + b;
    DIA_ASSERT(sum.x == 4.0f && sum.y == 6.0f, "Addition");
    
    Vector2D diff = a - b;
    DIA_ASSERT(diff.x == -2.0f && diff.y == -2.0f, "Subtraction");
    
    Vector2D scaled = a * 2.0f;
    DIA_ASSERT(scaled.x == 2.0f && scaled.y == 4.0f, "Scalar multiply");
    
    // Magnitude
    Vector2D v(3.0f, 4.0f);
    float mag = v.Magnitude();
    DIA_ASSERT(Dia::Maths::FloatEquals(mag, 5.0f), "Magnitude");  // 3-4-5 triangle
    
    float magSq = v.MagnitudeSquared();
    DIA_ASSERT(magSq == 25.0f, "Magnitude squared");
    
    // Normalize
    Vector2D normalized = v.Normalize();
    float normalizedMag = normalized.Magnitude();
    DIA_ASSERT(Dia::Maths::FloatEquals(normalizedMag, 1.0f), "Normalized magnitude");
    
    // Dot product
    Vector2D x(1.0f, 0.0f);
    Vector2D y(0.0f, 1.0f);
    float dot = Dia::Maths::Dot(x, y);
    DIA_ASSERT(dot == 0.0f, "Perpendicular dot product");
    
    float dotSame = Dia::Maths::Dot(x, x);
    DIA_ASSERT(dotSame == 1.0f, "Parallel dot product");
}
```

---

### DiaMaths: Matrices

#### Matrix33 Tests

```cpp
void TestMatrix33Operations()
{
    using namespace Dia::Maths;
    
    // Identity
    Matrix33 identity = Matrix33::Identity();
    Vector2D v(5.0f, 10.0f);
    Vector2D transformed = identity * v;
    DIA_ASSERT(transformed.x == v.x && transformed.y == v.y, "Identity transform");
    
    // Translation
    Matrix33 translation = Matrix33::Translation(10.0f, 20.0f);
    Vector2D point(0.0f, 0.0f);
    Vector2D translated = translation * point;
    DIA_ASSERT(translated.x == 10.0f && translated.y == 20.0f, "Translation");
    
    // Scale
    Matrix33 scale = Matrix33::Scale(2.0f, 3.0f);
    Vector2D original(5.0f, 10.0f);
    Vector2D scaled = scale * original;
    DIA_ASSERT(scaled.x == 10.0f && scaled.y == 30.0f, "Scale");
    
    // Matrix multiplication
    Matrix33 combined = translation * scale;
    Vector2D combinedResult = combined * Vector2D(1.0f, 1.0f);
    // Scale first (1*2, 1*3) = (2, 3), then translate (2+10, 3+20) = (12, 23)
    DIA_ASSERT(combinedResult.x == 12.0f && combinedResult.y == 23.0f, "Combined transform");
}
```

---

### DiaApplicationFlow: Module System

#### Module Dependency Tests

```cpp
void TestModuleDependencies()
{
    using namespace Dia::Application;
    
    // Create modules
    Module* moduleA = new TestModuleA();  // No dependencies
    Module* moduleB = new TestModuleB();  // Depends on A
    Module* moduleC = new TestModuleC();  // Depends on B
    
    // Create phase
    Phase* phase = new TestPhase();
    phase->AddModule(moduleA);
    phase->AddModule(moduleB);
    phase->AddModule(moduleC);
    
    // Declare dependencies
    moduleB->AddDependency(moduleA->GetId());
    moduleC->AddDependency(moduleB->GetId());
    
    // Resolve (should order: A, B, C)
    phase->ResolveDependencies();
    
    // Verify order
    const auto& modules = phase->GetModules();
    DIA_ASSERT(modules[0] == moduleA, "Module A first");
    DIA_ASSERT(modules[1] == moduleB, "Module B second");
    DIA_ASSERT(modules[2] == moduleC, "Module C third");
    
    // Cleanup
    delete phase;
}
```

---

## Testing Patterns

### Test Fixtures (Future: Google Test)

**Setup/Teardown pattern for repeated test setup:**

```cpp
class DynamicArrayTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Run before each test
        array = new Dia::Core::Containers::DynamicArray<int>();
    }
    
    void TearDown() override
    {
        // Run after each test
        delete array;
        array = nullptr;
    }
    
    Dia::Core::Containers::DynamicArray<int>* array;
};

TEST_F(DynamicArrayTest, AddElement)
{
    array->Add(10);
    EXPECT_EQ(array->Size(), 1);
}

TEST_F(DynamicArrayTest, RemoveElement)
{
    array->Add(10);
    array->RemoveAt(0);
    EXPECT_EQ(array->Size(), 0);
}
```

---

### Parameterized Tests (Future: Google Test)

**Test multiple inputs with same logic:**

```cpp
class VectorAdditionTest : public ::testing::TestWithParam<std::tuple<float, float, float, float, float, float>>
{
};

TEST_P(VectorAdditionTest, Addition)
{
    auto [ax, ay, bx, by, expectedX, expectedY] = GetParam();
    
    Dia::Maths::Vector2D a(ax, ay);
    Dia::Maths::Vector2D b(bx, by);
    Dia::Maths::Vector2D result = a + b;
    
    EXPECT_FLOAT_EQ(result.x, expectedX);
    EXPECT_FLOAT_EQ(result.y, expectedY);
}

INSTANTIATE_TEST_SUITE_P(
    VectorTests,
    VectorAdditionTest,
    ::testing::Values(
        std::make_tuple(1.0f, 2.0f, 3.0f, 4.0f, 4.0f, 6.0f),
        std::make_tuple(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
        std::make_tuple(-1.0f, -2.0f, 1.0f, 2.0f, 0.0f, 0.0f)
    )
);
```

---

## Mocking

### When to Mock

- External dependencies (file I/O, network)
- Slow operations (database, rendering)
- Non-deterministic behavior (time, random)

### Manual Mocking

**Mock canvas for testing without rendering:**

```cpp
class MockCanvas : public Dia::Graphics::ICanvas
{
public:
    void DrawLine(const Vector2D& start, const Vector2D& end, const Color& color) override
    {
        // Record call for verification
        mDrawLineCalls++;
    }
    
    void DrawCircle(const Vector2D& center, float radius, const Color& color) override
    {
        mDrawCircleCalls++;
    }
    
    // ... other methods
    
    int mDrawLineCalls = 0;
    int mDrawCircleCalls = 0;
};

void TestRenderer()
{
    MockCanvas canvas;
    Renderer renderer(&canvas);
    
    renderer.DrawBoundingBox(AABB(Vector2D(0, 0), Vector2D(10, 10)));
    
    // Verify canvas called correctly
    DIA_ASSERT(canvas.mDrawLineCalls == 4, "Drew 4 lines for box");
}
```

---

## Best Practices

### Do's ✅

- **Test one thing** - Each test should verify one behavior
- **Descriptive names** - TestDynamicArrayAddElement (clear what's tested)
- **Fast tests** - Unit tests should be < 1ms
- **Independent tests** - Tests should not depend on each other
- **Readable tests** - Tests are documentation

### Don'ts ❌

- **Don't test implementation** - Test behavior, not how it's done
- **Don't test trivial code** - No need to test `GetX() { return mX; }`
- **Don't test third-party** - Trust SFML, JsonCpp
- **Don't mock everything** - Only mock what you need
- **Don't duplicate logic** - Test shouldn't reimplement the code

---

## Summary

**Unit Testing Approach:**
- Test individual components in isolation
- Fast execution (< 1ms per test)
- Arrange-Act-Assert structure
- Test edge cases and error handling

**Current Infrastructure:**
- In-engine tests (UnitTestLevel)
- DIA_ASSERT for validation
- Manual test execution

**Future Infrastructure:**
- Google Test integration
- Test fixtures for setup/teardown
- Parameterized tests
- Mock framework

**Coverage Goals:**
- DiaCore: 70%+ (containers, type system)
- DiaMaths: 70%+ (vectors, matrices, shapes)
- DiaApplicationFlow: 60%+ (modules, phases)

**[→ Testing Strategy](test.md)**  
**[→ Integration Testing](integration-testing.md)**  
**[→ Test Coverage Targets](test-coverage-targets.md)**
