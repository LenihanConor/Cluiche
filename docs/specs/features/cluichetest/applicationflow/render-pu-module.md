# Feature Spec: Render PU Module

## Parent System
@docs/specs/systems/cluichetest/applicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Application Flow | @docs/specs/systems/cluichetest/applicationflow.md |
| Feature | Render PU Module | (this file) |

## Problem Statement

RenderPU runs on a dedicated 60Hz thread and has a single job: read the latest frame from SimToRender and draw it to the canvas. It is entirely stage-agnostic — whatever was written to the stream gets rendered.

## Acceptance Criteria

1. **RenderModule** — Reads SimToRender FrameStream, draws to canvas each tick
2. Runs at 60Hz on a dedicated thread
3. `stages: ["all"]` — always active
4. Stage-agnostic: doesn't know or care what stage is active
5. Gets canvas reference from BootstrapResources (per SD-008)
6. If no frame data available (stream empty), renders nothing (clear screen)
7. Logs at DoStart/DoStop entry/exit (per SD-007)

## Module Specification

```cpp
class RenderModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr StringCRC kTypeId{"RenderModule"};

protected:
    StartResult DoStart() override;   // Get canvas from bootstrap, return kReady
    void DoUpdate(float dt) override; // FetchLatest from SimToRender, draw to canvas
    StopResult DoStop() override;     // Return kDone
private:
    StreamReader<FrameData> mFrameInput{this, "SimToRender"};
};
```

## Render Loop

```
Each DoUpdate (60Hz):
1. const FrameData* frame = mFrameInput.FetchLatest();
2. if (!frame) { canvas->Clear(black); canvas->Display(); return; }
3. canvas->Clear(frame->clearColor);
4. for each draw command in frame->commands:
      canvas->Draw(command);
5. canvas->Display();
```

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/CluicheTest/Modules/RenderModule.h` | New |
| `Cluiche/CluicheTest/Modules/RenderModule.cpp` | New |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Update |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Update |

## Dependencies

- **DiaApplicationFlow** — Module, StreamReader, DIA_MODULE
- **DiaGraphics** — ICanvas, FrameData, draw commands
- **DiaCore/Time** — TimeAbsolute for FetchLatest

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant |
| PD-004 | Platform | No STL in public APIs | Compliant |
| SD-006 (DiaAppFlow) | System | Streams in config | Compliant — reads SimToRender declared in manifest |
| SD-007 (CT AppFlow) | System | Log at DoStart/DoStop | Compliant |
| SD-008 (CT AppFlow) | System | Canvas from BootstrapResources | Compliant — canvas not owned by this module |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | Is canvas->Display() thread-safe (called from RenderPU thread while MainPU owns the window)? | Yes — SFML allows rendering on a non-main thread as long as the window's GL context is activated on that thread. KernelModule deactivates context after creation; RenderModule activates it in DoStart. |
| 2 | Frame rate | Should RenderModule skip frames if SimToRender hasn't updated (same frame as last tick)? | No. Always render latest. If sim runs at 30Hz and render at 60Hz, you get the same frame twice — that's fine (consistent display, no tearing logic needed). |
| 3 | Vsync | Should RenderModule handle vsync? | Yes, enable vsync in DoStart (canvas setting). 60Hz PU frequency is a soft cap; vsync provides the hard sync. |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
