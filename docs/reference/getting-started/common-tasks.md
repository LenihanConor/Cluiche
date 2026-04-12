# Common Tasks

**Last Updated:** 2026-04-01

Common developer workflows and how-to guides for working with the Cluiche codebase.

---

## Overview

This document provides step-by-step instructions for common development tasks in the Cluiche project.

**Tasks Covered:**
1. Add a new Module
2. Create a new Level
3. Add a new DiaCore container
4. Debug a threading issue
5. Add a new Phase to a ProcessingUnit
6. Modify the render loop
7. Add input handling
8. Integrate a new external library

---

## Task 1: Add a New Module

**Goal:** Create a new module in a ProcessingUnit (e.g., add `PhysicsModule` to `SimProcessingUnit`)

### Step 1: Create Module Files

```cpp
// File: Cluiche/CluicheTest/ApplicationFlow/Modules/SimPhysicsModule.h
#pragma once

#include "DiaApplication/Module/Module.h"

namespace Cluiche
{
    class SimPhysicsModule : public Dia::Application::Module
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;
        
        SimPhysicsModule();
        
    protected:
        // Module lifecycle
        void OnConstruct() override;
        void OnStart() override;
        void OnUpdate() override;
        void OnStop() override;
        void OnDestruct() override;
        
    private:
        // Module data
    };
}
```

```cpp
// File: Cluiche/CluicheTest/ApplicationFlow/Modules/SimPhysicsModule.cpp
#include "SimPhysicsModule.h"

namespace Cluiche
{
    const Dia::Core::StringCRC SimPhysicsModule::kUniqueId = Dia::Core::StringCRC("SimPhysicsModule");
    
    SimPhysicsModule::SimPhysicsModule()
        : Module(kUniqueId)
    {
    }
    
    void SimPhysicsModule::OnConstruct()
    {
        // Initialize physics system
    }
    
    void SimPhysicsModule::OnStart()
    {
        // Start physics simulation
    }
    
    void SimPhysicsModule::OnUpdate()
    {
        // Update physics
    }
    
    void SimPhysicsModule::OnStop()
    {
        // Stop physics simulation
    }
    
    void SimPhysicsModule::OnDestruct()
    {
        // Cleanup physics system
    }
}
```

---

### Step 2: Add to Visual Studio Project

1. Open `Cluiche/CluicheTest/Cluiche.vcxproj` in text editor
2. Add files to `<ItemGroup>`:

```xml
<ClInclude Include="ApplicationFlow\Modules\SimPhysicsModule.h" />
<ClCompile Include="ApplicationFlow\Modules\SimPhysicsModule.cpp" />
```

3. Open `Cluiche/CluicheTest/Cluiche.vcxproj.filters`
4. Add files to `<ItemGroup>`:

```xml
<ClInclude Include="ApplicationFlow\Modules\SimPhysicsModule.h">
  <Filter>ApplicationFlow\Modules</Filter>
</ClInclude>
<ClCompile Include="ApplicationFlow\Modules\SimPhysicsModule.cpp">
  <Filter>ApplicationFlow\Modules</Filter>
</ClCompile>
```

---

### Step 3: Register Module with ProcessingUnit

```cpp
// File: Cluiche/CluicheTest/ApplicationFlow/SimProcessingUnit.cpp
#include "Modules/SimPhysicsModule.h"

void SimProcessingUnit::OnConstruct()
{
    // Add physics module
    AddModule(new SimPhysicsModule());
    
    // Other modules...
}
```

---

### Step 4: Add Dependencies (if needed)

```cpp
// In a Phase that uses the physics module
void SimUpdatePhase::OnConstruct()
{
    // Declare dependency on physics module
    AddDependency(SimPhysicsModule::kUniqueId);
}
```

---

### Step 5: Build and Test

```bash
# Build
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=Win32

# Run
Cluiche/bin/exe/Debug/Cluiche.exe
```

---

## Task 2: Create a New Level

**Goal:** Create a new game level (e.g., `TestLevel`)

### Step 1: Create Level Files

```cpp
// File: Cluiche/CluicheTest/Levels/TestLevel.h
#pragma once

#include "DiaApplication/Level/ILevel.h"

namespace Cluiche
{
    class TestLevel : public Dia::Application::ILevel
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;
        
        TestLevel();
        ~TestLevel();
        
        // ILevel interface
        const Dia::Core::StringCRC& GetId() const override;
        void Load() override;
        void Unload() override;
        
    private:
        // Level data
    };
}
```

```cpp
// File: Cluiche/CluicheTest/Levels/TestLevel.cpp
#include "TestLevel.h"

namespace Cluiche
{
    const Dia::Core::StringCRC TestLevel::kUniqueId = Dia::Core::StringCRC("TestLevel");
    
    TestLevel::TestLevel()
    {
    }
    
    TestLevel::~TestLevel()
    {
    }
    
    const Dia::Core::StringCRC& TestLevel::GetId() const
    {
        return kUniqueId;
    }
    
    void TestLevel::Load()
    {
        // Load level assets
        // Setup level state
    }
    
    void TestLevel::Unload()
    {
        // Cleanup level
    }
}
```

---

### Step 2: Add to Visual Studio Project

Same process as adding a module (see Task 1, Step 2).

---

### Step 3: Register with LevelFactory

```cpp
// File: Cluiche/CluicheTest/Main.cpp or appropriate factory registration location
#include "Levels/TestLevel.h"

// Register level factory
Dia::Application::LevelFactory::Instance().Register(
    TestLevel::kUniqueId,
    []() -> Dia::Application::ILevel* { return new TestLevel(); });
```

---

### Step 4: Transition to Level

```cpp
// Transition to TestLevel
Dia::Application::LevelFactory::Instance().TransitionTo(TestLevel::kUniqueId);
```

---

## Task 3: Add a New DiaCore Container

**Goal:** Add a new container type (e.g., `PriorityQueue`)

### Step 1: Create Container Files

```cpp
// File: Dia/DiaCore/Containers/PriorityQueue/PriorityQueue.h
#pragma once

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
    namespace Core
    {
        namespace Containers
        {
            template <class T>
            class PriorityQueue
            {
            public:
                PriorityQueue();
                
                void Push(const T& element, int priority);
                T Pop();
                
                const T& Top() const;
                bool IsEmpty() const;
                unsigned int Size() const;
                
            private:
                struct Element
                {
                    T value;
                    int priority;
                };
                
                DynamicArrayC<Element> mElements;
                
                void Heapify(unsigned int index);
            };
        }
    }
}
```

---

### Step 2: Add to DiaCore Project

1. Add files to `Dia/DiaCore/DiaCore.vcxproj`
2. Add files to `Dia/DiaCore/DiaCore.vcxproj.filters`

---

### Step 3: Write Unit Tests

```cpp
// File: Tests/UnitTests/DiaCore/Containers/PriorityQueueTests.cpp
#include "DiaCore/Containers/PriorityQueue/PriorityQueue.h"

void TestPriorityQueuePush()
{
    Dia::Core::Containers::PriorityQueue<int> queue;
    queue.Push(10, 1);
    queue.Push(20, 2);
    queue.Push(5, 0);
    
    DIA_ASSERT(queue.Top() == 20, "Top should be highest priority");
}

void TestPriorityQueuePop()
{
    Dia::Core::Containers::PriorityQueue<int> queue;
    queue.Push(10, 1);
    queue.Push(20, 2);
    
    int top = queue.Pop();
    DIA_ASSERT(top == 20, "Popped element should be 20");
    DIA_ASSERT(queue.Top() == 10, "New top should be 10");
}
```

---

### Step 4: Document in Module Architecture File

Update `Dia/DiaCore/dia.core.containers.architecture.module.md`:

```yaml
responsibilities:
  - Provides PriorityQueue<T> for priority-based element access
```

---

## Task 4: Debug a Threading Issue

**Goal:** Investigate and fix a race condition or threading bug

### Step 1: Enable Thread Safety Checks

```cpp
// In affected code, add thread ID logging
#include <thread>

void ProblematicFunction()
{
    std::thread::id threadId = std::this_thread::get_id();
    DIA_LOG("ProblematicFunction called from thread: %d", threadId);
    
    // Your code here
}
```

---

### Step 2: Use Mutex for Shared State

```cpp
#include <mutex>

class ThreadSafeClass
{
public:
    void Update()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        // Access shared state safely
        mSharedData++;
    }
    
private:
    std::mutex mMutex;
    int mSharedData;
};
```

---

### Step 3: Use FrameStream for Cross-Thread Communication

```cpp
// Producer (Main thread)
Dia::Application::FrameStream<InputEvent> mInputFrameStream;
mInputFrameStream.Write(event);

// Consumer (Sim thread)
InputEvent event;
while (mInputFrameStream.Read(event))
{
    ProcessInput(event);
}
```

---

### Step 4: Verify Thread Affinity

```cpp
// Ensure function called from correct thread
void RenderFunction()
{
    DIA_ASSERT(IsRenderThread(), "RenderFunction must be called from Render thread");
    // Render code
}
```

**[→ Thread Safety Guide](../ai-guides/thread-safety-guide.md)**

---

## Task 5: Add a New Phase to a ProcessingUnit

**Goal:** Add a phase to a ProcessingUnit (e.g., add `PreUpdatePhase` to `SimProcessingUnit`)

### Step 1: Create Phase Files

```cpp
// File: Cluiche/CluicheTest/ApplicationFlow/Phases/SimPreUpdatePhase.h
#pragma once

#include "DiaApplication/Phase/Phase.h"

namespace Cluiche
{
    class SimPreUpdatePhase : public Dia::Application::Phase
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;
        
        SimPreUpdatePhase();
        
    protected:
        void OnUpdate() override;
    };
}
```

```cpp
// File: Cluiche/CluicheTest/ApplicationFlow/Phases/SimPreUpdatePhase.cpp
#include "SimPreUpdatePhase.h"

namespace Cluiche
{
    const Dia::Core::StringCRC SimPreUpdatePhase::kUniqueId = Dia::Core::StringCRC("SimPreUpdatePhase");
    
    SimPreUpdatePhase::SimPreUpdatePhase()
        : Phase(kUniqueId)
    {
    }
    
    void SimPreUpdatePhase::OnUpdate()
    {
        // Pre-update logic
    }
}
```

---

### Step 2: Add Phase to ProcessingUnit

```cpp
// File: Cluiche/CluicheTest/ApplicationFlow/SimProcessingUnit.cpp
void SimProcessingUnit::OnConstruct()
{
    // Add phases in execution order
    AddPhase(new SimPreUpdatePhase());   // NEW
    AddPhase(new SimUpdatePhase());
    AddPhase(new SimPostUpdatePhase());
}
```

---

### Step 3: Configure Phase Transitions

```cpp
void SimPreUpdatePhase::OnUpdate()
{
    // Automatically transitions to next phase (SimUpdatePhase) when OnUpdate completes
}
```

---

## Task 6: Modify the Render Loop

**Goal:** Change rendering behavior (e.g., add debug rendering)

### Step 1: Locate Render Module

```cpp
// File: Cluiche/CluicheTest/ApplicationFlow/Modules/RenderCanvasModule.cpp
void RenderCanvasModule::OnUpdate()
{
    // Clear
    mCanvas->Clear(Dia::Graphics::Color::Black);
    
    // Render game objects
    RenderGameObjects();
    
    // NEW: Add debug rendering
    #ifdef DEBUG
    RenderDebugInfo();
    #endif
    
    // Present
    mCanvas->Present();
}
```

---

### Step 2: Implement Debug Rendering

```cpp
void RenderCanvasModule::RenderDebugInfo()
{
    // Draw FPS
    char fpsText[32];
    sprintf(fpsText, "FPS: %.1f", GetFPS());
    mCanvas->DrawText(fpsText, Dia::Maths::Vector2D(10.0f, 10.0f), Dia::Graphics::Color::Yellow, 14);
    
    // Draw bounding boxes
    for (const auto& entity : mEntities)
    {
        DrawBoundingBox(entity.GetBounds());
    }
}
```

---

## Task 7: Add Input Handling

**Goal:** Add keyboard/mouse input handling

### Step 1: Locate Input Module

```cpp
// File: Cluiche/CluicheTest/ApplicationFlow/Modules/MainInputModule.cpp
void MainInputModule::OnUpdate()
{
    Dia::Input::InputEvent event;
    while (mInputManager->PollEvent(event))
    {
        // NEW: Add custom input handling
        HandleCustomInput(event);
        
        // Forward to Sim thread
        mInputFrameStream->Write(event);
    }
}
```

---

### Step 2: Implement Custom Input Handling

```cpp
void MainInputModule::HandleCustomInput(const Dia::Input::InputEvent& event)
{
    using namespace Dia::Input;
    
    if (event.type == InputEventType::KeyPressed)
    {
        if (event.key.code == KeyCode::Escape)
        {
            // Pause game
            QueuePhaseTransition(Dia::Application::Phase::kPaused);
        }
        else if (event.key.code == KeyCode::F11)
        {
            // Toggle fullscreen
            ToggleFullscreen();
        }
    }
}
```

---

## Task 8: Integrate a New External Library

**Goal:** Add a new third-party library (e.g., ImGui)

### Step 1: Add Library Files

```bash
# Copy library to External/
cp -r path/to/imgui External/imgui/
```

---

### Step 2: Update Project Include Directories

1. Open project properties: Right-click project → Properties
2. C/C++ → General → Additional Include Directories
3. Add: `$(SolutionDir)../External/imgui/`

---

### Step 3: Update Project Library Directories (if needed)

1. Linker → General → Additional Library Directories
2. Add: `$(SolutionDir)../External/imgui/lib/`

---

### Step 4: Add Library Dependencies (if needed)

1. Linker → Input → Additional Dependencies
2. Add: `imgui.lib`

---

### Step 5: Use Library in Code

```cpp
#include "imgui.h"

void RenderUI()
{
    ImGui::Begin("Debug Window");
    ImGui::Text("Hello from ImGui!");
    ImGui::End();
}
```

---

## Summary

**Common Workflows:**
1. **Add Module** - Create files, register with ProcessingUnit
2. **Create Level** - Implement ILevel, register with LevelFactory
3. **Add Container** - Create template class, write tests, document
4. **Debug Threading** - Use mutexes, FrameStreams, thread ID logging
5. **Add Phase** - Create Phase class, add to ProcessingUnit
6. **Modify Render** - Edit RenderModule::OnUpdate()
7. **Add Input** - Edit InputModule, handle events
8. **Add External Library** - Copy files, update project settings

**Best Practices:**
- Always write unit tests for new code
- Document changes in architecture files
- Use existing patterns (Module, Phase, Factory)
- Follow naming conventions
- Test in both Debug and Release configurations

**Next Steps:**
- Read glossary for terminology
- Read debugging tips for troubleshooting
- Read coding standards for style guidelines

**[→ Glossary](glossary.md)**  
**[→ Debugging Tips](../development/debugging-tips.md)**  
**[→ Coding Standards](../development/coding-standards.md)**
