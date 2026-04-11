# Level Lifecycle Diagram

This state diagram shows how game levels are loaded, initialized, executed, and unloaded within the Cluiche engine:

- **Unloaded** - Level not yet created
- **Loading** - Level being instantiated by factory
- **Loaded** - Level constructed but not initialized
- **Starting** - Level initialization and resource loading
- **Running** - Main execution with per-frame updates
- **Stopping** - Cleanup and resource unloading

```mermaid
stateDiagram-v2
    [*] --> Unloaded: Application Starts

    Unloaded --> Loading: LevelFactory::Create()
    Loading --> Loaded: Constructor Complete
    Loaded --> Starting: Level::Start() Called
    Starting --> Running: Initialization Complete
    Running --> Running: Level::Update() per frame
    Running --> Stopping: Level::Stop() Called
    Stopping --> Unloaded: Cleanup Complete

    Unloaded --> [*]: Application Shutdown

    state Loading {
        [*] --> FactoryLookup
        FactoryLookup --> Constructor: Found in Registry
        Constructor --> PhaseSetup: Level Instance Created
        PhaseSetup --> UISetup: Configure Level Phases
        UISetup --> [*]: Configure Level UI Pages

        note right of FactoryLookup
            LevelFactory maintains
            registry of level types
        end note

        note right of Constructor
            Example:
            DummyLevel()
            UnitTestLevel()
        end note
    }

    state Loaded {
        [*] --> Idle
        Idle --> [*]: Awaiting Start()

        note right of Idle
            Level constructed but
            not yet initialized
        end note
    }

    state Starting {
        [*] --> InitializePhases
        InitializePhases --> InitializeModules: Setup Level Phases
        InitializeModules --> InitializeUI: Setup Level Modules
        InitializeUI --> LoadResources: Setup Level UI
        LoadResources --> [*]: Load Level Assets

        note right of InitializePhases
            Level can add custom
            phases to ProcessingUnit
        end note

        note right of InitializeModules
            Level can add custom
            modules to phases
        end note
    }

    state Running {
        [*] --> UpdateGame
        UpdateGame --> UpdatePhysics: Game Logic
        UpdatePhysics --> UpdateGraphics: Physics Simulation
        UpdateGraphics --> UpdateUI: Render Commands
        UpdateUI --> UpdateGame: UI Updates

        note right of UpdateGame
            Called every frame
            from SimProcessingUnit
        end note

        note right of UpdateGraphics
            Writes to
            SimToRenderFrameStream
        end note
    }

    state Stopping {
        [*] --> StopPhases
        StopPhases --> StopModules: Shutdown Level Phases
        StopModules --> StopUI: Shutdown Level Modules
        StopUI --> UnloadResources: Shutdown Level UI
        UnloadResources --> Destructor: Unload Level Assets
        Destructor --> [*]: Delete Level Instance

        note right of Destructor
            Level instance
            deleted by factory
        end note
    }

    note left of Unloaded
        Level Registry:
        - DummyLevel
        - UnitTestLevel
        - Custom levels...
    end note

    note right of Running
        Level owns:
        - Custom phases
        - Custom modules
        - Custom UI pages
        - Level-specific data
    end note
```

[← Back to Architecture Overview](../architecture.md)
