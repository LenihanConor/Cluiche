# Integration Testing

**Last Updated:** 2026-04-01

Integration testing approach for cross-component interactions in Cluiche.

---

## Overview

Integration tests verify that multiple components work together correctly.

**Scope:**
- Cross-module interactions
- Cross-thread communication
- System-level behavior
- End-to-end workflows

**Related Documents:**
- **[→ Testing Strategy](test.md)** - Overall testing approach
- **[→ Unit Testing](unit-testing.md)** - Component-level testing
- **[→ Thread Safety Testing](thread-safety-testing.md)** - Concurrency testing

---

## Integration Test Levels

### Level 1: Module Integration

**Test modules working together within a phase.**

**Example: Module Dependencies**
```cpp
void TestModuleDependencyResolution()
{
    // Arrange: Create modules with dependencies
    MainKernelModule* kernel = new MainKernelModule();
    LevelFactoryModule* levelFactory = new LevelFactoryModule();
    
    levelFactory->AddDependency(kernel->GetId());
    
    Phase* phase = new MainBootPhase();
    phase->AddModule(levelFactory);
    phase->AddModule(kernel);  // Order doesn't matter
    
    // Act: Resolve dependencies
    phase->ResolveDependencies();
    
    // Assert: Kernel initialized before LevelFactory
    phase->Start();
    DIA_ASSERT(kernel->IsStarted(), "Kernel started first");
    DIA_ASSERT(levelFactory->IsStarted(), "LevelFactory started second");
}
```

---

### Level 2: Phase Integration

**Test phase transitions and module persistence.**

**Example: Phase Transitions**
```cpp
void TestPhaseTransitions()
{
    // Arrange: Create processing unit with phases
    TestProcessingUnit* pu = new TestProcessingUnit();
    
    BootPhase* boot = new BootPhase();
    RunningPhase* running = new RunningPhase();
    
    pu->AddPhase(boot);
    pu->AddPhase(running);
    
    // Act: Transition Boot → Running
    pu->Start(boot);
    pu->Update();  // Boot phase runs
    pu->TransitionTo(running);
    
    // Assert: Running phase active
    DIA_ASSERT(pu->GetCurrentPhase() == running, "Transitioned to Running");
    DIA_ASSERT(boot->IsStopped(), "Boot phase stopped");
    DIA_ASSERT(running->IsRunning(), "Running phase started");
}
```

---

### Level 3: Thread Integration

**Test cross-thread communication.**

**Example: Input Flow (Main → Sim)**
```cpp
void TestInputPipeline()
{
    // Arrange: Create Main and Sim threads
    MainProcessingUnit* mainPU = new MainProcessingUnit();
    SimProcessingUnit* simPU = new SimProcessingUnit();
    
    FrameStream<InputEvent> inputStream;
    
    // Act: Main thread polls input
    InputEvent keyPress;
    keyPress.type = InputEventType::KeyPressed;
    keyPress.key.code = KeyCode::Space;
    
    inputStream.Write(keyPress);  // Main writes
    
    // Sim thread reads
    InputEvent received;
    bool hasEvent = inputStream.Read(received);
    
    // Assert: Event received
    DIA_ASSERT(hasEvent, "Sim received event");
    DIA_ASSERT(received.type == InputEventType::KeyPressed, "Correct type");
    DIA_ASSERT(received.key.code == KeyCode::Space, "Correct key");
}
```

---

### Level 4: System Integration

**Test full application scenarios.**

**Example: Level Transitions**
```cpp
void TestLevelTransition()
{
    // Arrange: Create application
    Application* app = new Application();
    app->Initialize();
    
    // Register levels
    LevelFactory::Instance().Register<MainMenuLevel>();
    LevelFactory::Instance().Register<GameLevel>();
    
    // Act: Load MainMenu
    app->LoadLevel(StringCRC("MainMenuLevel"));
    app->Update();
    
    DIA_ASSERT(app->GetCurrentLevel()->GetId() == StringCRC("MainMenuLevel"), 
               "MainMenu loaded");
    
    // Transition to Game
    app->LoadLevel(StringCRC("GameLevel"));
    app->Update();
    
    // Assert: GameLevel loaded, MainMenu unloaded
    DIA_ASSERT(app->GetCurrentLevel()->GetId() == StringCRC("GameLevel"),
               "GameLevel loaded");
    
    // Cleanup
    app->Shutdown();
}
```

---

## Common Integration Test Scenarios

### Scenario 1: Module Lifecycle

**Test: Module Start/Update/Stop**

```cpp
void TestModuleLifecycle()
{
    Phase* phase = new TestPhase();
    TestModule* module = new TestModule();
    
    phase->AddModule(module);
    
    // Construct
    phase->Construct();
    DIA_ASSERT(module->IsConstructed(), "Module constructed");
    
    // Start
    phase->Start();
    DIA_ASSERT(module->IsStarted(), "Module started");
    
    // Update
    phase->Update();
    DIA_ASSERT(module->GetUpdateCount() > 0, "Module updated");
    
    // Stop
    phase->Stop();
    DIA_ASSERT(module->IsStopped(), "Module stopped");
    
    // Destruct
    phase->Destruct();
    DIA_ASSERT(module->IsDestructed(), "Module destructed");
}
```

---

### Scenario 2: FrameStream Communication

**Test: Producer-Consumer Pattern**

```cpp
void TestFrameStreamCommunication()
{
    FrameStream<int> stream;
    
    // Producer writes
    stream.Write(1);
    stream.Write(2);
    stream.Write(3);
    
    // Consumer reads
    int value;
    DIA_ASSERT(stream.Read(value), "Read first");
    DIA_ASSERT(value == 1, "First value");
    
    DIA_ASSERT(stream.Read(value), "Read second");
    DIA_ASSERT(value == 2, "Second value");
    
    DIA_ASSERT(stream.Read(value), "Read third");
    DIA_ASSERT(value == 3, "Third value");
    
    // No more data
    DIA_ASSERT(!stream.Read(value), "No more data");
}
```

---

### Scenario 3: Level Loading

**Test: Level Load/Unload**

```cpp
void TestLevelLoading()
{
    LevelFactory& factory = LevelFactory::Instance();
    
    // Register level
    factory.Register<TestLevel>();
    
    // Create level
    ILevel* level = factory.Create(StringCRC("TestLevel"));
    DIA_ASSERT(level != nullptr, "Level created");
    
    // Load
    level->Load();
    DIA_ASSERT(level->IsLoaded(), "Level loaded");
    
    // Unload
    level->Unload();
    DIA_ASSERT(!level->IsLoaded(), "Level unloaded");
    
    // Cleanup
    delete level;
}
```

---

### Scenario 4: Graphics Pipeline

**Test: Render Thread Flow**

```cpp
void TestRenderPipeline()
{
    // Arrange: Create render components
    DiaSFMLRenderWindow* window = new DiaSFMLRenderWindow();
    RenderCanvasModule* canvas = new RenderCanvasModule(window);
    
    // Act: Render frame
    window->Clear(Color::Black);
    canvas->DrawTestScene();  // Draw primitives
    window->Display();
    
    // Assert: No crashes (visual verification)
    DIA_ASSERT(window->IsOpen(), "Window still open");
    
    // Cleanup
    window->Close();
    delete canvas;
    delete window;
}
```

---

## Test Infrastructure

### UnitTestLevel

**In-engine integration test harness.**

```cpp
class UnitTestLevel : public ILevel
{
public:
    void Load() override
    {
        RunIntegrationTests();
    }
    
private:
    void RunIntegrationTests()
    {
        DIA_LOG("=== Integration Tests ===");
        
        TestModuleIntegration();
        TestPhaseTransitions();
        TestInputPipeline();
        TestLevelTransitions();
        
        ReportResults();
    }
};
```

---

## Best Practices

### Do's ✅

- **Test interfaces** - Verify contracts between components
- **Test happy path** - Normal operation first
- **Test error paths** - Component failures, invalid input
- **Test boundaries** - Edge cases at integration points
- **Use real components** - Minimize mocking for integration tests

### Don'ts ❌

- **Don't test implementation** - Focus on interaction contracts
- **Don't make brittle tests** - Test behavior, not internal structure
- **Don't ignore timing** - Account for asynchronous operations
- **Don't skip cleanup** - Always teardown properly

---

## Coverage Goals

| Subsystem | Target | Priority |
|-----------|--------|----------|
| Module System | 80% | P0 |
| Phase Transitions | 80% | P0 |
| FrameStream | 90% | P0 |
| Level System | 70% | P1 |
| Input Pipeline | 70% | P1 |
| Render Pipeline | 50% | P2 |

---

## Summary

**Integration Testing:**
- Verify cross-component interactions
- Test at module, phase, thread, and system levels
- Use real components (minimize mocking)
- UnitTestLevel for in-engine testing

**Key Scenarios:**
- Module lifecycle
- FrameStream communication
- Level loading/unloading
- Phase transitions
- Graphics pipeline

**[→ Testing Strategy](test.md)**  
**[→ Unit Testing](unit-testing.md)**  
**[→ Thread Safety Testing](thread-safety-testing.md)**
