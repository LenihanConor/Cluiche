# Entry Points for Common Tasks

**Last Updated:** 2026-04-01

Step-by-step guides for AI agents to perform common development tasks in the Cluiche codebase.

---

## Task Index

**Module System:**
1. [Add a New DiaCore Container](#task-1-add-a-new-diacore-container)
2. [Add a New Cluiche Module](#task-2-add-a-new-cluiche-module)
3. [Add Module Dependency](#task-3-add-module-dependency)

**Level System:**
4. [Create a New Level](#task-4-create-a-new-level)
5. [Register Level in Factory](#task-5-register-level-in-factory)

**Threading:**
6. [Add a New Phase](#task-6-add-a-new-phase)
7. [Modify Thread Synchronization](#task-7-modify-thread-synchronization)

**UI and Graphics:**
8. [Extend UI Functionality](#task-8-extend-ui-functionality)
9. [Add Custom Rendering](#task-9-add-custom-rendering)

**Math and Utilities:**
10. [Add a DiaMaths Function](#task-10-add-a-diamaths-function)
11. [Fix DiaMaths Template Bug](#task-11-fix-diamaths-template-bug)

**Testing and Debugging:**
12. [Add Unit Test to UnitTestLevel](#task-12-add-unit-test-to-unittestlevel)
13. [Debug Threading Issue](#task-13-debug-threading-issue)

---

## Task 1: Add a New DiaCore Container

**Goal:** Create a new custom container (e.g., `Stack<T>`)

### Steps

**1. Choose Location**
```
Dia/DiaCore/Containers/Stack/
```

**2. Create Header File**

File: `Dia/DiaCore/Containers/Stack/Stack.h`

```cpp
#pragma once

#include "DiaCore/Containers/Arrays/DynamicArray.h"

namespace Dia
{
    namespace Core
    {
        namespace Containers
        {
            template<class T>
            class Stack
            {
            public:
                Stack() = default;
                ~Stack() = default;

                void Push(const T& value) {
                    mData.PushBack(value);
                }

                T Pop() {
                    DIA_ASSERT(!IsEmpty(), "Stack is empty");
                    T value = mData[mData.Size() - 1];
                    mData.PopBack();
                    return value;
                }

                const T& Top() const {
                    DIA_ASSERT(!IsEmpty(), "Stack is empty");
                    return mData[mData.Size() - 1];
                }

                bool IsEmpty() const {
                    return mData.IsEmpty();
                }

                unsigned int Size() const {
                    return mData.Size();
                }

            private:
                DynamicArray<T> mData;
            };
        }
    }
}
```

**3. Create Module Architecture File**

File: `Dia/DiaCore/Containers/Stack/dia.core.containers.stack.architecture.module.md`

```yaml
---
schema: dia.module.v1
module_id: dia.core.containers.stack
name: Stack
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Containers/Stack
language: cpp
parent_module_id: dia.core.containers

summary: >
  Stack container (LIFO).

intent: >
  Provides a stack (Last-In-First-Out) container built on DynamicArray.

responsibilities:
  - Provide LIFO stack operations (Push, Pop, Top)
  - Maintain stack invariants (cannot pop empty stack)

non_responsibilities:
  - Thread safety (user must synchronize)
  - Memory allocation strategy (uses DynamicArray)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaCore/Containers/Stack/Stack.h
  namespaces:
    - Dia::Core::Containers
  entry_points:
    - Stack<T>

dependencies:
  required:
    - dia.core.containers.arrays
  forbidden: []
---

# Stack<T>

Stack container using DynamicArray as backing storage.
```

**4. Update DiaCore Project**

Add to `Dia/DiaCore/DiaCore.vcxproj`:

```xml
<ItemGroup>
  <ClInclude Include="Containers\Stack\Stack.h" />
</ItemGroup>
```

Add to `Dia/DiaCore/DiaCore.vcxproj.filters`:

```xml
<ClInclude Include="Containers\Stack\Stack.h">
  <Filter>Containers\Stack</Filter>
</ClInclude>
```

**5. Update Parent Module**

Edit `Dia/DiaCore/Containers/dia.core.containers.architecture.module.md`:

Add to `dependent_modules`:
```yaml
dependent_modules:
  - dia.core.containers.arrays
  - dia.core.containers.hashtables
  - dia.core.containers.stack  # NEW
```

**6. Test**

Add test to `Cluiche/Levels/UnitTestLevel/UnitTestLevel.cpp`:

```cpp
void UnitTestLevel::RunTests() {
    TestStack();
}

void UnitTestLevel::TestStack() {
    Dia::Core::Containers::Stack<int> stack;
    
    stack.Push(1);
    stack.Push(2);
    stack.Push(3);
    
    DIA_ASSERT(stack.Top() == 3, "Top should be 3");
    DIA_ASSERT(stack.Pop() == 3, "Pop should return 3");
    DIA_ASSERT(stack.Pop() == 2, "Pop should return 2");
    DIA_ASSERT(stack.Size() == 1, "Size should be 1");
}
```

**7. Verify**

```bash
python Tools/dia_modules.py --validate
```

---

## Task 2: Add a New Cluiche Module

**Goal:** Create a new module for the Sim thread (e.g., `SimPhysicsModule`)

### Steps

**1. Choose Location**
```
Cluiche/CluicheKernel/ApplicationFlow/Modules/
```

**2. Create Header File**

File: `Cluiche/CluicheKernel/ApplicationFlow/Modules/SimPhysicsModule.h`

```cpp
#pragma once

#include "DiaApplicationFlow/ApplicationModule.h"

namespace Cluiche
{
    class SimPhysicsModule : public Dia::Application::Module
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        SimPhysicsModule();
        virtual ~SimPhysicsModule() = default;

        // Public API
        void UpdatePhysics(float deltaTime);

    private:
        // Module lifecycle
        virtual void DoStart() override;
        virtual void DoUpdate() override;
        virtual void DoStop() override;

        // Internal state
        bool mInitialized;
    };
}
```

**3. Create Implementation File**

File: `Cluiche/CluicheKernel/ApplicationFlow/Modules/SimPhysicsModule.cpp`

```cpp
#include "SimPhysicsModule.h"
#include "DiaCore/Core/Log.h"

namespace Cluiche
{
    const Dia::Core::StringCRC SimPhysicsModule::kUniqueId = Dia::Core::StringCRC("SimPhysicsModule");

    SimPhysicsModule::SimPhysicsModule()
        : Dia::Application::Module(kUniqueId)
        , mInitialized(false)
    {
        // Register dependencies if needed
        // RegisterDependency(&GetDependency<SomeOtherModule>());
    }

    void SimPhysicsModule::DoStart()
    {
        DIA_LOG("SimPhysicsModule: Starting");
        // Initialize physics system
        mInitialized = true;
    }

    void SimPhysicsModule::DoUpdate()
    {
        if (mInitialized)
        {
            // Update physics (called every frame)
            float deltaTime = 0.016f; // Get from TimeServer
            UpdatePhysics(deltaTime);
        }
    }

    void SimPhysicsModule::DoStop()
    {
        DIA_LOG("SimPhysicsModule: Stopping");
        // Cleanup physics system
        mInitialized = false;
    }

    void SimPhysicsModule::UpdatePhysics(float deltaTime)
    {
        // Physics update logic here
    }
}
```

**4. Update Cluiche Project**

Add to `Cluiche/CluicheTest/Cluiche.vcxproj`:

```xml
<ItemGroup>
  <ClInclude Include="CluicheKernel\ApplicationFlow\Modules\SimPhysicsModule.h" />
</ItemGroup>
<ItemGroup>
  <ClCompile Include="CluicheKernel\ApplicationFlow\Modules\SimPhysicsModule.cpp" />
</ItemGroup>
```

**5. Add Module to Phase**

Edit `Cluiche/ApplicationFlow/Phases/SimBootPhase.cpp`:

```cpp
#include "CluicheKernel/ApplicationFlow/Modules/SimPhysicsModule.h"

void SimBootPhase::DoStart()
{
    // Existing modules...
    AddModule(new Cluiche::SimPhysicsModule());
}
```

**6. Build and Test**

```bash
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
```

---

## Task 3: Add Module Dependency

**Goal:** Make `ModuleB` depend on `ModuleA`

### Steps

**1. Identify Modules**

Example: `SimPhysicsModule` depends on `SimTimeServerModule`

**2. Update Constructor**

Edit `SimPhysicsModule.h`:

```cpp
class SimPhysicsModule : public Dia::Application::Module
{
public:
    SimPhysicsModule();
    
private:
    Cluiche::SimTimeServerModule* mTimeServer;  // Dependency
};
```

Edit `SimPhysicsModule.cpp`:

```cpp
#include "SimTimeServerModule.h"

SimPhysicsModule::SimPhysicsModule()
    : Dia::Application::Module(kUniqueId)
    , mTimeServer(nullptr)
{
    // Register dependency
    RegisterDependency(&GetDependency<Cluiche::SimTimeServerModule>());
}

void SimPhysicsModule::DoStart()
{
    // Get dependency
    mTimeServer = GetDependency<Cluiche::SimTimeServerModule>();
    DIA_ASSERT(mTimeServer != nullptr, "TimeServer dependency missing");
}

void SimPhysicsModule::DoUpdate()
{
    // Use dependency
    float deltaTime = mTimeServer->GetDeltaTime();
    UpdatePhysics(deltaTime);
}
```

**3. Automatic Ordering**

Phase will automatically start `SimTimeServerModule` before `SimPhysicsModule`.

---

## Task 4: Create a New Level

**Goal:** Create a new game level (e.g., `MyGameLevel`)

### Steps

**1. Create Directory**
```
Cluiche/Levels/MyGameLevel/
```

**2. Create Header File**

File: `Cluiche/Levels/MyGameLevel/MyGameLevel.h`

```cpp
#pragma once

#include "DiaApplicationFlow/ApplicationStateObject.h"

namespace Cluiche
{
    class MyGameLevel : public Dia::Application::StateObject
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        MyGameLevel();
        virtual ~MyGameLevel() = default;

    private:
        // Lifecycle
        virtual void DoStart() override;
        virtual void DoUpdate() override;
        virtual void DoStop() override;
    };
}
```

**3. Create Implementation File**

File: `Cluiche/Levels/MyGameLevel/MyGameLevel.cpp`

```cpp
#include "MyGameLevel.h"
#include "DiaCore/Core/Log.h"

namespace Cluiche
{
    const Dia::Core::StringCRC MyGameLevel::kUniqueId = Dia::Core::StringCRC("MyGameLevel");

    MyGameLevel::MyGameLevel()
        : Dia::Application::StateObject(kUniqueId)
    {
    }

    void MyGameLevel::DoStart()
    {
        DIA_LOG("MyGameLevel: Starting");
        // Initialize level (load assets, spawn entities, etc.)
    }

    void MyGameLevel::DoUpdate()
    {
        // Update game logic every frame
    }

    void MyGameLevel::DoStop()
    {
        DIA_LOG("MyGameLevel: Stopping");
        // Cleanup level (unload assets, destroy entities, etc.)
    }
}
```

**4. Create Module Architecture File**

File: `Cluiche/Levels/MyGameLevel/dia.cluiche.levels.mygamelevel.architecture.module.md`

```yaml
---
schema: dia.module.v1
module_id: dia.cluiche.levels.mygamelevel
name: MyGameLevel
owner_team: TBD
layer: application
status: active
maturity: dev

path: Cluiche/Levels/MyGameLevel
language: cpp
parent_module_id: dia.cluiche.levels

summary: >
  My custom game level.

intent: >
  Implements gameplay logic for MyGameLevel.

responsibilities:
  - Define level behavior
  - Manage level state

dependent_modules: []

public_api:
  headers:
    - Cluiche/Levels/MyGameLevel/MyGameLevel.h
  entry_points:
    - MyGameLevel

dependencies:
  required:
    - dia.application
---
```

**5. Update Cluiche Project**

Add to `Cluiche/CluicheTest/Cluiche.vcxproj`:

```xml
<ItemGroup>
  <ClInclude Include="Levels\MyGameLevel\MyGameLevel.h" />
</ItemGroup>
<ItemGroup>
  <ClCompile Include="Levels\MyGameLevel\MyGameLevel.cpp" />
</ItemGroup>
```

**6. Register Level** (see Task 5)

---

## Task 5: Register Level in Factory

**Goal:** Make level available at runtime via LevelFactory

### Steps

**1. Include Level Header**

Edit `Cluiche/CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.cpp`:

```cpp
#include "Cluiche/Levels/MyGameLevel/MyGameLevel.h"
```

**2. Register in DoStart()**

```cpp
void LevelFactoryModule::DoStart()
{
    // Existing levels...
    Dia::Application::LevelFactory::Instance()->Register<Cluiche::DummyStage>("DummyStage");
    Dia::Application::LevelFactory::Instance()->Register<Cluiche::UnitTestLevel>("UnitTestLevel");
    
    // NEW
    Dia::Application::LevelFactory::Instance()->Register<Cluiche::MyGameLevel>("MyGameLevel");
}
```

**3. Create Level at Runtime**

```cpp
Dia::Application::ILevel* level = 
    Dia::Application::LevelFactory::Instance()->Create("MyGameLevel");
level->Start();
```

---

## Task 6: Add a New Phase

**Goal:** Add a new phase to a processing unit (e.g., `SimRunningPhase`)

### Steps

**1. Create Header File**

File: `Cluiche/ApplicationFlow/Phases/SimRunningPhase.h`

```cpp
#pragma once

#include "DiaApplicationFlow/ApplicationPhase.h"

namespace Cluiche
{
    class SimRunningPhase : public Dia::Application::Phase
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        SimRunningPhase();
        virtual ~SimRunningPhase() = default;

    private:
        virtual void DoStart() override;
        virtual void DoUpdate() override;
        virtual void DoStop() override;
    };
}
```

**2. Create Implementation File**

File: `Cluiche/ApplicationFlow/Phases/SimRunningPhase.cpp`

```cpp
#include "SimRunningPhase.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h"
#include "DiaCore/Core/Log.h"

namespace Cluiche
{
    const Dia::Core::StringCRC SimRunningPhase::kUniqueId = 
        Dia::Core::StringCRC("SimRunningPhase");

    SimRunningPhase::SimRunningPhase()
        : Dia::Application::Phase(kUniqueId)
    {
    }

    void SimRunningPhase::DoStart()
    {
        DIA_LOG("SimRunningPhase: Starting");
        
        // Add modules for running phase
        AddModule(new Cluiche::SimTimeServerModule());
        // Add more modules...
    }

    void SimRunningPhase::DoUpdate()
    {
        // Update all modules
        UpdateAllModules();
    }

    void SimRunningPhase::DoStop()
    {
        DIA_LOG("SimRunningPhase: Stopping");
        // Cleanup handled by Phase base class
    }
}
```

**3. Transition to New Phase**

Edit `Cluiche/ApplicationFlow/Phases/SimBootStrapPhase.cpp`:

```cpp
void SimBootStrapPhase::DoUpdate()
{
    // After bootstrap complete, transition to running
    if (IsBootstrapComplete())
    {
        GetProcessingUnit()->TransitionPhase(new Cluiche::SimRunningPhase());
    }
}
```

---

## Task 7: Modify Thread Synchronization

**Goal:** Add thread-safe communication between threads

### Steps

**1. Identify Communication Need**

Example: Main thread sends messages to Sim thread

**2. Use FrameStream**

Edit `MainProcessingUnit.h`:

```cpp
#include "DiaApplicationFlow/FrameStream.h"

class MainProcessingUnit : public Dia::Application::ProcessingUnit
{
private:
    Dia::Application::FrameStream<std::string> mMainToSimMessages;
public:
    Dia::Application::FrameStream<std::string>& GetMainToSimMessages() {
        return mMainToSimMessages;
    }
};
```

**3. Write from Main Thread**

```cpp
// In MainUIModule or Main thread code
mainPU->GetMainToSimMessages().Write("ButtonClicked");
```

**4. Read from Sim Thread**

```cpp
// In SimInputFrameStreamModule or Sim thread code
std::string message;
if (mainPU->GetMainToSimMessages().Read(message))
{
    // Process message
    DIA_LOG("Sim received: %s", message.c_str());
}
```

**5. FrameStream is Thread-Safe**

Uses `std::mutex` internally, safe for producer/consumer.

---

## Task 8: Extend UI Functionality

**Goal:** Add new UI element and bind to C++

### Steps

**1. Locate UI Files**

UI pages in: `External/Webix/` or custom HTML files

**2. Edit HTML**

Add button:
```html
<button id="myButton" onclick="handleMyButtonClick()">Click Me</button>
```

**3. Add JavaScript Handler**

```javascript
function handleMyButtonClick() {
    // Call C++ via UI binding
    CluicheAPI.OnMyButtonClicked();
}
```

**4. Bind C++ Method**

Edit `Cluiche/CluicheKernel/ApplicationFlow/Modules/MainUIModule.cpp`:

```cpp
void MainUIModule::DoStart()
{
    // Existing bindings...
    
    // NEW binding
    mUISystem->BindMethod("OnMyButtonClicked", 
        [this]() {
            DIA_LOG("Button clicked from UI");
            // Handle button click
        });
}
```

**5. Test**

Run application, click button, verify log output.

---

## Task 9: Add Custom Rendering

**Goal:** Draw custom shapes to screen

### Steps

**1. Get Canvas**

In `RenderProcessingUnit` or Render-thread module:

```cpp
Dia::Graphics::ICanvas* canvas = GetCanvas();
```

**2. Draw Shapes**

```cpp
void MyRenderModule::DoUpdate()
{
    Dia::Graphics::ICanvas* canvas = GetCanvas();
    
    // Draw line
    Dia::Maths::Vector2D start(100.0f, 100.0f);
    Dia::Maths::Vector2D end(200.0f, 200.0f);
    Dia::Graphics::Color color(255, 0, 0, 255);  // Red
    canvas->DrawLine(start, end, color);
    
    // Draw circle
    Dia::Maths::Vector2D center(300.0f, 300.0f);
    float radius = 50.0f;
    canvas->DrawCircle(center, radius, color);
}
```

**3. Rendering Happens Automatically**

`RenderProcessingUnit` clears and presents each frame.

---

## Task 10: Add a DiaMaths Function

**Goal:** Add new math utility function (e.g., `Clamp`)

### Steps

**1. Choose Location**

If it's a general utility: `Dia/DiaMaths/Core/FloatMaths.h`

**2. Add Function**

Edit `Dia/DiaMaths/Core/FloatMaths.h`:

```cpp
namespace Dia
{
    namespace Maths
    {
        inline float Clamp(float value, float min, float max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }
        
        template<typename T>
        inline T Clamp(const T& value, const T& min, const T& max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }
    }
}
```

**3. Add Test**

Edit `Cluiche/Levels/UnitTestLevel/UnitTestLevel.cpp`:

```cpp
void UnitTestLevel::TestClamp()
{
    float result = Dia::Maths::Clamp(5.0f, 0.0f, 10.0f);
    DIA_ASSERT(result == 5.0f, "Clamp(5, 0, 10) should be 5");
    
    result = Dia::Maths::Clamp(-5.0f, 0.0f, 10.0f);
    DIA_ASSERT(result == 0.0f, "Clamp(-5, 0, 10) should be 0");
    
    result = Dia::Maths::Clamp(15.0f, 0.0f, 10.0f);
    DIA_ASSERT(result == 10.0f, "Clamp(15, 0, 10) should be 10");
}
```

---

## Task 11: Fix DiaMaths Template Bug

**Goal:** Fix missing `InverseLerp` specialization for `Vector2D`

### Steps

**1. Locate Function**

File: `Dia/DiaMaths/Core/Interpolation.h`

**2. Add Specialization**

```cpp
// Existing scalar version
inline float InverseLerp(float a, float b, float value)
{
    return (value - a) / (b - a);
}

// NEW: Vector2D version
inline float InverseLerp(const Vector2D& a, const Vector2D& b, const Vector2D& value)
{
    // Use X component (or magnitude if needed)
    return InverseLerp(a.x, b.x, value.x);
    
    // Alternative: use magnitude
    // float distAB = (b - a).Magnitude();
    // float distAValue = (value - a).Magnitude();
    // return distAValue / distAB;
}
```

**3. Add Test**

```cpp
void UnitTestLevel::TestInverseLerpVector2D()
{
    Dia::Maths::Vector2D a(0.0f, 0.0f);
    Dia::Maths::Vector2D b(10.0f, 0.0f);
    Dia::Maths::Vector2D value(5.0f, 0.0f);
    
    float t = Dia::Maths::InverseLerp(a, b, value);
    DIA_ASSERT(fabs(t - 0.5f) < 0.001f, "InverseLerp should return 0.5");
}
```

**4. Verify Fix**

Build and run tests.

---

## Task 12: Add Unit Test to UnitTestLevel

**Goal:** Add a new test to in-engine test harness

### Steps

**1. Open File**

Edit `Cluiche/Levels/UnitTestLevel/UnitTestLevel.cpp`

**2. Add Test Method**

```cpp
void UnitTestLevel::RunTests()
{
    TestDynamicArray();
    TestVector2D();
    TestMyNewFeature();  // NEW
}

void UnitTestLevel::TestMyNewFeature()
{
    // Arrange
    int input = 42;
    
    // Act
    int result = MyNewFunction(input);
    
    // Assert
    DIA_ASSERT(result == 84, "MyNewFunction should double input");
}
```

**3. Run Tests**

Build and run Cluiche, select "UnitTestLevel" from UI.

**4. Check Console**

Look for test output:
```
[TEST] Running TestMyNewFeature...
[PASS] TestMyNewFeature
```

---

## Task 13: Debug Threading Issue

**Goal:** Investigate potential race condition or deadlock

### Steps

**1. Identify Symptoms**

- Crash?
- Deadlock (hangs)?
- Incorrect values?

**2. Check Thread Access**

Identify which threads access shared state:
- Main thread: UI, input
- Render thread: Rendering
- Sim thread: Game logic

**3. Add Mutex Protection**

Example: Protect shared variable

```cpp
class SharedState
{
public:
    void SetValue(int value) {
        std::lock_guard<std::mutex> lock(mMutex);
        mValue = value;
    }
    
    int GetValue() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mValue;
    }

private:
    int mValue;
    mutable std::mutex mMutex;
};
```

**4. Use FrameStream for Cross-Thread Communication**

Instead of direct shared state, use `FrameStream<T>` (thread-safe queue).

**5. Enable Thread Sanitizer** (if available)

Compile with `/fsanitize=thread` (MSVC) or `-fsanitize=thread` (GCC/Clang).

**6. Add Logging**

```cpp
DIA_LOG("Thread %d: Accessing shared state", std::this_thread::get_id());
```

**7. Verify Fix**

Run stress test (tight loop, many iterations).

---

## Summary

**Quick Task Reference:**

| Task | Entry Point |
|------|-------------|
| **Add container** | `Dia/DiaCore/Containers/` |
| **Add module** | `Cluiche/CluicheKernel/ApplicationFlow/Modules/` |
| **Add level** | `Cluiche/Levels/` |
| **Add phase** | `Cluiche/ApplicationFlow/Phases/` |
| **Add math function** | `Dia/DiaMaths/Core/` |
| **Add test** | `Cluiche/Levels/UnitTestLevel/UnitTestLevel.cpp` |
| **Debug threading** | Check `std::mutex`, use `FrameStream<T>` |

**[→ Code Patterns Reference](patterns-reference.md)**  
**[→ System Boundaries](system-boundaries.md)**  
**[→ Back to AI Guide](AI-README.md)**
