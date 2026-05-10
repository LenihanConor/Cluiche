# Feature Spec: Sim PU Modules

## Parent System
@docs/specs/systems/cluichetest/applicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Application Flow | @docs/specs/systems/cluichetest/applicationflow.md |
| Feature | Sim PU Modules | (this file) |

## Problem Statement

SimPU runs on a dedicated 30Hz thread and needs infrastructure modules that provide time, input consumption, and loading screen rendering — all running across all stages so that stage-specific level modules have a stable foundation.

## Acceptance Criteria

1. **TimeServerModule** — Provides frame deltaTime and total game clock. kReady immediately.
2. **InputStreamModule** — Reads InputToSim EventStream, buffers input state for sim modules to query. Depends on TimeServer.
3. **LoadingScreenModule** — Writes to SimToRender during transitions (when no level module is active). Produces a loading frame each tick.
4. All three have `stages: ["all"]` — never stopped during transitions
5. All three log at DoStart/DoStop entry/exit (per SD-007)
6. LoadingScreen updates before level modules (per SD-009 — config ordering ensures this)
7. InputStreamModule provides queryable input state (IsKeyDown, WasKeyPressed this frame) for consuming modules via ModuleRef

## Module Specifications

### TimeServerModule

```cpp
class TimeServerModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"TimeServerModule"};

    float GetDeltaTime() const;
    float GetTotalTime() const;
    unsigned int GetFrameCount() const;

protected:
    StartResult DoStart() override;   // Init clock, return kReady
    void DoUpdate(float dt) override; // Accumulate time, increment frame
    StopResult DoStop() override;     // Return kDone
};
```

### InputStreamModule

```cpp
class InputStreamModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"InputStreamModule"};

    bool IsKeyDown(KeyCode key) const;
    bool WasKeyPressed(KeyCode key) const;
    bool WasKeyReleased(KeyCode key) const;

protected:
    StartResult DoStart() override;   // Return kReady
    void DoUpdate(float dt) override; // Consume InputToSim, update key state
    StopResult DoStop() override;     // Clear state, return kDone
private:
    EventStreamReader<InputEvent> mInput{this, "InputToSim"};
    ModuleRef<TimeServerModule> mTimeServer{this};
};
```

### LoadingScreenModule

```cpp
class LoadingScreenModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"LoadingScreenModule"};

protected:
    StartResult DoStart() override;   // Return kReady
    void DoUpdate(float dt) override; // Write loading frame to SimToRender
    StopResult DoStop() override;     // Return kDone
private:
    StreamWriter<FrameData> mRenderOutput{this, "SimToRender"};
};
```

## Update Ordering (SD-009)

In the manifest, LoadingScreen is listed before DummyLevel in SimPU's modules array. Both write to SimToRender. Since FrameStream keeps the latest write:
- During transitions: only LoadingScreen writes → loading UI displayed
- During DummyStage: LoadingScreen writes first, then DummyLevel overwrites → game frame displayed
- No conditional logic needed — pure ordering

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/CluicheTest/Modules/TimeServerModule.h` | New |
| `Cluiche/CluicheTest/Modules/TimeServerModule.cpp` | New |
| `Cluiche/CluicheTest/Modules/InputStreamModule.h` | New |
| `Cluiche/CluicheTest/Modules/InputStreamModule.cpp` | New |
| `Cluiche/CluicheTest/Modules/LoadingScreenModule.h` | New |
| `Cluiche/CluicheTest/Modules/LoadingScreenModule.cpp` | New |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Update |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Update |

## Dependencies

- **DiaApplicationFlow** — Module, ModuleRef, StreamWriter, EventStreamReader, DIA_MODULE
- **DiaCore/Time** — TimeAbsolute for timestamps
- **DiaInput** — InputEvent, KeyCode types
- **DiaGraphics** — FrameData (for SimToRender writes)

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant |
| PD-004 | Platform | No STL in public APIs | Compliant — query methods return primitives/engine types |
| SD-003 (DiaAppFlow) | System | ModuleRef only | Compliant — InputStreamModule uses ModuleRef<TimeServerModule> |
| SD-006 (DiaAppFlow) | System | Streams in config | Compliant — StreamWriter/Reader IDs match manifest declarations |
| SD-007 (CT AppFlow) | System | Log at DoStart/DoStop | Compliant |
| SD-009 (CT AppFlow) | System | Update order for LoadingScreen | Compliant — manifest ordering, no conditional logic |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | TimeServer | Should TimeServer provide pause/unpause (game time vs wall time)? | Yes. Game time (pauseable) + wall time (always ticking). Framework provides wall dt; TimeServer scales by pause state for game dt. |
| 2 | Input | Should InputStreamModule buffer multiple frames of events or just current frame? | Current frame only. Events consumed each DoUpdate, state reflects "this frame." Previous-frame comparison enables WasPressed/WasReleased. |
| 3 | LoadingScreen | What does the loading frame look like? | Minimal: solid background color + "Loading..." text via FrameData text primitives. Good enough for testbed. Real games would show progress bars. |
| 4 | LoadingScreen | Should LoadingScreen know it's "not needed" when a level module is active? | No. It always writes. Level module overwrites it. Zero awareness of other modules — pure decoupling via stream ordering (SD-009). |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
