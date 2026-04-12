# Coding Standards

**Last Updated:** 2026-04-01

Code style and best practices for the Cluiche project.

---

## Overview

This document defines coding standards for the Cluiche codebase. Consistency improves readability and maintainability.

**Key Principles:**
- **Clarity over cleverness** - Write code that's easy to understand
- **Consistency** - Follow existing patterns
- **Simplicity** - Avoid unnecessary complexity
- **Explicit over implicit** - Make intentions clear

---

## Formatting

### Indentation

**Use tabs (not spaces)**

```cpp
// ✅ Good: Tabs
void Function()
{
	int value = 10;
	if (value > 5)
	{
		DoSomething();
	}
}

// ❌ Bad: Spaces
void Function()
{
    int value = 10;
    if (value > 5)
    {
        DoSomething();
    }
}
```

**Rationale:** Tabs allow developers to configure their preferred indentation width.

---

### Braces

**Braces on new line (Allman style)**

```cpp
// ✅ Good: Braces on new line
if (condition)
{
	DoSomething();
}

void Function()
{
	// Body
}

class MyClass
{
public:
	void Method();
};

// ❌ Bad: Braces on same line (K&R style)
if (condition) {
	DoSomething();
}
```

**Exception:** Single-line blocks may omit braces (but be careful):

```cpp
// ✅ Acceptable
if (condition)
	DoSomething();

// ✅ Better: Always use braces for clarity
if (condition)
{
	DoSomething();
}
```

---

### Line Length

**Prefer 100-120 characters per line**

```cpp
// ✅ Good: Fits on one line
void SetPosition(const Dia::Maths::Vector2D& position);

// ✅ Good: Break long lines
void ProcessInputEvent(
	const Dia::Input::InputEvent& event,
	Dia::Application::FrameStream<Dia::Input::InputEvent>* frameStream);

// ❌ Bad: Too long
void ProcessInputEventAndForwardToSimThreadViaFrameStreamWhileLoggingDebugInformation(const Dia::Input::InputEvent& event, Dia::Application::FrameStream<Dia::Input::InputEvent>* frameStream);
```

---

### Whitespace

**Use whitespace for readability**

```cpp
// ✅ Good: Readable spacing
int a = 10;
int b = 20;
int sum = a + b;

if (sum > 0)
{
	DoSomething();
}

// ❌ Bad: Too cramped
int a=10;
int b=20;
int sum=a+b;
if(sum>0){DoSomething();}
```

**Blank lines:**
- One blank line between functions
- One blank line between logical sections within a function

---

## Naming Conventions

### Classes and Structs

**PascalCase**

```cpp
// ✅ Good
class ProcessingUnit { };
class DynamicArray { };
struct InputEvent { };

// ❌ Bad
class processing_unit { };  // snake_case
class dynamicArray { };     // camelCase
```

---

### Functions and Methods

**PascalCase**

```cpp
// ✅ Good
void DoSomething();
int GetValue() const;
void SetPosition(const Vector2D& pos);

// ❌ Bad
void doSomething();         // camelCase
void do_something();        // snake_case
```

---

### Variables

**Local variables and parameters: camelCase**

```cpp
// ✅ Good
void Update(float deltaTime)
{
	int playerHealth = 100;
	Vector2D playerPosition;
}

// ❌ Bad
void Update(float DeltaTime)          // PascalCase
{
	int PlayerHealth = 100;           // PascalCase
	Vector2D player_position;         // snake_case
}
```

---

### Member Variables

**mCamelCase (m prefix for "member")**

```cpp
// ✅ Good
class Player
{
private:
	int mHealth;
	Vector2D mPosition;
	float mSpeed;
};

// ❌ Bad
class Player
{
private:
	int health;           // No prefix
	int _health;          // Underscore prefix
	int m_health;         // Underscore separator
};
```

---

### Constants

**kPascalCase (k prefix for "constant")**

```cpp
// ✅ Good
const int kMaxPlayers = 4;
const float kPi = 3.14159f;
const StringCRC kModuleId = StringCRC("PhysicsModule");

static const int kBufferSize = 1024;

// ❌ Bad
const int MAX_PLAYERS = 4;        // SCREAMING_SNAKE_CASE
const int maxPlayers = 4;         // camelCase (looks like variable)
```

---

### Interfaces

**IClassName (I prefix for "interface")**

```cpp
// ✅ Good
class ICanvas { };
class IWindow { };
class IInputSource { };

// ❌ Bad
class Canvas { };           // No prefix (ambiguous)
class CanvasInterface { };  // Suffix instead of prefix
```

---

### Enums

**PascalCase for enum name, PascalCase for values**

```cpp
// ✅ Good
enum class InputEventType
{
	KeyPressed,
	KeyReleased,
	MouseMoved
};

// ❌ Bad
enum class InputEventType
{
	KEY_PRESSED,        // SCREAMING_SNAKE_CASE
	keyReleased,        // camelCase
	mouse_moved         // snake_case
};
```

---

### Namespaces

**PascalCase, nested namespaces**

```cpp
// ✅ Good
namespace Dia
{
	namespace Core
	{
		class DynamicArray { };
	}
}

// Usage
Dia::Core::DynamicArray<int> array;

// ❌ Bad
namespace dia_core  // snake_case
{
	class DynamicArray { };
}
```

---

## Comments

### File Headers

**Minimal file headers (no copyright boilerplate)**

```cpp
// ✅ Good: Simple pragma once
#pragma once

#include "DiaCore/Core/Types.h"

namespace Dia
{
	// ...
}

// ❌ Bad: Unnecessary boilerplate
/**
 * File: MyClass.h
 * Author: John Doe
 * Date: 2023-01-01
 * Copyright (c) 2023 Company Name
 * Description: This file contains the MyClass class
 */
```

---

### Class Comments

**Document public interfaces, not obvious code**

```cpp
// ✅ Good: Document purpose and usage
/**
 * ProcessingUnit is a high-level execution container that owns modules and phases.
 * Can run on a separate thread. Manages module lifecycle and phase transitions.
 */
class ProcessingUnit
{
public:
	/**
	 * Add a module to this processing unit.
	 * Module will be constructed during PU construction.
	 * 
	 * @param module Module to add (takes ownership)
	 */
	void AddModule(Module* module);
};

// ❌ Bad: Obvious comments
/**
 * This is a class
 */
class MyClass
{
public:
	/**
	 * This function adds two numbers
	 * @param a First number
	 * @param b Second number
	 * @return Sum of a and b
	 */
	int Add(int a, int b) { return a + b; }  // Obvious
};
```

---

### Inline Comments

**Explain "why", not "what"**

```cpp
// ✅ Good: Explain rationale
// Cache world matrix because GetWorldMatrix() is slow (no caching, traverses hierarchy)
Matrix33 cachedWorld = transform.GetWorldMatrix();

// ⚠️ Acceptable: Clarify non-obvious behavior
// VSync blocks until next frame (may take 16ms at 60 Hz)
window->Display();

// ❌ Bad: Obvious
// Increment i
i++;

// Set position to 10
position = 10;
```

---

### TODO Comments

**Format: TODO(name): description**

```cpp
// ✅ Good
// TODO(conor): Implement physics collision response
// TODO(team): Optimize this for large arrays (> 1000 elements)

// ❌ Bad
// TODO fix this
// FIXME
```

---

## Code Organization

### Header Files

**Header guard: #pragma once**

```cpp
// ✅ Good
#pragma once

#include "DiaCore/Core/Types.h"

namespace Dia
{
	class MyClass { };
}

// ❌ Bad: Old-style include guards
#ifndef DIA_MYCLASS_H
#define DIA_MYCLASS_H

// ...

#endif  // DIA_MYCLASS_H
```

---

### Include Order

**Order:**
1. Corresponding header (if .cpp file)
2. Project headers (Dia)
3. External dependencies (SFML, etc.)
4. Standard library

**Separate groups with blank line**

```cpp
// MyClass.cpp
#include "MyClass.h"  // Corresponding header first

#include "DiaCore/Containers/Array.h"
#include "DiaApplication/Module/Module.h"

#include <SFML/Graphics.hpp>

#include <vector>
#include <string>
```

---

### Forward Declarations

**Use forward declarations to minimize dependencies**

```cpp
// ✅ Good: Forward declare when possible
// MyClass.h
#pragma once

namespace Dia
{
	class OtherClass;  // Forward declaration
	
	class MyClass
	{
	private:
		OtherClass* mOther;  // Pointer, forward decl sufficient
	};
}

// ❌ Bad: Unnecessary include
#include "OtherClass.h"  // Full include not needed for pointer
```

---

## Best Practices

### Const Correctness

**Mark methods const when they don't modify state**

```cpp
// ✅ Good
class Vector2D
{
public:
	float Magnitude() const;  // Doesn't modify, marked const
	void Normalize();         // Modifies, not const
};

// ❌ Bad
class Vector2D
{
public:
	float Magnitude();  // Doesn't modify, but not const
};
```

---

### Pass by Const Reference

**For large types, pass by const reference**

```cpp
// ✅ Good
void SetPosition(const Vector2D& position);
void ProcessMatrix(const Matrix44& matrix);

// ❌ Bad: Unnecessary copy
void SetPosition(Vector2D position);  // Copies Vector2D
void ProcessMatrix(Matrix44 matrix);  // Copies Matrix44
```

**Exception:** Small types (int, float, pointers) pass by value

```cpp
// ✅ Good
void SetHealth(int health);
void SetSpeed(float speed);
void SetModule(Module* module);
```

---

### Return by Value

**Prefer return by value (NRVO)**

```cpp
// ✅ Good: Return by value (compiler optimizes)
Vector2D GetPosition() const
{
	return mPosition;
}

// ❌ Bad: Return by const reference (dangling if temporary)
const Vector2D& GetPosition() const
{
	Vector2D temp = mPosition;
	return temp;  // DANGLING REFERENCE!
}
```

---

### Nullptr

**Use nullptr, not NULL or 0**

```cpp
// ✅ Good
Module* module = nullptr;
if (module == nullptr)
{
	// ...
}

// ❌ Bad
Module* module = NULL;  // Old C macro
Module* module = 0;     // Ambiguous
```

---

### Auto

**Use auto for obvious types (iterators, lambdas)**

```cpp
// ✅ Good: Auto for iterators
auto it = myMap.find(key);
for (auto& element : myArray)
{
	// ...
}

// ✅ Good: Auto for lambdas
auto callback = [](int x) { return x * 2; };

// ⚠️ Acceptable: Explicit type for clarity
std::map<StringCRC, Module*>::iterator it = myMap.find(key);

// ❌ Bad: Auto obscures type
auto x = GetValue();  // What type is x?
```

---

### Assertions

**Use DIA_ASSERT for debug checks**

```cpp
// ✅ Good
DIA_ASSERT(index < mSize, "Index out of bounds");
DIA_ASSERT(module != nullptr, "Module is null");

// ❌ Bad: Silent failure
if (index >= mSize)
{
	return;  // No indication of error
}
```

---

### Magic Numbers

**Avoid magic numbers, use named constants**

```cpp
// ✅ Good
const int kMaxPlayers = 4;
const float kGravity = 9.8f;

if (playerCount > kMaxPlayers)
{
	// ...
}

// ❌ Bad
if (playerCount > 4)  // What is 4?
{
	// ...
}
```

---

### RAII

**Use RAII for resource management**

```cpp
// ✅ Good: RAII with destructor
class Texture
{
public:
	Texture() { mData = LoadTexture(); }
	~Texture() { UnloadTexture(mData); }
	
private:
	void* mData;
};

// ❌ Bad: Manual cleanup (easy to forget)
void* texture = LoadTexture();
// ... use texture ...
UnloadTexture(texture);  // Must remember to call
```

---

## Anti-Patterns

### Hungarian Notation

**Don't use Hungarian notation**

```cpp
// ❌ Bad
int iCount;
float fSpeed;
bool bIsActive;
char* szName;

// ✅ Good
int count;
float speed;
bool isActive;
char* name;
```

---

### Stringly Typed Code

**Prefer StringCRC over raw strings for IDs**

```cpp
// ✅ Good: StringCRC (compile-time hash, fast comparison)
const StringCRC kModuleId = StringCRC("PhysicsModule");

if (id == kModuleId)  // O(1) integer comparison
{
	// ...
}

// ❌ Bad: String comparison (slow, error-prone)
const char* kModuleName = "PhysicsModule";

if (strcmp(name, kModuleName) == 0)  // O(n) string comparison
{
	// ...
}
```

---

### Deep Nesting

**Avoid deep nesting (prefer early return)**

```cpp
// ✅ Good: Early return
void Process(Module* module)
{
	if (module == nullptr)
	{
		return;
	}
	
	if (!module->IsReady())
	{
		return;
	}
	
	module->Update();
}

// ❌ Bad: Deep nesting
void Process(Module* module)
{
	if (module != nullptr)
	{
		if (module->IsReady())
		{
			module->Update();
		}
	}
}
```

---

## Summary

**Formatting:**
- Tabs for indentation
- Braces on new line (Allman style)
- 100-120 char line length

**Naming:**
- PascalCase: classes, functions
- camelCase: variables, parameters
- mCamelCase: members
- kPascalCase: constants
- IClassName: interfaces

**Comments:**
- Document "why", not "what"
- Minimal file headers
- TODO(name): description

**Best Practices:**
- Const correctness
- Pass by const reference
- Use nullptr
- Use DIA_ASSERT
- Avoid magic numbers
- RAII

**Anti-Patterns:**
- No Hungarian notation
- Prefer StringCRC over strings
- Avoid deep nesting

**[→ Contributing](contributing.md)**  
**[→ API Conventions](../reference/api/conventions.md)**
