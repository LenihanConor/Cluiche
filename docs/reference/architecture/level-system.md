# Level System

**Last Updated:** 2026-03-31

The Level System provides pluggable game states via a factory pattern, enabling runtime level loading and switching.

---

## Overview

**Levels** are self-contained game states that can be loaded and unloaded at runtime. Each level can define custom phases, modules, and UI pages.

**Key Features:**
- ✅ Factory pattern for level registration and creation
- ✅ Pluggable architecture (easy to add new levels)
- ✅ Level-specific phases and modules
- ✅ Level-specific UI pages
- ✅ Runtime level switching

**[→ Level lifecycle diagram](diagrams/level-lifecycle.mmd)**

---

## Core Components

### ILevel Interface

**Location:** `Cluiche/CluicheTest/CluicheKernel/ILevel.h` (conceptual, may vary)

**Interface:**
```cpp
class ILevel {
public:
    virtual ~ILevel() {}
    
    virtual void Start() = 0;   // Initialize level
    virtual void Update() = 0;  // Per-frame update
    virtual void Stop() = 0;    // Cleanup level
    
    virtual const char* GetName() const = 0;
};
```

**Lifecycle:**
1. **Create** - Factory instantiates level
2. **Start** - Level initializes (loads resources, sets up phases/modules)
3. **Update** - Level updates each frame (game logic)
4. **Stop** - Level cleans up (unloads resources, removes phases/modules)
5. **Destroy** - Factory deletes level

---

### LevelFactory

**Location:** `Cluiche/CluicheTest/CluicheKernel/LevelFactory.h`

**Purpose:** Registry and factory for level types

**Implementation:**
```cpp
class LevelFactory {
public:
    // Singleton access
    static LevelFactory& Instance();
    
    // Register level type
    template<typename T>
    void Register(const char* name) {
        StringCRC crc(name);
        mRegistry[crc] = []() -> ILevel* {
            return new T();
        };
    }
    
    // Create level instance
    ILevel* Create(const char* name) {
        StringCRC crc(name);
        auto it = mRegistry.find(crc);
        if (it != mRegistry.end()) {
            return it->second();  // Call factory function
        }
        return nullptr;
    }
    
    // Destroy level instance
    void Destroy(ILevel* level) {
        delete level;
    }
    
    // Get all registered level names
    DynamicArray<const char*> GetLevelNames() const;

private:
    LevelFactory() {}
    
    // Registry: name CRC -> factory function
    std::map<StringCRC, std::function<ILevel*()>> mRegistry;
};
```

**Singleton Pattern:** Single global registry for all levels

---

### LevelFactoryModule

**Location:** `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h`

**Purpose:** Provide module access to LevelFactory

**Implementation:**
```cpp
class LevelFactoryModule : public Dia::Application::Module {
public:
    LevelFactoryModule(MainKernelModule* kernel)
        : mKernel(kernel)
    {
        RegisterDependency(mKernel);
    }
    
    LevelFactory& GetLevelFactory() {
        return LevelFactory::Instance();
    }

private:
    void DoStart() override {
        // Nothing (factory is singleton)
    }
    
    void DoUpdate() override {
        // Nothing
    }
    
    void DoStop() override {
        // Nothing
    }
    
    MainKernelModule* mKernel;
};
```

**Wrapper Module:** Provides dependency tracking for factory access

---

## Level Registration

### Registration Flow

**When:** During `MainBootPhase::AfterModulesStart()`

**Where:** `Cluiche/CluicheTest/ApplicationFlow/Phases/MainBootPhase.cpp`

**Example:**
```cpp
void MainBootPhase::AfterModulesStart() {
    // Get factory via module
    LevelFactoryModule* factoryModule = GetModule<LevelFactoryModule>();
    LevelFactory& factory = factoryModule->GetLevelFactory();
    
    // Register levels
    factory.Register<DummyStage>("DummyStage");
    factory.Register<UnitTestLevel>("UnitTestLevel");
    // factory.Register<MyCustomLevel>("MyCustomLevel");
    
    // Transition to bootstrap phase
    mProcessingUnit->TransitionPhase(mNextPhase);
}
```

**Template Registration:** Factory stores type-erased factory function

---

## Level Creation

### Creation Flow

**When:** After user selects level from UI

**Where:** `SimBootStrapPhase` (Sim thread)

**Example:**
```cpp
void SimBootStrapPhase::OnLevelSelected(const char* levelName) {
    // Get factory
    LevelFactory& factory = LevelFactory::Instance();
    
    // Destroy current level (if any)
    if (mCurrentLevel) {
        mCurrentLevel->Stop();
        factory.Destroy(mCurrentLevel);
        mCurrentLevel = nullptr;
    }
    
    // Create new level
    mCurrentLevel = factory.Create(levelName);
    
    if (mCurrentLevel) {
        // Initialize level
        mCurrentLevel->Start();
        DIA_LOG("SimBootStrapPhase", "Level '%s' loaded", levelName);
    } else {
        DIA_LOG_ERROR("SimBootStrapPhase", "Failed to create level '%s'", levelName);
    }
}
```

**Error Handling:** Returns nullptr if level name not found

---

## Level Update

### Update Flow

**When:** Each frame in `SimBootStrapPhase::AfterModulesUpdate()`

**Where:** Sim thread

**Example:**
```cpp
void SimBootStrapPhase::AfterModulesUpdate() {
    // Update current level
    if (mCurrentLevel) {
        mCurrentLevel->Update();
    }
    
    // Generate frame data for rendering
    FrameData frameData = GenerateFrameData();
    
    // Write to render thread
    mStartData->mSimToRenderFrameStream->PushFrame(frameData);
}
```

**Per-Frame:** Level update called every simulation frame

---

## Example Levels

### DummyStage

**Location:** `Cluiche/CluicheTest/Stages/DummyStage/`

**Purpose:** Example level demonstrating basic functionality

**Structure:**
```
DummyStage/
├── DummyStage.h/cpp                 # Level implementation
├── LevelFlow/
│   └── Phases/
│       ├── DummyStageBootPhase.h/cpp
│       └── DummyStageRunningPhase.h/cpp
└── UI/
    ├── dummy_stage.html
    └── dummy_stage.css
```

**Implementation:**
```cpp
// DummyStage.h
class DummyStage : public ILevel {
public:
    DummyStage();
    ~DummyStage();
    
    void Start() override;
    void Update() override;
    void Stop() override;
    
    const char* GetName() const override { return "DummyStage"; }

private:
    // Level-specific state
    DummyStageBootPhase* mBootPhase;
    DummyStageRunningPhase* mRunningPhase;
    
    // Level-specific data
    int mScore;
    float mTime;
};

// DummyStage.cpp
DummyStage::DummyStage()
    : mBootPhase(nullptr)
    , mRunningPhase(nullptr)
    , mScore(0)
    , mTime(0.0f)
{
}

void DummyStage::Start() {
    DIA_LOG("DummyStage", "Starting");
    
    // Create level-specific phases
    mBootPhase = new DummyStageBootPhase();
    mRunningPhase = new DummyStageRunningPhase();
    
    // Initialize phase (could add to ProcessingUnit here)
    mBootPhase->Start();
    
    // Load UI page
    UISystem::Instance()->LoadPage("dummy_stage.html");
}

void DummyStage::Update() {
    // Update game logic
    mTime += TimeServer::Instance()->GetDeltaTimeFloat();
    
    if (/* some condition */) {
        mScore += 10;
    }
    
    // Update phases
    if (mRunningPhase) {
        mRunningPhase->Update();
    }
}

void DummyStage::Stop() {
    DIA_LOG("DummyStage", "Stopping with score %d", mScore);
    
    // Cleanup phases
    if (mBootPhase) {
        mBootPhase->Stop();
        delete mBootPhase;
    }
    if (mRunningPhase) {
        mRunningPhase->Stop();
        delete mRunningPhase;
    }
}
```

**Features:**
- Custom phases (DummyStageBootPhase, DummyStageRunningPhase)
- Custom UI page (dummy_stage.html)
- Level-specific state (mScore, mTime)

---

### UnitTestLevel

**Location:** `Cluiche/CluicheTest/Levels/UnitTestLevel/`

**Purpose:** In-engine test harness for running unit tests

**Structure:**
```
UnitTestLevel/
├── UnitTestLevel.h/cpp              # Test level implementation
├── LevelFlow/
│   └── Phases/
│       └── UnitTestRunningPhase.h/cpp
└── UI/
    ├── unit_test.html
    └── unit_test.css
```

**Implementation:**
```cpp
class UnitTestLevel : public ILevel {
public:
    void Start() override {
        DIA_LOG("UnitTestLevel", "Starting tests");
        
        // Register tests
        RegisterTest("TestDynamicArray", &UnitTestLevel::TestDynamicArray);
        RegisterTest("TestHashTable", &UnitTestLevel::TestHashTable);
        RegisterTest("TestVector2D", &UnitTestLevel::TestVector2D);
        
        // Run all tests
        RunAllTests();
    }
    
    void Update() override {
        // Tests run once in Start(), nothing to update
    }
    
    void Stop() override {
        DIA_LOG("UnitTestLevel", "Tests complete: %d passed, %d failed",
                mPassedTests, mFailedTests);
    }

private:
    void RegisterTest(const char* name, void (UnitTestLevel::*testFunc)()) {
        mTests.PushBack({name, testFunc});
    }
    
    void RunAllTests() {
        for (const auto& test : mTests) {
            RunTest(test);
        }
    }
    
    void RunTest(const Test& test) {
        try {
            (this->*test.func)();
            mPassedTests++;
            DIA_LOG("UnitTestLevel", "[PASS] %s", test.name);
        } catch (...) {
            mFailedTests++;
            DIA_LOG_ERROR("UnitTestLevel", "[FAIL] %s", test.name);
        }
    }
    
    // Test methods
    void TestDynamicArray() {
        DynamicArray<int> arr;
        arr.PushBack(1);
        arr.PushBack(2);
        DIA_ASSERT(arr.Size() == 2);
        DIA_ASSERT(arr[0] == 1);
        DIA_ASSERT(arr[1] == 2);
    }
    
    void TestHashTable() {
        HashTable<StringCRC, int> table;
        table.Insert(StringCRC("key"), 42);
        DIA_ASSERT(*table.Find(StringCRC("key")) == 42);
    }
    
    void TestVector2D() {
        Vector2D v1(1.0f, 2.0f);
        Vector2D v2(3.0f, 4.0f);
        Vector2D sum = v1 + v2;
        DIA_ASSERT(sum.x == 4.0f);
        DIA_ASSERT(sum.y == 6.0f);
    }
    
    struct Test {
        const char* name;
        void (UnitTestLevel::*func)();
    };
    
    DynamicArray<Test> mTests;
    int mPassedTests = 0;
    int mFailedTests = 0;
};
```

**Features:**
- Test registration and execution
- Pass/fail tracking
- In-engine testing (no external framework needed)

---

## Creating a New Level

### Step-by-Step Guide

#### 1. Create Level Directory

```bash
mkdir Cluiche/CluicheTest/Levels/MyLevel
mkdir Cluiche/CluicheTest/Levels/MyLevel/LevelFlow
mkdir Cluiche/CluicheTest/Levels/MyLevel/LevelFlow/Phases
mkdir Cluiche/CluicheTest/Levels/MyLevel/UI
```

#### 2. Create Level Header

**File:** `Cluiche/CluicheTest/Levels/MyLevel/MyLevel.h`

```cpp
#pragma once
#include "CluicheKernel/ILevel.h"

class MyLevel : public ILevel {
public:
    MyLevel();
    ~MyLevel();
    
    void Start() override;
    void Update() override;
    void Stop() override;
    
    const char* GetName() const override { return "MyLevel"; }

private:
    // Level-specific state
    int mLevelData;
};
```

#### 3. Implement Level

**File:** `Cluiche/CluicheTest/Levels/MyLevel/MyLevel.cpp`

```cpp
#include "MyLevel.h"

MyLevel::MyLevel()
    : mLevelData(0)
{
}

MyLevel::~MyLevel() {
}

void MyLevel::Start() {
    DIA_LOG("MyLevel", "Starting");
    
    // Initialize level
    mLevelData = 100;
    
    // Load UI
    // UISystem::Instance()->LoadPage("my_level.html");
}

void MyLevel::Update() {
    // Update game logic
    mLevelData++;
}

void MyLevel::Stop() {
    DIA_LOG("MyLevel", "Stopping");
    
    // Cleanup
}
```

#### 4. Register Level

**File:** `Cluiche/CluicheTest/ApplicationFlow/Phases/MainBootPhase.cpp`

```cpp
void MainBootPhase::AfterModulesStart() {
    LevelFactory& factory = LevelFactory::Instance();
    
    // Existing registrations
    factory.Register<DummyStage>("DummyStage");
    factory.Register<UnitTestLevel>("UnitTestLevel");
    
    // NEW: Register your level
    factory.Register<MyLevel>("MyLevel");
    
    // ...
}
```

#### 5. Add to Visual Studio Project

**Update:** `Cluiche/CluicheTest/Cluiche.vcxproj`

```xml
<ClCompile Include="Levels\MyLevel\MyLevel.cpp" />
```

```xml
<ClInclude Include="Levels\MyLevel\MyLevel.h" />
```

**[→ Visual Studio guide](../development/visual-studio-guide.md)**

#### 6. Build and Run

```bash
# Build solution
msbuild Cluiche.sln /p:Configuration=Release

# Run
Cluiche/bin/Release/Cluiche.exe
```

**Level Selection:** Select "MyLevel" from launch UI

---

## Advanced Features

### Level-Specific Phases

Levels can define custom phases and add them to the SimProcessingUnit:

```cpp
void MyLevel::Start() {
    // Create custom phase
    MyLevelRunningPhase* phase = new MyLevelRunningPhase(this);
    
    // Add to Sim ProcessingUnit
    SimProcessingUnit* simPU = GetSimProcessingUnit();
    simPU->TransitionPhase(phase);
}
```

**Use Case:** Complex level flow (boot → setup → running → boss fight → ending)

---

### Level-Specific Modules

Levels can define custom modules and add them to phases:

```cpp
class MyLevelModule : public Dia::Application::Module {
    // Level-specific functionality
};

void MyLevel::Start() {
    // Create and register module
    MyLevelModule* module = new MyLevelModule();
    
    // Add to current phase
    Phase* currentPhase = GetCurrentPhase();
    currentPhase->AddModule(module);
}
```

**Use Case:** Level-specific systems (enemy spawner, power-up manager, etc.)

---

### Level-Specific UI

Levels can load custom UI pages:

```cpp
void MyLevel::Start() {
    // Load level-specific HTML page
    UISystem::Instance()->LoadPage("my_level.html");
    
    // Bind UI callbacks
    UISystem::Instance()->BindMethod("onButtonClick", 
        new BoundMethodT<MyLevel>(this, &MyLevel::OnButtonClick));
}

void MyLevel::OnButtonClick(const UIDataBuffer& args) {
    // Handle button click from UI
    DIA_LOG("MyLevel", "Button clicked");
}
```

**Use Case:** Level-specific HUD, menus, overlays

---

## Level Lifecycle Diagram

See [level-lifecycle.mmd](diagrams/level-lifecycle.mmd) for visual representation:

```
Unloaded → Loading → Loaded → Starting → Running → Stopping → Unloaded
               ↑                   ↓
               └─── Factory ───────┘
```

**States:**
1. **Unloaded** - Level not in memory
2. **Loading** - Factory creating instance
3. **Loaded** - Instance created, not initialized
4. **Starting** - Start() executing (loading resources)
5. **Running** - Update() called each frame
6. **Stopping** - Stop() executing (cleanup)
7. **Unloaded** - Instance destroyed by factory

---

## Best Practices

### ✅ Do:

1. **Keep levels self-contained**
   - All level-specific code in level directory
   - Don't leak level state to global scope

2. **Initialize in Start(), cleanup in Stop()**
   ```cpp
   void Start() { LoadResources(); }
   void Stop() { UnloadResources(); }
   ```

3. **Use factory pattern**
   - Always register levels via LevelFactory
   - Don't create levels directly with `new`

4. **Log level lifecycle events**
   ```cpp
   DIA_LOG("MyLevel", "Starting with difficulty %d", mDifficulty);
   ```

5. **Handle level switching gracefully**
   - Save state before stopping
   - Clean up all resources in Stop()

### ❌ Don't:

1. **Create global level state**
   ```cpp
   // BAD: Global level variable
   MyLevel* gCurrentLevel;
   ```

2. **Leak resources**
   ```cpp
   // BAD: Allocate in Start(), forget to delete in Stop()
   void Start() { mTexture = new Texture(); }
   void Stop() { /* Forgot to delete mTexture! */ }
   ```

3. **Access other levels directly**
   ```cpp
   // BAD: Direct access to other level
   OtherLevel* otherLevel = GetOtherLevel();  // Coupling
   ```

4. **Store ProcessingUnit/Phase pointers**
   ```cpp
   // BAD: Storing phase pointer (can become invalid)
   mPhase = GetCurrentPhase();
   ```

---

## Summary

The Level System provides:
- ✅ **Factory pattern** for runtime level creation
- ✅ **Pluggable architecture** (easy to add levels)
- ✅ **Level-specific phases, modules, and UI**
- ✅ **Clean lifecycle** (Start → Update → Stop)
- ✅ **Examples** (DummyStage, UnitTestLevel)

**Key Components:**
- `ILevel` - Level interface
- `LevelFactory` - Level registry and factory
- `LevelFactoryModule` - Module wrapper for factory access

**Creating a Level:**
1. Create directory structure
2. Implement ILevel interface
3. Register in MainBootPhase
4. Add to Visual Studio project
5. Build and run

**[→ Back to Architecture Overview](architecture.md)**