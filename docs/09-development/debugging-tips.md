# Debugging Tips

**Last Updated:** 2026-04-01

Debugging strategies, tools, and common issues for the Cluiche project.

---

## Overview

This document provides practical debugging advice for common issues in the Cluiche codebase.

**Topics Covered:**
1. Visual Studio Debugging
2. Threading Issues
3. Memory Issues
4. Performance Problems
5. Common Crashes
6. Logging and Assertions

---

## Visual Studio Debugging

### Setting Up Debugger

**1. Set Startup Project:**
- Right-click `Cluiche` project in Solution Explorer
- Select "Set as StartUp Project"

**2. Build Configuration:**
- Select `Debug|Win32` from configuration dropdown
- Debug builds include symbols and assertions

**3. Start Debugging:**
- Press F5 (Start Debugging)
- Or Ctrl+F5 (Start Without Debugging - faster)

---

### Breakpoints

**Set Breakpoint:**
- Click in left margin next to line number
- Or press F9 on current line

**Conditional Breakpoint:**
- Right-click breakpoint → Conditions
- Example: `playerHealth < 0`

**Logpoint (Tracepoint):**
- Right-click breakpoint → Actions
- Example: `Health: {playerHealth}`

**Data Breakpoint:**
- Break when memory location changes
- Debug → New Breakpoint → Data Breakpoint
- Useful for tracking unexpected modifications

---

### Watch Window

**Add Variables to Watch:**
- Right-click variable → Add Watch
- Or type expression in Watch window

**Useful Expressions:**
```cpp
// Array contents
mArray.mData[0]@mArray.mSize  // Show first mSize elements

// Pointer dereference
*mPointer

// Member access
this->mPosition.x

// Conditional expression
mHealth > 0 ? mHealth : 0
```

---

### Call Stack

**View Call Stack:**
- Debug → Windows → Call Stack (or Ctrl+Alt+C)

**Navigate Stack:**
- Double-click frame to view code
- Switch between frames to see variables at each level

**Example:**
```
Cluiche.exe!SimPhysicsModule::OnUpdate()  Line 42
Cluiche.exe!Module::Update()  Line 18
Cluiche.exe!Phase::UpdateModules()  Line 67
Cluiche.exe!SimProcessingUnit::Run()  Line 102
```

---

### Autos and Locals Windows

**Autos Window:**
- Shows variables used in current line and previous line
- Debug → Windows → Autos

**Locals Window:**
- Shows all local variables in current scope
- Debug → Windows → Locals

---

## Threading Issues

### Identifying Thread Problems

**Symptoms:**
- Race conditions (non-deterministic behavior)
- Deadlocks (application hangs)
- Data corruption (incorrect values)

---

### Debugging Threads

**Threads Window:**
- Debug → Windows → Threads (or Ctrl+Alt+H)
- Shows all active threads
- Can switch between threads

**Thread Markers:**
```cpp
#include <thread>

void MyFunction()
{
	std::thread::id threadId = std::this_thread::get_id();
	DIA_LOG("Running on thread: %d", threadId);
}
```

---

### Common Thread Issues

**Issue 1: Accessing Shared State Without Mutex**

```cpp
// ❌ Bad: Race condition
class Counter
{
public:
	void Increment() { mValue++; }  // Not thread-safe!
	
private:
	int mValue;
};

// ✅ Good: Use mutex
class Counter
{
public:
	void Increment()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mValue++;
	}
	
private:
	std::mutex mMutex;
	int mValue;
};
```

---

**Issue 2: Deadlock**

```cpp
// ❌ Bad: Lock order inconsistency
Thread 1: Lock A → Lock B
Thread 2: Lock B → Lock A  // DEADLOCK!

// ✅ Good: Consistent lock order
Thread 1: Lock A → Lock B
Thread 2: Lock A → Lock B
```

**Debugging Deadlocks:**
- Pause debugger (Break All)
- Check Threads window for blocked threads
- Examine call stacks of each thread
- Look for mutex lock patterns

---

**Issue 3: Calling Non-Thread-Safe API**

```cpp
// ❌ Bad: Calling OpenGL from Sim thread
// Sim thread
mCanvas->DrawCircle(position, radius, color);  // ERROR! Must call from Render thread

// ✅ Good: Use FrameStream to forward to correct thread
// Sim thread
mRenderFrameStream.Write(RenderCommand::DrawCircle(position, radius, color));

// Render thread
RenderCommand cmd;
while (mRenderFrameStream.Read(cmd))
{
	ExecuteRenderCommand(cmd);
}
```

---

## Memory Issues

### Memory Leaks

**Detecting Leaks:**
- Visual Studio has built-in leak detection
- Or use external tools (Dr. Memory, Valgrind on WSL)

**Common Causes:**
- Forgetting to delete
- Missing destructor calls
- Circular references

**Example:**
```cpp
// ❌ Bad: Memory leak
void CreateModule()
{
	Module* module = new Module();
	// Never deleted!
}

// ✅ Good: Proper cleanup
void CreateModule()
{
	Module* module = new Module();
	// ... use module ...
	delete module;
}

// ✅ Better: RAII
void CreateModule()
{
	std::unique_ptr<Module> module = std::make_unique<Module>();
	// Automatically deleted when out of scope
}
```

---

### Buffer Overruns

**Detecting:**
- Enable runtime checks: /RTC1 in Debug builds (usually enabled by default)
- Use Address Sanitizer (Visual Studio 2019+)

**Common Cause:**
```cpp
// ❌ Bad: Buffer overrun
char buffer[10];
strcpy(buffer, "This string is too long!");  // Overrun!

// ✅ Good: Bounds checking
char buffer[100];
strncpy(buffer, "Safe string", sizeof(buffer) - 1);
buffer[sizeof(buffer) - 1] = '\0';  // Null-terminate
```

---

### Null Pointer Dereference

**Detecting:**
- Usually causes access violation crash
- Check call stack for where null was dereferenced

**Prevention:**
```cpp
// ✅ Good: Always check pointers
Module* module = GetModule(id);
if (module != nullptr)
{
	module->Update();
}

// ✅ Better: Use assertions
Module* module = GetModule(id);
DIA_ASSERT(module != nullptr, "Module not found");
module->Update();
```

---

### Dangling Pointers

**Common Cause:**
```cpp
// ❌ Bad: Dangling pointer
Module* module = new Module();
delete module;
module->Update();  // ERROR! Dangling pointer

// ✅ Good: Set to nullptr after delete
Module* module = new Module();
delete module;
module = nullptr;

// ✅ Better: Use smart pointers
std::unique_ptr<Module> module = std::make_unique<Module>();
// Can't use after destruction
```

---

## Performance Problems

### Profiling

**Visual Studio Profiler:**
- Debug → Performance Profiler
- CPU Usage, Memory Usage, GPU Usage

**External Tools:**
- VTune (Intel)
- Tracy Profiler (open source)
- Optick (open source)

---

### Common Performance Issues

**Issue 1: Calling Expensive Functions in Loop**

```cpp
// ❌ Bad: Recomputing world matrix every iteration
for (int i = 0; i < 1000; ++i)
{
	Vector2D transformed = transform.GetWorldMatrix() * points[i];
	// GetWorldMatrix() traverses hierarchy 1000 times!
}

// ✅ Good: Cache world matrix
Matrix33 worldMatrix = transform.GetWorldMatrix();
for (int i = 0; i < 1000; ++i)
{
	Vector2D transformed = worldMatrix * points[i];
}
```

---

**Issue 2: Unnecessary Allocations**

```cpp
// ❌ Bad: Allocating every frame
void Update()
{
	std::vector<Entity> entities;  // Allocated every frame
	entities.reserve(100);
	// ...
}

// ✅ Good: Reuse allocation
class System
{
public:
	void Update()
	{
		mEntities.clear();  // Reuse allocation
		// ...
	}
	
private:
	std::vector<Entity> mEntities;
};
```

---

**Issue 3: String Comparison in Hot Path**

```cpp
// ❌ Bad: String comparison
if (strcmp(name, "PhysicsModule") == 0)  // O(n)
{
	// ...
}

// ✅ Good: StringCRC comparison
const StringCRC kPhysicsModuleId = StringCRC("PhysicsModule");
if (id == kPhysicsModuleId)  // O(1)
{
	// ...
}
```

---

## Common Crashes

### Access Violation

**Typical Causes:**
- Null pointer dereference
- Dangling pointer
- Buffer overrun
- Stack overflow

**Debugging:**
1. Look at call stack
2. Check variable values in Watch window
3. Identify which pointer is null/invalid
4. Trace back to where pointer was set

---

### Stack Overflow

**Typical Causes:**
- Infinite recursion
- Large stack allocations

**Example:**
```cpp
// ❌ Bad: Infinite recursion
void Recurse(int n)
{
	Recurse(n + 1);  // No base case!
}

// ✅ Good: Base case
void Recurse(int n)
{
	if (n <= 0)
	{
		return;  // Base case
	}
	Recurse(n - 1);
}
```

---

### Assertion Failures

**When Assertions Fire:**
- Debug builds only (disabled in Release)
- DIA_ASSERT(condition, "message")

**Debugging:**
1. Read assertion message
2. Check call stack for context
3. Examine variables that failed condition
4. Trace back to why condition failed

**Example:**
```cpp
DIA_ASSERT(index < mSize, "Index out of bounds");
// Break here, check:
// - What is index?
// - What is mSize?
// - Why is index >= mSize?
```

---

## Logging and Assertions

### DIA_LOG

**Usage:**
```cpp
#include "DiaCore/Core/Log.h"

DIA_LOG("Player health: %d", health);
DIA_LOG("Position: (%.2f, %.2f)", pos.x, pos.y);
```

**Output:**
- Visual Studio Output window
- Or log file (if configured)

---

### DIA_ASSERT

**Usage:**
```cpp
#include "DiaCore/Core/Assert.h"

DIA_ASSERT(module != nullptr, "Module is null");
DIA_ASSERT(index < mSize, "Index %d out of bounds (size: %d)", index, mSize);
```

**Behavior:**
- Debug builds: Breaks into debugger
- Release builds: Compiled out (no overhead)

---

### Strategic Logging

**Log Important Events:**
```cpp
void Module::OnStart()
{
	DIA_LOG("Module %s starting", GetName());
	// ...
	DIA_LOG("Module %s started successfully", GetName());
}

void Level::Load()
{
	DIA_LOG("Loading level: %s", GetName());
	LoadAssets();
	DIA_LOG("Level %s loaded", GetName());
}
```

**Log Errors:**
```cpp
Module* module = GetModule(id);
if (module == nullptr)
{
	DIA_LOG("ERROR: Module not found: %u", id);
	return;
}
```

---

## Debugging Specific Issues

### Threading Race Condition

**Steps:**
1. Add thread ID logging:
   ```cpp
   DIA_LOG("Thread: %d, Value: %d", std::this_thread::get_id(), mValue);
   ```
2. Run multiple times to see if behavior changes
3. Identify shared state accessed from multiple threads
4. Add mutex protection or use FrameStream

---

### Transform Hierarchy Bug

**Symptoms:**
- Objects in wrong positions
- Transforms not updating

**Debugging:**
```cpp
// Log local and world transforms
DIA_LOG("Local: (%.2f, %.2f), World: (%.2f, %.2f)",
	transform.GetPosition().x, transform.GetPosition().y,
	transform.GetWorldPosition().x, transform.GetWorldPosition().y);

// Check parent
if (transform.GetParent() != nullptr)
{
	DIA_LOG("Parent: (%.2f, %.2f)",
		transform.GetParent()->GetPosition().x,
		transform.GetParent()->GetPosition().y);
}
```

---

### Physics Collision Bug

**Debugging:**
```cpp
// Visualize collision shapes
void DebugDrawCollisions()
{
	for (const Circle& circle : mCircles)
	{
		canvas->DrawCircle(circle.center, circle.radius, Color::Green);
	}
	
	for (const AABB& box : mBoxes)
	{
		DrawAABB(box, Color::Blue);
	}
}
```

---

### Frame Rate Drop

**Profiling:**
1. Use Visual Studio Profiler (CPU Usage)
2. Identify hot functions (most time spent)
3. Optimize hot path
4. Repeat

**Common Culprits:**
- Transform hierarchy updates (cache world matrix)
- Collision detection (use spatial partitioning)
- Rendering (batch draw calls)

---

## Tools

### Visual Studio Diagnostic Tools

**CPU Usage:**
- Debug → Performance Profiler → CPU Usage
- Shows function call times

**Memory Usage:**
- Debug → Performance Profiler → Memory Usage
- Shows allocations and leaks

**GPU Usage:**
- Debug → Performance Profiler → GPU Usage
- Shows rendering performance

---

### External Tools

**Tracy Profiler:**
- Real-time profiler
- Frame-by-frame analysis
- Open source

**RenderDoc:**
- Graphics debugger
- Captures and analyzes frames
- Free and open source

**Dr. Memory:**
- Memory error detector
- Finds leaks, buffer overruns, use-after-free
- Free and open source

---

## Summary

**Visual Studio:**
- Breakpoints (regular, conditional, data)
- Watch window (expressions, array ranges)
- Call stack (navigate frames)
- Threads window (view all threads)

**Threading:**
- Use mutexes for shared state
- Consistent lock order (avoid deadlock)
- Use FrameStream for cross-thread communication

**Memory:**
- Check for null before dereference
- Set pointers to nullptr after delete
- Use RAII and smart pointers

**Performance:**
- Cache expensive computations
- Reuse allocations
- Use StringCRC for IDs

**Logging:**
- DIA_LOG for events
- DIA_ASSERT for invariants
- Strategic logging (errors, state changes)

**[→ Coding Standards](coding-standards.md)**  
**[→ Common Tasks](../00-getting-started/common-tasks.md)**  
**[→ Thread Safety Guide](../06-ai-guides/thread-safety-guide.md)**
