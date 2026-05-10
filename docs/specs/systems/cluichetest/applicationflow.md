# System Spec: CluicheTest Application Flow

## Parent Application
@docs/specs/applications/cluichetest.md

## Research
@docs/research/diapp_simplif/summary.md

## Purpose

CluicheTest Application Flow defines how CluicheTest uses DiaApplicationFlow to structure its multi-threaded execution. It specifies the three ProcessingUnits (Main, Sim, Render), their modules, stages, and inter-PU streams. This is the concrete application of the framework — where DiaApplicationFlow is generic infrastructure, this system defines CluicheTest's specific topology.

## Responsibilities

- Define CluicheTest's ProcessingUnit topology (Main, Sim, Render)
- Define stream connections between PUs (InputToSim, SimToRender)
- Define Boot stage (infrastructure modules, auto-advance)
- Define DummyStage (first game stage with rendering and sim logic)
- Define patterns for adding future stages (StupidStage, etc.)
- Implement concrete modules: KernelModule, TimeServerModule, InputStreamModule, LevelModules, RenderModule, LoadingScreenModule, AssetServiceModule
- Demonstrate the DiaApplicationFlow framework in a real application

## Non-Responsibilities

- **Framework infrastructure** — DiaApplicationFlow handles lifecycle, transitions, validation
- **Asset pipeline** — separate system (asset-pipeline.md)
- **Game content/logic** — level-specific behavior belongs in level modules
- **Editor integration** — DiaApplicationEditor handles manifest editing

## Architecture

### Processing Units

| PU | Thread | Frequency | Role |
|----|--------|-----------|------|
| MainPU | Main (non-dedicated) | 30 Hz | Infrastructure: window, input collection, asset service, UI |
| SimPU | Dedicated | 30 Hz | Game simulation: time, input consumption, level logic, physics |
| RenderPU | Dedicated | 60 Hz | Rendering: reads frame data from SimToRender, draws to canvas |

**Startup order:** MainPU → SimPU → RenderPU (config array order).

### Streams

| Stream | Type | From | To | Purpose |
|--------|------|------|-----|---------|
| InputToSim | EventStream\<InputEvent\> | MainPU | SimPU | Keyboard/mouse events from window to sim |
| SimToRender | FrameStream\<FrameData\> | SimPU | RenderPU | Draw commands from sim to renderer |
| SimToUI | EventStream\<UICommand\> | SimPU | MainPU | Sim-driven UI updates (HUD, score, text) |

### Stages

| Stage | Trigger | PUs affected | Purpose |
|-------|---------|-------------|---------|
| Boot | auto (all modules kReady) | All | Load infrastructure, verify systems |
| DummyStage | auto (after Boot) | Sim, Render | First game level — sprites, physics, input handling |
| (Future stages) | manual (game logic) | Sim, Render | Additional levels/modes |

### Module Map

#### MainPU Modules (all stages = "all")

| Module | Type | Dependencies | Streams | Purpose |
|--------|------|--------------|---------|---------|
| Logger | LoggerModule | none | — | DiaLogger setup |
| Kernel | KernelModule | Logger | writes: InputToSim | Window, input source, event dispatch, calls RequestShutdown() on window close |
| AssetService | AssetServiceModule | Logger | — | Asset loading coordination |
| UIModule | UIModule | Kernel | reads: SimToUI | UI system management, consumes sim-driven UI commands |

#### SimPU Modules

| Module | Type | Stages | Dependencies | Streams | Purpose |
|--------|------|--------|--------------|---------|---------|
| TimeServer | TimeServerModule | all | none | — | Frame time, game clock |
| InputStream | InputStreamModule | all | TimeServer | reads: InputToSim | Consume input events for sim |
| LoadingScreen | LoadingScreenModule | all | none | writes: SimToRender | Render loading UI during transitions |
| DummyLevel | DummyLevelModule | DummyStage | TimeServer, InputStream | writes: SimToRender, SimToUI | Game logic for DummyStage |

#### RenderPU Modules

| Module | Type | Stages | Dependencies | Streams | Purpose |
|--------|------|--------|--------------|---------|---------|
| Renderer | RenderModule | all | none | reads: SimToRender | Fetch latest frame, draw to canvas |

### Manifest Structure

```
cluichetest.diagame
├── imports: cluiche.diaapp (type: manifest) — defines all PUs, streams, Boot modules
└── imports: dummy_stage.diastage (type: stage) — adds DummyStage modules to SimPU
```

**cluiche.diaapp** — single merged manifest:
```json
{
    "version": 2,
    "stages": ["Boot", "DummyStage"],
    "initial_stage": "Boot",
    "auto_stages": ["Boot", "DummyStage"],
    "streams": [
        { "id": "InputToSim", "type": "EventStream<InputEvent>", "from": "MainPU", "to": "SimPU" },
        { "id": "SimToRender", "type": "FrameStream<FrameData>", "from": "SimPU", "to": "RenderPU" },
        { "id": "SimToUI", "type": "EventStream<UICommand>", "from": "SimPU", "to": "MainPU" }
    ],
    "processing_units": [
        {
            "instance_id": "MainPU",
            "frequency_hz": 30,
            "dedicated_thread": false,
            "modules": [
                { "instance_id": "Logger", "type_id": "LoggerModule", "stages": ["all"], "dependencies": [] },
                { "instance_id": "Kernel", "type_id": "KernelModule", "stages": ["all"], "dependencies": ["Logger"], "writes": ["InputToSim"] },
                { "instance_id": "AssetService", "type_id": "AssetServiceModule", "stages": ["all"], "dependencies": ["Logger"] },
                { "instance_id": "UI", "type_id": "UIModule", "stages": ["all"], "dependencies": ["Kernel"], "reads": ["SimToUI"] }
            ]
        },
        {
            "instance_id": "SimPU",
            "frequency_hz": 30,
            "dedicated_thread": true,
            "modules": [
                { "instance_id": "TimeServer", "type_id": "TimeServerModule", "stages": ["all"], "dependencies": [] },
                { "instance_id": "InputStream", "type_id": "InputStreamModule", "stages": ["all"], "dependencies": ["TimeServer"], "reads": ["InputToSim"] },
                { "instance_id": "LoadingScreen", "type_id": "LoadingScreenModule", "stages": ["all"], "dependencies": [], "writes": ["SimToRender"] }
            ]
        },
        {
            "instance_id": "RenderPU",
            "frequency_hz": 60,
            "dedicated_thread": true,
            "modules": [
                { "instance_id": "Renderer", "type_id": "RenderModule", "stages": ["all"], "dependencies": [], "reads": ["SimToRender"] }
            ]
        }
    ]
}
```

**dummy_stage.diastage** adds DummyStage modules:
```json
{
    "name": "DummyStage",
    "manifest": "dummy_stage.diaapp"
}
```

**dummy_stage.diaapp** (stage-specific modules merged into SimPU):
```json
{
    "version": 2,
    "processing_units": [
        {
            "instance_id": "SimPU",
            "modules": [
                {
                    "instance_id": "DummyLevel",
                    "type_id": "DummyLevelModule",
                    "stages": ["DummyStage"],
                    "dependencies": ["TimeServer", "InputStream"],
                    "reads": ["InputToSim"],
                    "writes": ["SimToRender", "SimToUI"],
                    "start_timeout_ms": 5000,
                    "stop_timeout_ms": 2000
                }
            ]
        }
    ]
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Main PU Modules | KernelModule (window/input), LoggerModule, AssetServiceModule, UIModule | [main-pu-modules.md](../../features/cluichetest/applicationflow/main-pu-modules.md) | Approved |
| Sim PU Modules | TimeServerModule, InputStreamModule, LoadingScreenModule | [sim-pu-modules.md](../../features/cluichetest/applicationflow/sim-pu-modules.md) | Approved |
| Render PU Module | RenderModule (fetch SimToRender, draw to canvas) | [render-pu-module.md](../../features/cluichetest/applicationflow/render-pu-module.md) | Approved |
| DummyStage Level | DummyLevelModule — game logic, texture rendering, input handling | [dummy-stage-level.md](../../features/cluichetest/applicationflow/dummy-stage-level.md) | Approved |
| Manifest Configuration | cluiche.diaapp + dummy_stage.diastage + dummy_stage.diaapp | [manifest-configuration.md](../../features/cluichetest/applicationflow/manifest-configuration.md) | Approved |

## Platform Primitives Used

- **DiaApplicationFlow** — Application, ProcessingUnit, Module, ModuleRef, StreamReader/Writer, Stages
- **DiaCore/Frame** — FrameStream<FrameData> (underlying stream impl)
- **DiaGraphics** — ICanvas, FrameData, draw commands
- **DiaWindow** — Window creation and management
- **DiaInput** — InputSourceManager, keyboard/mouse events
- **DiaUI** — UI system for HUD/menus
- **DiaSFML** — SFML backend
- **DiaAssetRuntime** — Asset loading in DoStart
- **DiaLogger** — Logging

## Dependencies on Other Systems

**Required:**
- **DiaApplicationFlow** — Framework this system is built on
- **DiaGraphics** — Rendering abstractions
- **DiaWindow** — Window management
- **DiaInput** — Input events
- **DiaAssetRuntime** — Asset loading during stage transitions
- **DiaLogger** — Logging

**Sibling Systems:**
- **Asset Pipeline** — Provides deployed assets that modules load
- **CluicheTestScenarios** — E2E tests that exercise this application flow

## Out of Scope

- **Framework internals** — DiaApplicationFlow handles lifecycle, transitions
- **Asset pipeline configuration** — separate system
- **Editor** — DiaApplicationFlowEditor handles manifest editing
- **E2E test definitions** — CluicheTestScenarios system

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Three PUs: Main (infra), Sim (game logic), Render (drawing) | Proven threading model. Main collects input + manages window. Sim runs game. Render draws. Clear ownership. | All features | Accepted | Yes |
| SD-002 | Main never changes stage — only Sim and Render do | Main owns infrastructure (window, input, assets) that's always needed. Stage switching is a game-content concern. | All features | Accepted | Yes |
| SD-003 | Boot auto-advances to first game stage | Boot exists only to verify infrastructure is ready. No user interaction needed during boot. | Manifest | Accepted | Yes |
| SD-004 | LoadingScreen is a retained sim module that writes to SimToRender | During transitions, Sim is stopping/starting game modules. LoadingScreen keeps rendering so RenderPU isn't starved. No framework-level concept needed. | Sim PU | Accepted | Yes |
| SD-005 | Stage-specific modules come from .diastage files | Keeps the base manifest clean (infra only). Each stage is independently addable/removable. | Manifest | Accepted | Yes |
| SD-006 | DummyStage auto-advances from Boot (first stage = auto) | For the demo/testbed, boot straight into gameplay. Future stages use manual transition (game logic triggers). | Manifest | Accepted | No |
| SD-007 | All modules log at DoStart entry/exit and DoStop entry/exit | Makes stage transitions traceable in logs — see exactly which modules are starting/stopping and in what order. Uses DiaLogger Application channel. | All features | Accepted | Yes |
| SD-008 | Canvas/Window created pre-Application in Main.cpp, passed as bootstrap resource | Canvas is shared across PUs (Render draws, UI renders). Not owned by any module. Framework exposes it to modules that request it — avoids singleton or cross-PU ModuleRef. | All features | Accepted | Yes |
| SD-009 | LoadingScreen wins via update order: LoadingScreen updates before level modules; level module overwrites with latest frame when active | FrameStream keeps latest write. Same thread (SimPU), sequential update. During transitions LoadingScreen is only writer. No conditional logic needed — just config ordering. | Sim PU | Accepted | Yes |
| SD-010 | KernelModule calls RequestShutdown() on window close — no "quitting" stage | Testbed doesn't need graceful save/cleanup stage. Immediate shutdown is correct for CluicheTest. Real games would add a shutdown stage. | All features | Accepted | No |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for IDs | All module instance IDs, stage names, stream IDs are StringCRC |
| PD-004 | Platform | No STL in public APIs | Module interfaces use DiaCore containers |
| PD-007 | Platform | C++20 | Module code uses C++20 features |
| PD-010 | Platform | .diagame root, .diastage for stages | File hierarchy: cluichetest.diagame → cluiche.diaapp + dummy_stage.diastage |
| AD-001 (CluicheTest) | App | Three ProcessingUnits (Main/Render/Sim) | Maintained — same three PUs |
| AD-003 (CluicheTest) | App | Entry point in Main.cpp | Main.cpp creates Application from manifest, calls Start/Update/Stop |
| SD-001 (DiaAppFlow) | System | Config is sole source of truth | All PU/module/stream/stage wiring in .diaapp manifest |
| SD-002 (DiaAppFlow) | System | Stages replace Phases | No Phase subclasses — stages in config |
| SD-003 (DiaAppFlow) | System | ModuleRef<T> only | All module access via ModuleRef<T> |
| SD-004 (DiaAppFlow) | System | App-wide TransitionTo | Stage transitions affect all PUs simultaneously |
| SD-005 (DiaAppFlow) | System | Transitions at start of next frame | Queued, async |
| SD-006 (DiaAppFlow) | System | Streams in config, framework-owned | InputToSim and SimToRender declared in manifest |
| SD-013 (DiaAppFlow) | System | DoStart returns StartResult | Modules use kLoading for async asset loads |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Streams | Should InputToSim carry raw SFML events or engine-abstracted InputEvents? | Engine-abstracted InputEvents (from DiaInput). Keeps SFML coupling in MainPU's KernelModule only. |
| 2 | Boot | What does Boot actually validate before auto-advancing? | All Boot-stage modules return kReady. For CluicheTest: Logger up, Kernel has window/canvas, AssetService initialized. |
| 3 | Render | Does RenderPU need to know the current stage? | No. It reads SimToRender and draws. Stage-agnostic. Whatever's written to the stream gets rendered. |
| 4 | DummyStage | Should DummyStage auto-advance from Boot or require explicit trigger? | Auto for CluicheTest (testbed — boot straight into demo). Future real games would use manual trigger after menu. |
| 5 | Future stages | What's the developer workflow for adding a new stage? | Create .diastage file, create stage .diaapp with modules, add import to .diagame, implement module classes, register with DIA_MODULE. |

## Status

`Approved` — 2026-05-08. Plan: [applicationflow.plan.md](applicationflow.plan.md)
