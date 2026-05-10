# Feature Spec: Main PU Modules

## Parent System
@docs/specs/systems/cluichetest/applicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Application Flow | @docs/specs/systems/cluichetest/applicationflow.md |
| Feature | Main PU Modules | (this file) |

## Problem Statement

MainPU needs four infrastructure modules that run on the main thread across all stages: Logger setup, window/input management, asset loading coordination, and UI system. These are the "always-on" foundation that every stage depends on.

## Acceptance Criteria

1. **LoggerModule** — Initializes DiaLogger with console + file sinks in DoStart, kReady immediately
2. **KernelModule** — Creates window and canvas in DoStart (may be kLoading if window creation is async), dispatches input events to InputToSim stream, calls RequestShutdown() on window close
3. **AssetServiceModule** — Initializes DiaAssetRuntime session in DoStart, handles stage load/unload requests, kReady when session established
4. **UIModule** — Initializes UI system (Ultralight), reads SimToUI EventStream and applies UI commands, kReady when UI system initialized
5. All four modules have `stages: ["all"]` — never stopped during transitions
6. All four log at DoStart entry/exit and DoStop entry/exit (per SD-007)
7. KernelModule receives canvas from BootstrapResources (per SD-008), does not create it
8. Dependency chain: Logger → Kernel → UIModule, Logger → AssetService

## Module Specifications

### LoggerModule

```cpp
class LoggerModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"LoggerModule"};
protected:
    StartResult DoStart() override;   // Init sinks, return kReady
    void DoUpdate(float dt) override; // Flush if needed
    StopResult DoStop() override;     // Flush final, return kDone
};
```

### KernelModule

```cpp
class KernelModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"KernelModule"};
protected:
    StartResult DoStart() override;   // Get canvas from bootstrap, create window, init input
    void DoUpdate(float dt) override; // Poll OS events, write InputToSim
    StopResult DoStop() override;     // Close window, return kDone
private:
    EventStreamWriter<InputEvent> mInputWriter{this, "InputToSim"};
    ModuleRef<LoggerModule> mLogger{this};
};
```

### AssetServiceModule

```cpp
class AssetServiceModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"AssetServiceModule"};
protected:
    StartResult DoStart() override;   // Init DiaAssetRuntime session, return kReady
    void DoUpdate(float dt) override; // Pump async load requests
    StopResult DoStop() override;     // Shutdown session, return kDone
private:
    ModuleRef<LoggerModule> mLogger{this};
};
```

### UIModule

```cpp
class UIModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"UIModule"};
protected:
    StartResult DoStart() override;   // Init Ultralight, return kReady
    void DoUpdate(float dt) override; // Consume SimToUI, update UI state, render
    StopResult DoStop() override;     // Shutdown UI, return kDone
private:
    EventStreamReader<UICommand> mUICommands{this, "SimToUI"};
    ModuleRef<KernelModule> mKernel{this};
};
```

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/CluicheTest/Modules/LoggerModule.h` | New |
| `Cluiche/CluicheTest/Modules/LoggerModule.cpp` | New |
| `Cluiche/CluicheTest/Modules/KernelModule.h` | New |
| `Cluiche/CluicheTest/Modules/KernelModule.cpp` | New |
| `Cluiche/CluicheTest/Modules/AssetServiceModule.h` | New |
| `Cluiche/CluicheTest/Modules/AssetServiceModule.cpp` | New |
| `Cluiche/CluicheTest/Modules/UIModule.h` | New |
| `Cluiche/CluicheTest/Modules/UIModule.cpp` | New |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Update |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Update |

## Dependencies

- **DiaApplicationFlow** — Module base class, ModuleRef, StreamWriter/Reader, DIA_MODULE
- **DiaLogger** — sink setup (LoggerModule)
- **DiaWindow** — window creation (KernelModule)
- **DiaInput** — InputSourceManager, InputEvent (KernelModule)
- **DiaAssetRuntime** — session management (AssetServiceModule)
- **DiaUI / Ultralight** — UI system (UIModule)

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — all kTypeId are StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — module APIs use Dia types |
| SD-001 (DiaAppFlow) | System | Config is sole source of truth | Compliant — modules don't self-register to PUs; manifest does |
| SD-003 (DiaAppFlow) | System | ModuleRef only | Compliant — all inter-module access via ModuleRef<T> |
| SD-007 (CT AppFlow) | System | Log at DoStart/DoStop | Compliant — all modules log lifecycle entry/exit |
| SD-008 (CT AppFlow) | System | Canvas from BootstrapResources | Compliant — KernelModule gets canvas from bootstrap, not creating it |
| SD-010 (CT AppFlow) | System | RequestShutdown on window close | Compliant — KernelModule implements this |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Kernel | Should KernelModule own the InputSourceManager or get it from bootstrap? | Own it. InputSourceManager is module-level state. Only the canvas/window (shared render target) comes from bootstrap. |
| 2 | UI | What UI commands does UIModule initially support from SimToUI? | For DummyStage: simple text overlay (score, FPS). UICommand is an enum+payload (ShowText, HideText, UpdateValue). Extensible later. |
| 3 | Assets | Should AssetServiceModule expose anything to other modules, or is it self-contained? | Self-contained at the module level. Other modules call DiaAssetRuntime APIs directly — AssetServiceModule just ensures the session is alive and pumps async work. |
| 4 | Shutdown | What if the window close happens mid-transition? | RequestShutdown is always valid. Application queues it and processes after current transition completes (or aborts transition). |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
