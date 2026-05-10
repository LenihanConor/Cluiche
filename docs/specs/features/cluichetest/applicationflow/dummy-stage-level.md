# Feature Spec: DummyStage Level

## Parent System
@docs/specs/systems/cluichetest/applicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Application Flow | @docs/specs/systems/cluichetest/applicationflow.md |
| Feature | DummyStage Level | (this file) |

## Problem Statement

CluicheTest needs a concrete game stage that demonstrates the full DiaApplicationFlow pattern: a stage-specific module with async asset loading, input handling, sim logic, rendering output, and UI commands. DummyStage is the reference implementation for how future game stages are built.

## Acceptance Criteria

1. **DummyLevelModule** — stage-specific module on SimPU, active only during DummyStage
2. DoStart loads textures/assets via DiaAssetRuntime — returns kLoading until assets ready, then kReady
3. DoUpdate runs game logic: reads input (via ModuleRef<InputStreamModule>), updates sprite positions, writes FrameData to SimToRender
4. DoUpdate sends UI commands to SimToUI (score, FPS display)
5. DoStop releases asset handles, returns kDone
6. Comes from `dummy_stage.diastage` / `dummy_stage.diaapp` (per SD-005)
7. Dependencies: TimeServer, InputStream (declared in manifest)
8. Writes: SimToRender, SimToUI (declared in manifest)
9. Logs at DoStart/DoStop entry/exit (per SD-007)
10. Demonstrates async startup pattern (kLoading → kReady over multiple frames)

## Module Specification

```cpp
class DummyLevelModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"DummyLevelModule"};

protected:
    StartResult DoStart() override;   // Request asset loads, return kLoading until done
    void DoUpdate(float dt) override; // Game logic, write frame + UI commands
    StopResult DoStop() override;     // Release assets, return kDone
private:
    ModuleRef<TimeServerModule> mTimeServer{this};
    ModuleRef<InputStreamModule> mInput{this};
    StreamWriter<FrameData> mRenderOutput{this, "SimToRender"};
    EventStreamWriter<UICommand> mUIOutput{this, "SimToUI"};

    // Asset handles
    // Sprite/game state
};
```

## Game Logic (Minimal Testbed)

DummyStage demonstrates:
- Textured sprite rendering (loaded via DiaAssetRuntime)
- Sprite movement from keyboard input (arrow keys / WASD)
- Frame counter displayed via SimToUI
- Basic collision with screen bounds

This is not complex game logic — it validates the framework patterns work end-to-end.

## Async Startup Pattern

```
Frame 1: DoStart() — request texture loads from AssetService, return kLoading
Frame 2: DoStart() — check asset handles, still loading, return kLoading
Frame N: DoStart() — all assets ready, return kReady
         → Module transitions to Active, DoUpdate begins
```

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/CluicheTest/Modules/DummyLevelModule.h` | New |
| `Cluiche/CluicheTest/Modules/DummyLevelModule.cpp` | New |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Update |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Update |

## Dependencies

- **DiaApplicationFlow** — Module, ModuleRef, StreamWriter, EventStreamWriter, DIA_MODULE
- **DiaAssetRuntime** — texture/asset loading
- **DiaGraphics** — FrameData, draw commands, sprite primitives
- **DiaInput** — KeyCode (via InputStreamModule)
- **Sim PU Modules** (this system) — TimeServerModule, InputStreamModule via ModuleRef

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — kTypeId is StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant |
| SD-001 (DiaAppFlow) | System | Config is sole source of truth | Compliant — stage membership, deps, streams all in manifest |
| SD-003 (DiaAppFlow) | System | ModuleRef only | Compliant — TimeServer and InputStream accessed via ModuleRef |
| SD-005 (CT AppFlow) | System | Stage-specific from .diastage | Compliant — module defined in dummy_stage.diaapp |
| SD-007 (CT AppFlow) | System | Log at DoStart/DoStop | Compliant |
| SD-009 (CT AppFlow) | System | Update order (LoadingScreen before level) | Compliant — manifest ordering, DummyLevel's SimToRender write is latest |
| SD-013 (DiaAppFlow) | System | DoStart returns StartResult | Compliant — demonstrates kLoading pattern |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Assets | What textures does DummyStage load? | Reuse existing DummyStage assets (sprite textures already in asset pipeline). No new art needed. |
| 2 | Complexity | How much game logic should DummyStage have? | Minimal: one moveable sprite, keyboard control, screen bounds. Enough to prove the pipeline works, not enough to be a distraction. |
| 3 | UI | What UICommands does DummyLevel send? | ShowText("FPS", value), ShowText("Score", value). Two simple text overlays. |
| 4 | Stop | Should DoStop wait for asset unloads or just release handles? | Just release handles (decrement ref counts). AssetRuntime handles actual unload asynchronously. DoStop returns kDone immediately. |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
