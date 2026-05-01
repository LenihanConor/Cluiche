# API Conventions

**Last Updated:** 2026-04-01

API design conventions and coding standards for Cluiche and Dia engine.

---

## Naming Conventions

### Classes and Structs

**Pattern:** PascalCase

```cpp
class ProcessingUnit { };
class DynamicArray { };
struct Vector2D { };
```

**Interfaces:** `I` prefix

```cpp
class ICanvas { };
class IWindow { };
class ILevel { };
```

---

### Functions and Methods

**Pattern:** PascalCase

```cpp
void Update();
int GetValue() const;
void SetValue(int value);
```

**Boolean Queries:** `Is` or `Has` prefix

```cpp
bool IsReady() const;
bool HasData() const;
bool CanProceed() const;
```

---

### Member Variables

**Pattern:** `m` prefix + camelCase

```cpp
class MyClass {
private:
    int mValue;
    std::string mName;
    bool mInitialized;
};
```

---

### Constants

**Pattern:** `k` prefix + PascalCase

```cpp
static const int kMaxSize = 100;
static const float kGravity = 9.8f;
static const StringCRC kUniqueId = StringCRC("MyModule");
```

---

### Static Members

**Pattern:** `s` prefix + PascalCase

```cpp
class Singleton {
private:
    static Singleton* sInstance;
};
```

---

### Template Parameters

**Pattern:** Single uppercase letter or PascalCase

```cpp
template<typename T>
class Array { };

template<class Key, class Value>
class HashTable { };

template<typename TComponent>
class Factory { };
```

---

### Namespaces

**Pattern:** PascalCase, hierarchical

```cpp
namespace Dia {
    namespace Core {
        class DynamicArray { };
    }
    
    namespace Maths {
        class Vector2D { };
    }
}
```

**Usage:**
```cpp
Dia::Core::DynamicArray<int> arr;
Dia::Maths::Vector2D vec;
```

---

### Macros

**Pattern:** ALL_CAPS with underscores

```cpp
#define DIA_ASSERT(cond, msg) ...
#define DIA_LOG(fmt, ...) ...
```

---

### Files

**Pattern:** Match class name

```cpp
// Header
Vector2D.h

// Source
Vector2D.cpp
```

**Module Architecture Files:**
```
dia.parent.module.architecture.module.md
```

---

## Code Style

### Indentation

**Use tabs, not spaces**

```cpp
class MyClass
{
public:
	void MyMethod()
	{
		if (condition)
		{
			DoSomething();
		}
	}
};
```

---

### Braces

**Opening brace on new line**

```cpp
// ✅ Good
void MyFunction()
{
	// Body
}

// ❌ Bad
void MyFunction() {
	// Body
}
```

---

### Whitespace

**One blank line between functions**

```cpp
void Function1()
{
	// Body
}

void Function2()
{
	// Body
}
```

**No trailing whitespace**

---

### Line Length

**Soft limit:** 100 characters
**Hard limit:** 120 characters

**Break long lines:**
```cpp
void FunctionWithManyParameters(
	int param1,
	float param2,
	const std::string& param3,
	bool param4)
{
	// Body
}
```

---

## API Design Patterns

### Const Correctness

**Mark read-only methods const:**

```cpp
class Vector2D
{
public:
	float Magnitude() const;      // Doesn't modify
	void Normalize();             // Modifies
	
	float GetX() const { return x; }
	void SetX(float value) { x = value; }
	
private:
	float x, y;
};
```

---

### Return by Value vs Reference

**Small types:** Return by value

```cpp
int GetValue() const { return mValue; }
float GetLength() const { return mLength; }
```

**Large types:** Return by const reference

```cpp
const std::string& GetName() const { return mName; }
const Vector3D& GetPosition() const { return mPosition; }
```

---

### Parameter Passing

**Input parameters:**
- Small types (int, float, pointer): By value
- Large types: By const reference

```cpp
void SetValue(int value);                      // Small, by value
void SetName(const std::string& name);         // Large, by const ref
void ProcessData(const Vector3D& position);    // Large, by const ref
```

**Output parameters:**
- By pointer or non-const reference

```cpp
bool TryGetValue(int* outValue);              // Output via pointer
void GetBounds(Vector2D& outMin, Vector2D& outMax);  // Output via ref
```

---

### Ownership and Lifetime

**Raw pointers:** Non-owning

```cpp
void ProcessModule(Module* module);  // Doesn't own, doesn't delete
```

**Returning new objects:** Document ownership

```cpp
// Caller owns returned object, must delete
ILevel* LevelFactory::Create(const char* name);
```

**Smart pointers:** Use sparingly

```cpp
// Only when ownership semantics complex
std::unique_ptr<Data> CreateData();
```

---

## Error Handling

### Assertions

**Use for preconditions:**

```cpp
void ProcessData(Data* data)
{
	DIA_ASSERT(data != nullptr, "Data must not be null");
	DIA_ASSERT(data->IsValid(), "Data must be valid");
	// Process data
}
```

---

### Return Values

**Prefer return values over exceptions:**

```cpp
// ✅ Good: Return bool for success/failure
bool TryParse(const char* str, int* outValue);

// ❌ Bad: Exceptions (avoided in this codebase)
int Parse(const char* str);  // May throw
```

---

### Null Checks

**Check inputs:**

```cpp
void SafeFunction(Object* obj)
{
	if (obj == nullptr)
	{
		DIA_LOG("Warning: null object");
		return;
	}
	
	obj->DoSomething();
}
```

---

## Thread Safety

### Document Thread Requirements

```cpp
/**
 * @brief Updates module state
 * @warning Must be called from Main thread only
 */
void MainUIModule::Update();
```

---

### Use const for Thread-Safe Read-Only

```cpp
class Config
{
public:
	// Thread-safe if Config immutable
	int GetMaxPlayers() const { return mMaxPlayers; }
	
private:
	const int mMaxPlayers;
};
```

---

### Protect Mutable State

```cpp
class SharedCounter
{
public:
	void Increment()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mCount++;
	}
	
	int GetCount() const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mCount;
	}
	
private:
	int mCount;
	mutable std::mutex mMutex;  // mutable for const methods
};
```

---

## Documentation

### Header Comments

```cpp
/**
 * @brief One-line description of class
 *
 * Detailed multi-line description of what this class does,
 * why it exists, and how to use it.
 *
 * Example:
 * @code
 * MyClass obj;
 * obj.DoSomething();
 * @endcode
 */
class MyClass
{
	// ...
};
```

---

### Function Comments

```cpp
/**
 * @brief One-line description
 *
 * Detailed description (optional if obvious).
 *
 * @param param1 Description of parameter
 * @param param2 Description of parameter
 * @return Description of return value
 *
 * @note Important notes about usage
 * @warning Thread safety or other warnings
 */
int MyFunction(int param1, float param2);
```

---

### Inline Comments

**Use sparingly, prefer self-documenting code:**

```cpp
// ✅ Good: Code is self-documenting
int playerCount = GetActivePlayers();

// ❌ Bad: Comment states the obvious
int playerCount = GetActivePlayers();  // Get the player count
```

**Use for non-obvious logic:**

```cpp
// Normalize vector only if non-zero to avoid division by zero
if (length > 0.0f)
{
	Normalize();
}
```

---

## Common Anti-Patterns to Avoid

### Hungarian Notation

```cpp
// ❌ Bad: Hungarian notation
int iCount;
float fValue;
bool bFlag;

// ✅ Good: Descriptive names
int count;
float value;
bool isReady;
```

---

### Overly Abbreviated Names

```cpp
// ❌ Bad: Hard to understand
void ProcDat(Dat* d);

// ✅ Good: Clear and readable
void ProcessData(Data* data);
```

---

### Magic Numbers

```cpp
// ❌ Bad: Magic number
if (playerCount > 4) { }

// ✅ Good: Named constant
const int kMaxPlayers = 4;
if (playerCount > kMaxPlayers) { }
```

---

### Stringly-Typed

```cpp
// ❌ Bad: String-based type system
Module* GetModule(const char* name);

// ✅ Good: Type-safe
template<typename T>
T* GetDependency();
```

---

## Type Aliases

**Use sparingly, prefer explicit types:**

```cpp
// ✅ Good: Descriptive alias for complex type
using ModuleRegistry = HashTable<StringCRC, Module*>;

// ❌ Bad: Unnecessary alias
using Int = int;  // Don't do this
```

---

## Forward Declarations

**Use to reduce dependencies:**

```cpp
// Header: MyClass.h
class OtherClass;  // Forward declaration

class MyClass
{
	void DoSomething(OtherClass* other);
};
```

**Include in source:**

```cpp
// Source: MyClass.cpp
#include "OtherClass.h"  // Full include in .cpp
```

---

## Include Order

**Standard order:**

1. Corresponding header (for .cpp files)
2. C system headers
3. C++ standard library headers
4. External library headers
5. Project headers (Dia)
6. Local headers

```cpp
// MyClass.cpp
#include "MyClass.h"              // 1. Corresponding header

#include <cstdio>                 // 2. C system
#include <cmath>

#include <string>                 // 3. C++ standard
#include <vector>

#include <SFML/Graphics.hpp>      // 4. External

#include "DiaCore/Core/Assert.h"  // 5. Project
#include "DiaMaths/Vector2D.h"

#include "LocalHelper.h"          // 6. Local
```

---

## Header Guards

**Use `#pragma once`:**

```cpp
#pragma once

// Header content
```

---

## Summary

**Naming:**
- Classes: `PascalCase`
- Functions: `PascalCase`
- Members: `mCamelCase`
- Constants: `kPascalCase`
- Interfaces: `IPascalCase`

**Style:**
- Tabs for indentation
- Braces on new line
- 100-120 character line limit
- Const correctness
- Descriptive names

**Error Handling:**
- Assertions for preconditions
- Return values for errors
- Avoid exceptions

**Thread Safety:**
- Document thread requirements
- Use `mutable std::mutex` for const methods
- Prefer immutable data

**Documentation:**
- Brief comments for public API
- Self-documenting code over inline comments
- Example code in headers

**Avoid:**
- Hungarian notation
- Magic numbers
- Stringly-typed APIs
- Overly abbreviated names

**[→ API Overview](api-overview.md)**  
**[→ Back to Documentation Index](../README.md)**
