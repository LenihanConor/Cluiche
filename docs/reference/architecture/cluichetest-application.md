# CluicheTest Game Application Architecture

**Last Updated:** 2026-04-12

Detailed architecture of the CluicheTest game application, a demo game and testbed built on the Dia engine.

---

## Overview

**CluicheTest** (the game application, not the platform) is a multi-threaded demo game that showcases the Dia engine's Module/Phase/ProcessingUnit pattern. It provides a complete example of how to build a game with three independent threads and a pluggable level system. This application serves as both a demonstration of the engine's capabilities and as a testbed for validating engine features.

**Location:** `Cluiche/CluicheTest/`

**Key Features:**
- Three-thread architecture (Main, Render, Sim)
- Phase-based execution model
- Pluggable level system via factory pattern
- Cross-thread communication via FrameStreams
- Web-based UI system

---

## Application Entry Point

### Main.cpp

**Location:** `Cluiche/CluicheTest/Main.cpp`

**Entry Point:**
```cpp
int main() {
    // Create main thread processing unit
    Cluiche::MainProcessingUnit mainPU;
    
    // Initialize phases
    mainPU.Start();
    
    // Main update loop
    mainPU.Update();
    
    // Shutdown
    mainPU.Stop();
    
    return 0;
}
```

**Execution Flow:**
1. Create `MainProcessingUnit` (configures initial phase)
2. `Start()` - Initializes `MainBootPhase`
3. `Update()` - Runs main loop (blocks until exit)
4. `Stop()` - Cleans up and shuts down

**Simple Design:** Main.cpp has minimal logic; all functionality in ProcessingUnits, Phases, and Modules.

---

## Processing Units

### MainProcessingUnit

**Location:** `Cluiche/CluicheTest/ApplicationFlow/ProcessingUnits/MainProcessingUnit.h`

**Purpose:** Bootstrap application, coordinate UI, spawn worker threads

**Phases:**
1. **MainBootPhase** - Initialize core systems, register levels
2. **MainBootStrapPhase** - Show launch UI, spawn threads

**Initialization:**
```cpp
class MainProcessingUnit : public Dia::Application::ProcessingUnit {
public:
    MainProcessingUnit()
        : mBootPhase(this, &mBootStrapPhase)
        , mBootStrapPhase(this) 
    {
        // Start with boot phase
        TransitionPhase(&mBootPhase);
    }

private:
    MainBootPhase mBootPhase;
    MainBootStrapPhase mBootStrapPhase;
};
```

**Key Responsibilities:**
- Create and configure phases
- Pass shared state to spawned threads (mRunning flag, FrameStreams)
- Coordinate shutdown (sets mRunning = false)

---

### RenderProcessingUnit

**Location:** `Cluiche/CluicheTest/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h`

**Purpose:** Render graphics at consistent 60 FPS

**Phases:**
1. **RenderRunningPhase** - Main rendering loop

**Initialization:**
```cpp
class RenderProcessingUnit : public Dia::Application::ProcessingUnit {
public:
    RenderProcessingUnit(StartData* startData)
        : mStartData(startData)
        , mRunningPhase(this, startData)
    {
        // Frequency limiter @ 60 FPS
        SetThreadLimiter(new TimeThreadLimiter(60.0f));
        
        // Start with running phase
        TransitionPhase(&mRunningPhase);
    }

private:
    StartData* mStartData;
    RenderRunningPhase mRunningPhase;
};
```

**StartData:**
```cpp
struct StartData {
    bool* mRunning;                                    // Shared running flag
    SimToRenderFrameStream* mSimToRenderFrameStream;  // Graphics commands
    MainKernelModule* mMainKernel;                    // Window/Canvas
};
```

**Key Responsibilities:**
- Read graphics commands from `SimToRenderFrameStream`
- Render to SFML canvas via `MainKernelModule`
- Maintain 60 FPS via `TimeThreadLimiter`

---

### SimProcessingUnit

**Location:** `Cluiche/CluicheTest/ApplicationFlow/ProcessingUnits/SimProcessingUnit.h`

**Purpose:** Run game simulation at variable rate

**Phases:**
1. **SimBootPhase** - Initialize simulation modules
2. **SimBootStrapPhase** - Main simulation loop

**Initialization:**
```cpp
class SimProcessingUnit : public Dia::Application::ProcessingUnit {
public:
    SimProcessingUnit(StartData* startData)
        : mStartData(startData)
        , mBootPhase(this, &mBootStrapPhase, startData)
        , mBootStrapPhase(this, startData)
    {
        // No frequency limiter (runs as fast as possible)
        TransitionPhase(&mBootPhase);
    }

private:
    StartData* mStartData;
    SimBootPhase mBootPhase;
    SimBootStrapPhase mBootStrapPhase;
};
```

**StartData:**
```cpp
struct StartData {
    bool* mRunning;                                    // Shared running flag
    InputToSimFrameStream* mInputToSimFrameStream;    // Input events
    SimToRenderFrameStream* mSimToRenderFrameStream;  // Graphics commands
    MainUIModule* mMainUIModule;                      // UI access (cross-thread)
};
```

**Key Responsibilities:**
- Read input events from `InputToSimFrameStream`
- Update game logic and physics
- Write graphics commands to `SimToRenderFrameStream`
- Bridge to UI via `SimUIProxyModule`

---

## Phases

### MainBootPhase

**Location:** `Cluiche/CluicheTest/ApplicationFlow/Phases/MainBootPhase.h`

**Purpose:** Initialize core systems and register levels

**Modules:**
- `MainKernelModule` - Time, input, window, canvas
- `LevelFactoryModule` - Level registry access

**Lifecycle:**
```cpp
class MainBootPhase : public Dia::Application::Phase {
public:
    MainBootPhase(MainProcessingUnit* pu, MainBootStrapPhase* nextPhase)
        : mProcessingUnit(pu)
        , mNextPhase(nextPhase)
        , mMainKernelModule()
        , mLevelFactoryModule(&mMainKernelModule)
    {
        AddModule(&mMainKernelModule);
        AddModule(&mLevelFactoryModule);
    }

protected:
    void AfterModulesStart() override {
        // Register levels with factory
        LevelFactory& factory = LevelFactory::Instance();
        factory.Register<DummyLevel>("DummyLevel");
        factory.Register<UnitTestLevel>("UnitTestLevel");
        
        // Transition to bootstrap phase
        mProcessingUnit->TransitionPhase(mNextPhase);
    }

private:
    MainKernelModule mMainKernelModule;
    LevelFactoryModule mLevelFactoryModule;
    MainBootStrapPhase* mNextPhase;
};
```

**Key Actions:**
- Initialize time server @ 30Hz
- Create SFML window
- Create canvas for rendering
- Initialize input source manager
- Register all levels

---

### MainBootStrapPhase

**Location:** `Cluiche/CluicheTest/ApplicationFlow/Phases/MainBootStrapPhase.h`

**Purpose:** Show launch UI, spawn threads, coordinate application

**Modules (Retained):**
- `MainKernelModule` (from MainBootPhase)
- `LevelFactoryModule` (from MainBootPhase)

**Modules (New):**
- `MainUIModule` - UI system

**Lifecycle:**
```cpp
class MainBootStrapPhase : public Dia::Application::Phase {
public:
    MainBootStrapPhase(MainProcessingUnit* pu)
        : mProcessingUnit(pu)
        , mMainUIModule()
    {
        // Retain kernel and factory from boot phase
        RetainModule(&GetModule<MainKernelModule>());
        RetainModule(&GetModule<LevelFactoryModule>());
        
        // Add new UI module
        AddModule(&mMainUIModule);
    }

protected:
    void AfterModulesStart() override {
        // Spawn render thread
        StartData renderData = {
            &mRunning,
            &mSimToRenderFrameStream,
            &GetModule<MainKernelModule>()
        };
        mRenderThread = new std::thread(RenderProcessingUnit(renderData));
        
        // Spawn sim thread
        StartData simData = {
            &mRunning,
            &mInputToSimFrameStream,
            &mSimToRenderFrameStream,
            &mMainUIModule
        };
        mSimThread = new std::thread(SimProcessingUnit(simData));
    }

    void BeforeModulesUpdate() override {
        // Collect input and write to sim thread
        CollectInput();
    }

    void BeforeModulesStop() override {
        // Signal threads to stop
        mRunning = false;
        
        // Wait for threads to complete
        mRenderThread->join();
        mSimThread->join();
        
        delete mRenderThread;
        delete mSimThread;
    }

private:
    MainUIModule mMainUIModule;
    std::thread* mRenderThread;
    std::thread* mSimThread;
    bool mRunning = true;
    InputToSimFrameStream mInputToSimFrameStream;
    SimToRenderFrameStream mSimToRenderFrameStream;
};
```

**Key Actions:**
- Show launch UI (level selection)
- Spawn `RenderProcessingUnit` thread
- Spawn `SimProcessingUnit` thread
- Coordinate shutdown (join threads)

---

### RenderRunningPhase

**Location:** `Cluiche/CluicheTest/ApplicationFlow/Phases/RenderRunningPhase.h`

**Purpose:** Main rendering loop @ 60 FPS

**Modules:**
- `RenderKernel` - Frame management
- `RenderCanvas` - SFML rendering

**Lifecycle:**
```cpp
class RenderRunningPhase : public Dia::Application::Phase {
public:
    RenderRunningPhase(RenderProcessingUnit* pu, StartData* startData)
        : mProcessingUnit(pu)
        , mStartData(startData)
    {
        AddModule(&mRenderKernel);
        AddModule(&mRenderCanvas);
    }

protected:
    void BeforeModulesUpdate() override {
        // Check running flag
        if (!(*mStartData->mRunning)) {
            mProcessingUnit->Stop();
        }
    }

    void AfterModulesUpdate() override {
        // Read graphics from sim thread
        FrameData* frameData = mStartData->mSimToRenderFrameStream->PeekFrame();
        
        if (frameData) {
            // Clear canvas
            mStartData->mMainKernel->GetCanvas()->Clear();
            
            // Render frame data
            frameData->Render(mStartData->mMainKernel->GetCanvas());
            
            // Display
            mStartData->mMainKernel->GetCanvas()->Display();
        }
    }

private:
    StartData* mStartData;
};
```

**Key Actions:**
- Read `SimToRenderFrameStream` each frame
- Render graphics commands to canvas
- Display at 60 FPS (controlled by ProcessingUnit's TimeThreadLimiter)

---

### SimBootPhase

**Location:** `Cluiche/CluicheTest/ApplicationFlow/Phases/SimBootPhase.h`

**Purpose:** Initialize simulation modules

**Modules:**
- `SimTimeServerModule` - Independent simulation clock

**Lifecycle:**
```cpp
class SimBootPhase : public Dia::Application::Phase {
public:
    SimBootPhase(SimProcessingUnit* pu, SimBootStrapPhase* nextPhase, StartData* startData)
        : mProcessingUnit(pu)
        , mNextPhase(nextPhase)
        , mStartData(startData)
    {
        AddModule(&mSimTimeServerModule);
    }

protected:
    void AfterModulesStart() override {
        // Transition to bootstrap phase
        mProcessingUnit->TransitionPhase(mNextPhase);
    }

private:
    SimTimeServerModule mSimTimeServerModule;
    SimBootStrapPhase* mNextPhase;
};
```

**Key Actions:**
- Initialize simulation time server (independent from Main thread time)

---

### SimBootStrapPhase

**Location:** `Cluiche/CluicheTest/ApplicationFlow/Phases/SimBootStrapPhase.h`

**Purpose:** Main simulation loop

**Modules (Retained):**
- `SimTimeServerModule` (from SimBootPhase)

**Modules (New):**
- `SimInputFrameStreamModule` - Input consumer
- `SimUIProxyModule` - Cross-thread UI bridge

**Lifecycle:**
```cpp
class SimBootStrapPhase : public Dia::Application::Phase {
public:
    SimBootStrapPhase(SimProcessingUnit* pu, StartData* startData)
        : mProcessingUnit(pu)
        , mStartData(startData)
    {
        // Retain time server from boot phase
        RetainModule(&GetModule<SimTimeServerModule>());
        
        // Add new modules
        AddModule(&mSimInputFrameStreamModule);
        AddModule(&mSimUIProxyModule);
    }

protected:
    void BeforeModulesUpdate() override {
        // Check running flag
        if (!(*mStartData->mRunning)) {
            mProcessingUnit->Stop();
        }
        
        // Read input from main thread
        EventData* events = mStartData->mInputToSimFrameStream->ConsumeFrame();
        if (events) {
            ProcessInput(events);
        }
    }

    void AfterModulesUpdate() override {
        // Update current level
        if (mCurrentLevel) {
            mCurrentLevel->Update();
        }
        
        // Generate frame data
        FrameData frameData = GenerateFrameData();
        
        // Write to render thread
        mStartData->mSimToRenderFrameStream->PushFrame(frameData);
    }

private:
    SimInputFrameStreamModule mSimInputFrameStreamModule;
    SimUIProxyModule mSimUIProxyModule;
    ILevel* mCurrentLevel;
};
```

**Key Actions:**
- Read input from `InputToSimFrameStream`
- Update current level
- Update physics/logic
- Write graphics to `SimToRenderFrameStream`

---

## Core Modules

### MainKernelModule

**Location:** `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h`

**Purpose:** Provide core services (time, input, window, canvas)

**Services:**
```cpp
class MainKernelModule : public Dia::Application::Module {
public:
    // Service accessors
    Dia::Core::TimeServer* GetTimeServer() { return &mTimeServer; }
    Dia::Input::InputSourceManager* GetInputManager() { return &mInputManager; }
    Dia::Window::IWindow* GetWindow() { return mWindow; }
    Dia::Graphics::ICanvas* GetCanvas() { return mCanvas; }

private:
    void DoStart() override {
        // Initialize time @ 30Hz
        mTimeServer.SetFrequency(30.0f);
        mTimeServer.Start();
        
        // Create SFML window
        mWindow = Dia::SFML::RenderWindowFactory::Create();
        
        // Create canvas
        mCanvas = new Dia::Graphics::Canvas(mWindow);
        
        // Initialize input
        mInputManager.AddSource(new Dia::SFML::InputSource(mWindow));
    }

    void DoUpdate() override {
        mTimeServer.Update();
        mInputManager.Update();
    }

    void DoStop() override {
        delete mCanvas;
        delete mWindow;
    }

    Dia::Core::TimeServer mTimeServer;
    Dia::Input::InputSourceManager mInputManager;
    Dia::Window::IWindow* mWindow;
    Dia::Graphics::ICanvas* mCanvas;
};
```

**Used By:** Nearly all other modules depend on MainKernelModule

---

### MainUIModule

**Location:** `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/MainUIModule.h`

**Purpose:** UI system (observer subject)

**Implementation:**
```cpp
class MainUIModule : public Dia::Application::Module, public Dia::Core::ObserverSubject {
public:
    MainUIModule();
    
    Dia::UI::IUISystem* GetUISystem() { return mUISystem; }

private:
    void DoStart() override {
        // Initialize UI system
        mUISystem = new Dia::UI::Awesomium::UISystem();
        mUISystem->Initialize();
        
        // Load launch page
        mUISystem->LoadPage("launch.html");
        
        // Notify observers (SimUIProxyModule)
        Notify();
    }

    void DoUpdate() override {
        // Update UI system (process events)
        mUISystem->Update();
    }

    void DoStop() override {
        delete mUISystem;
    }

    Dia::UI::IUISystem* mUISystem;
};
```

**Observers:** `SimUIProxyModule` (on Sim thread)

---

### LevelFactoryModule

**Location:** `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h`

**Purpose:** Provide access to `LevelFactory` singleton

**Implementation:**
```cpp
class LevelFactoryModule : public Dia::Application::Module {
public:
    LevelFactoryModule(MainKernelModule* kernel)
        : mKernel(kernel)
    {
        RegisterDependency(mKernel);
    }
    
    LevelFactory& GetLevelFactory() { return LevelFactory::Instance(); }

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

**Simple Wrapper:** Provides dependency tracking for LevelFactory access

---

### SimTimeServerModule

**Location:** `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h`

**Purpose:** Independent simulation clock

**Implementation:**
```cpp
class SimTimeServerModule : public Dia::Application::Module {
public:
    Dia::Core::TimeServer* GetTimeServer() { return &mTimeServer; }

private:
    void DoStart() override {
        mTimeServer.Start();
    }

    void DoUpdate() override {
        mTimeServer.Update();
    }

    void DoStop() override {
        // Nothing
    }

    Dia::Core::TimeServer mTimeServer;
};
```

**Why Separate Time?** Sim thread has independent clock from Main thread (30Hz)

---

### SimInputFrameStreamModule

**Location:** `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h`

**Purpose:** Input event consumer

**Implementation:**
```cpp
class SimInputFrameStreamModule : public Dia::Application::Module {
public:
    SimInputFrameStreamModule(SimTimeServerModule* timeServer, InputToSimFrameStream* stream)
        : mTimeServer(timeServer)
        , mInputStream(stream)
    {
        RegisterDependency(mTimeServer);
    }

    EventData* GetLatestInput() { return mInputStream->ConsumeFrame(); }

private:
    void DoStart() override {
        // Nothing
    }

    void DoUpdate() override {
        // Input consumed in phase hooks
    }

    void DoStop() override {
        // Nothing
    }

    SimTimeServerModule* mTimeServer;
    InputToSimFrameStream* mInputStream;
};
```

**Access Pattern:** Phase reads input via `GetLatestInput()`

---

### SimUIProxyModule

**Location:** `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/SimUIProxyModule.h`

**Purpose:** Cross-thread UI bridge (observer)

**Implementation:**
```cpp
class SimUIProxyModule : public Dia::Application::Module, public Dia::Core::Observer {
public:
    SimUIProxyModule(SimTimeServerModule* timeServer, MainUIModule* mainUI)
        : mTimeServer(timeServer)
        , mMainUI(mainUI)
        , mUIReady(false)
    {
        RegisterDependency(mTimeServer);
        mMainUI->Attach(this);  // Observer pattern
    }

    void SendMessage(const char* msg) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mUIReady) {
            mMainUI->GetUISystem()->SendMessage(msg);
        }
    }

private:
    void OnNotify() override {
        std::lock_guard<std::mutex> lock(mMutex);
        mUIReady = true;
    }

    void DoStart() override {
        // Wait for UI ready notification
    }

    void DoUpdate() override {
        // Proxy is passive (message-based)
    }

    void DoStop() override {
        mMainUI->Detach(this);
    }

    SimTimeServerModule* mTimeServer;
    MainUIModule* mMainUI;
    std::mutex mMutex;
    bool mUIReady;
};
```

**Thread Safety:** Mutex protects UI access from Sim thread

---

## Level System

### LevelFactory

**Location:** `Cluiche/CluicheTest/CluicheKernel/LevelFactory.h`

**Purpose:** Registry and factory for levels

**Implementation:**
```cpp
class LevelFactory {
public:
    static LevelFactory& Instance();
    
    template<typename T>
    void Register(const char* name) {
        mRegistry[StringCRC(name)] = []() { return new T(); };
    }
    
    ILevel* Create(const char* name) {
        StringCRC crc(name);
        if (mRegistry.find(crc) != mRegistry.end()) {
            return mRegistry[crc]();
        }
        return nullptr;
    }

private:
    std::map<StringCRC, std::function<ILevel*()>> mRegistry;
};
```

**Registration:**
```cpp
// In MainBootPhase::AfterModulesStart()
LevelFactory::Instance().Register<DummyLevel>("DummyLevel");
LevelFactory::Instance().Register<UnitTestLevel>("UnitTestLevel");
```

**Creation:**
```cpp
// In SimBootStrapPhase (after user selects level)
ILevel* level = LevelFactory::Instance().Create("DummyLevel");
level->Start();
```

**[→ Level system details](level-system.md)**

---

## Data Flow

### Input Flow (Main → Sim)

```
1. Main thread: InputSourceManager collects SFML events
2. Main thread: Push to InputToSimFrameStream
3. Sim thread: ConsumeFrame from InputToSimFrameStream
4. Sim thread: Process input events
5. Sim thread: Update game logic based on input
```

### Graphics Flow (Sim → Render)

```
1. Sim thread: Update game state (positions, sprites, etc.)
2. Sim thread: Generate FrameData (graphics commands)
3. Sim thread: Push to SimToRenderFrameStream
4. Render thread: PeekFrame from SimToRenderFrameStream
5. Render thread: Render commands to SFML canvas
6. Render thread: Display @ 60 FPS
```

### UI Flow (Main ↔ Sim)

```
1. Main thread: MainUIModule initializes UI system
2. Main thread: Notify observers (SimUIProxyModule)
3. Sim thread: SimUIProxyModule receives notification (mutex)
4. Sim thread: SendMessage to UI via proxy (mutex-protected)
5. Main thread: UI processes message
```

**[→ Threading model details](threading-model.md)**

---

## Directory Structure

```
Cluiche/CluicheTest/
├── Main.cpp                                     # Entry point
├── Cluiche.vcxproj                             # Visual Studio project
├── Cluiche.vcxproj.filters                     # VS filters
│
├── ApplicationFlow/                             # Core application flow
│   ├── ProcessingUnits/                        # Thread orchestrators
│   │   ├── MainProcessingUnit.h/cpp
│   │   ├── RenderProcessingUnit.h/cpp
│   │   └── SimProcessingUnit.h/cpp
│   └── Phases/                                  # Phase implementations
│       ├── MainBootPhase.h/cpp
│       ├── MainBootStrapPhase.h/cpp
│       ├── RenderRunningPhase.h/cpp
│       ├── SimBootPhase.h/cpp
│       └── SimBootStrapPhase.h/cpp
│
├── CluicheKernel/                               # Core modules and systems
│   ├── LevelFactory.h/cpp                       # Level registry
│   └── ApplicationFlow/
│       ├── Modules/                             # Core modules
│       │   ├── MainKernelModule.h/cpp
│       │   ├── MainUIModule.h/cpp
│       │   ├── LevelFactoryModule.h/cpp
│       │   ├── SimTimeServerModule.h/cpp
│       │   ├── SimInputFrameStreamModule.h/cpp
│       │   └── SimUIProxyModule.h/cpp
│       └── Phases/
│           └── MainPhaseBase.h/cpp              # Base phase utilities
│
└── Levels/                                      # Level implementations
    ├── DummyLevel/
    │   ├── DummyLevel.h/cpp                     # Example level
    │   ├── LevelFlow/Phases/                    # Level-specific phases
    │   └── UI/                                   # Level-specific UI pages
    └── UnitTestLevel/
        ├── UnitTestLevel.h/cpp                  # Test harness level
        ├── LevelFlow/Phases/
        └── UI/
```

---

## Summary

Cluiche demonstrates a **production-ready multi-threaded game architecture** using Dia's Module/Phase/ProcessingUnit pattern:

**Key Components:**
- ✅ 3 ProcessingUnits (Main, Render, Sim threads)
- ✅ 6 Phases (Boot, BootStrap, Running across threads)
- ✅ 6 Core Modules (Kernel, UI, LevelFactory, TimeServer, InputFrame, UIProxy)
- ✅ Pluggable level system (DummyLevel, UnitTestLevel)
- ✅ Thread-safe communication (FrameStreams, Observer pattern)

**Architecture Highlights:**
- Clean separation of concerns (bootstrap, render, simulation)
- Explicit dependencies and lifecycle
- Testable modules
- Scalable design (easy to add levels, modules, phases)

**[→ Back to Architecture Overview](architecture.md)**