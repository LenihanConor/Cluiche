**Spec:** @docs/specs/applications/dia/systems/diasteeringvisualdebugger/diasteeringvisualdebugger.md
**Status:** Todo

## API Decisions

- `VisitAgents` template form is the prereq; C-style overload deferred until test mock needs arise
- `DetectionBoxes` drawer starts disabled per SD-002 — test `DetectionBoxes_OffByDefault` must confirm fresh state
- Arrow length = velocity magnitude × scale (not normalised) per SD-003
- Drawer enables use `std::atomic<bool>`; `OnCommand` arrives on Render PU, drawers read on Sim PU

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `SteeringSystem::VisitAgents(Fn&&)` to `DiaSteering` under `#ifdef DIA_DEBUG`; iterates internal agent map calling fn(id, SteeringAgent, desiredVelocity) | New unit test in `TestSteeringSystem_Debug.cpp`: register 2 agents, call VisitAgents, assert both ids visited with correct state | Todo | sonnet | Must not be callable in Release (template body inside `#ifdef DIA_DEBUG`) |
| 2 | Scaffold `DiaSteeringVisualDebugger` module: `SteeringVisualDebugger.h/.cpp`, `VelocityArrowsDrawer.h/.cpp`, `SeparationRadiusDrawer.h/.cpp`, `DetectionBoxDrawer.h/.cpp`; `DiaSteeringVisualDebugger.vcxproj` + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` reports PENDING for AC-4 (not ERROR); build succeeds | Todo | haiku | `dia scaffold module` for stub, then `dia docs vcxproj-add` for each file |
| 3 | Implement `SteeringVisualDebugger` identity methods, `HasWorldDrawers()=true`, `GetDrawerCount()=3`, `GetDrawer()`, `Register()`/`Unregister()`, `GetJSONState()` skeleton emitting drawers array + `stats.agentCount`, `OnCommand("toggle",...)` with `std::atomic<bool>` per drawer, `OnCommand("setScale","arrowScale")` | `dia check debugger-contract` AC-4,10,11,12 pass | Todo | sonnet | |
| 4 | Implement `VelocityArrowsDrawer::Draw()`: call `mSystem.VisitAgents()`; per agent draw current-velocity arrow (colour `kDebugVelocityCurrent`) and desired-velocity arrow (colour `kDebugVelocityDesired`) from agent position; lengths × `GetDebugScale()`; guarded by atomic enabled flag | `RecordingDebugVisitor` with 2 agents → 4 line primitives; both colour keys used | Todo | sonnet | |
| 5 | Implement `SeparationRadiusDrawer::Draw()`: per agent draw circle at position, radius = `separationRadius × GetDebugScale()`, colour `kDebugSeparation` | 2 agents → 2 circle primitives; radius doubles when scale doubles | Todo | sonnet | |
| 6 | Implement `DetectionBoxDrawer::Draw()`: per agent draw quad outline from detection box half-extents × `GetDebugScale()`, colour `kDebugDetection` | 2 agents → 2 quad primitives | Todo | sonnet | |
| 7 | Write mandatory test shapes in `Tests/GoogleTests/DiaSteeringVisualDebugger/TestSteeringVisualDebugger.cpp`: DrawerGate, PrimitiveType, ScaleSensitivity, JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Todo | sonnet | Mock `IDebugDraw` or `RecordingDebugVisitor`; construct `SteeringVisualDebugger` with synthetic `SteeringSystem` |
| 8 | Write domain-specific test shapes: TwoArrowsPerAgent, ColoursDiffer, OneCirclePerAgent, OneBoxPerAgent, DrawerGate_SeparationRadius, DrawerGate_DetectionBoxes, Toggle_SeparationRadius, Toggle_DetectionBoxes, DetectionBoxes_OffByDefault, AgentCount_Accurate, NoAgents_NoOutput_NoAssert, ArrowScale_Command | All 12 shapes pass | Todo | sonnet | |
| 9 | `dia check debugger-contract` full pass — zero errors, zero warnings | Exit code 0 | Todo | haiku | |
