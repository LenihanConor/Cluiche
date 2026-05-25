# Feature Spec: Visual Debugger Module

## Parent System
@docs/specs/systems/cluichetest/teststages.md

## Mockup
@docs/specs/features/cluichetest/teststages/visual-debugger-module.mockup.html

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-002, PD-004, PD-006, PD-007 |
| Application | @docs/specs/applications/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/systems/cluichetest/teststages.md | SD-TS-001, SD-TS-003, SD-TS-005 |
| Sibling | @docs/specs/features/cluichetest/teststages/visual-feedback.md | Shares DebugUIModule + ImGui frame lifecycle |

## Status
Approved

## Problem Statement

The RigidBody2D test stage runs a physics simulation that is invisible to developers: no collision shapes, no velocity arrows, no contact points are rendered. Five drawer classes already exist in `DiaRigidBody2DVisualDebugger` implementing `IVisualDebugger`, and `DebugLayerManager` exists as a registry with enable/disable support, but nothing wires them into the render path. This feature establishes the pattern for all future visual debuggers: a `VisualDebuggerModule` (SimPU) owns and drives the draw layer registry; a `VisualDebuggerConsoleModule` (MainPU) drives a single ImGui console window shared across all debugger domains. The console has three regions: domain tabs at the top (one per registered domain — RigidBody2D, Animation2D, IK2D, etc.) each with collapsible Draw Layers and Stats sections; a global command input; and bottom tabs (Output for REPL responses, Warnings for the DiaLogger warn+ tail). Domain modules register their drawers on start and unregister on stop.

## Acceptance Criteria

### Module lifecycle
- **AC-VDM-01** — `VisualDebuggerModule` (SimPU, `CluicheGameBaseline`) owns a `DebugLayerManager` instance and calls `manager.Draw(frameData)` once per Sim frame.
- **AC-VDM-02** — `VisualDebuggerConsoleModule` (MainPU, `CluicheGameBaseline`) is constructed with a pointer to the `DebugLayerManager` owned by `VisualDebuggerModule`; no static accessor is used.
- **AC-VDM-03** — `VisualDebuggerConsoleModule` calls `console.Render(manager, debugFrameData)` each MainPU frame when the console is visible, and `Dia::ImGui::NewFrame()` has already been called by `DebugUIModule`.
- **AC-VDM-04** — Both modules follow the `DoStart` / `DoUpdate` / `DoStop` + `DIA_MODULE()` pattern used by all `CluicheGameBaseline` modules.

### Thread safety
- **AC-VDM-05** — `IVisualDebugger::mEnabled` is changed from `bool` to `std::atomic<bool>` so that `EnableLayer` / `DisableLayer` calls from MainPU are safe against `Draw()` reads on SimPU without a lock.

### IVisualDebugger interface extension
- **AC-VDM-06** — `IVisualDebugger` gains a virtual no-op `DrawImGui()` method; existing drawers compile without changes.
- **AC-VDM-07** — `DiaVisualDebuggerConsole` is restructured with the following layout (see mockup):
  - **Domain tabs (top)** — one tab per registered domain (e.g. "RigidBody2D", "Animation2D"); each tab contains two `ImGui::CollapsingHeader` sections: "Draw Layers" (layer checkboxes + per-layer `DrawImGui()` panels) and "Stats" (domain-specific metrics grid). Both default collapsed.
  - **Global command input** — always visible below the domain tabs; a single text field dispatching to `DiaAPI::CommandRegistry` on Enter.
  - **Bottom tabs** — "Output" (REPL: responses to commands executed this session, cleared on console close) and "Warnings" (DiaLogger warn+ tail ring buffer, 64 lines). The existing `ConsoleSink` moves here.

### RigidBody2D drawer wiring
- **AC-VDM-08** — `RigidBody2DTestModule::DoStart()` instantiates all five drawers (`PhysicsShapesDrawer`, `VelocityArrowsDrawer`, `ContactNormalsDrawer`, `ConstraintLinesDrawer`, `PhysicsAABBDrawer`) and registers them with `VisualDebuggerModule`'s `DebugLayerManager` via `ModuleRef<VisualDebuggerModule>`.
- **AC-VDM-09** — `RigidBody2DTestModule::DoStop()` unregisters all five drawers before destroying them.
- **AC-VDM-10** — All five drawers implement `DrawImGui()` with the following controls:
  - `PhysicsShapesDrawer`: checkbox "Show sleeping bodies" (default: on)
  - `VelocityArrowsDrawer`: float slider "Arrow scale" range [0.1, 5.0] (default: 1.0)
  - `ContactNormalsDrawer`: float slider "Normal length" range [0.1, 5.0] (default: 0.5)
  - `ConstraintLinesDrawer`: no controls (empty `DrawImGui()`)
  - `PhysicsAABBDrawer`: radio "Draw mode" — Outline (default) / Fill

### Visual correctness
- **AC-VDM-11** — When running `dia run cluichetest` and navigating to `RigidBody2DStage`, pressing the debug console toggle key shows the console window with 5 layers listed under `rb2d.*` layer names.
- **AC-VDM-12** — Disabling a layer in the console stops that layer's primitives appearing in the next rendered frame.
- **AC-VDM-13** — Dynamic bodies are drawn white, static bodies grey, sleeping bodies dark blue by `PhysicsShapesDrawer`.

### No-op outside DEBUG
- **AC-VDM-14** — All new code is wrapped in `#ifdef DIA_DEBUG`; Release builds compile and run without the console or drawers.

## Design

### Module split (SimPU / MainPU)

```
SimPU
  VisualDebuggerModule
    owns:  DebugLayerManager mManager
    does:  mManager.Draw(frameData)  every DoUpdate()
    exposes: DebugLayerManager& GetLayerManager()

MainPU
  VisualDebuggerConsoleModule
    constructed with: DebugLayerManager* (pointer to SimPU module's manager)
    owns:  DiaVisualDebuggerConsole mConsole
    does:  mConsole.Render(*mManager, debugFrameData)  every DoUpdate() when DebugUIModule::IsFrameActive()
```

The `DebugLayerManager*` pointer is injected at construction — the stage's PU setup creates `VisualDebuggerModule` first, then passes `&visualDebuggerModule.GetLayerManager()` into `VisualDebuggerConsoleModule`. Same injection pattern used across the baseline for canvas, window, etc.

### Thread safety

`IVisualDebugger::mEnabled` is promoted to `std::atomic<bool>` (relaxed load in `Draw()`, relaxed store in `SetEnabled()`). The rest of `DebugLayerManager` (registration, sort) is only touched at stage start/stop, never mid-frame from a different thread, so no additional lock is needed.

### DrawImGui() extension

```cpp
// IVisualDebugger.h addition
virtual void DrawImGui() {}   // default no-op; override for per-drawer controls
```

### Console layout

```
┌─ Debug Console ──────────────────────────────────┐
│ [RigidBody2D▼] [Animation2D] [IK2D]              │  ← domain tabs
│ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ │
│  ▶ Draw Layers    (CollapsingHeader, default open)│
│     [✓] rb2d.shapes        ▼                     │
│          [✓] Show sleeping bodies                 │
│     [✓] rb2d.velocity_arrows  ▼                  │
│          Arrow scale ────●──── 1.0               │
│     ...                                          │
│  ▶ Physics Stats  (CollapsingHeader, collapsed)  │
│     Bodies: 5 awake / 10   Contacts: 5 active    │
│     Islands: 2             Step time: 1.2ms      │
│     Substeps: 2 / 4                              │
│ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ │
│ Command: [debug.layer.list_____________]          │
│ [Output▼] [Warnings]                             │  ← bottom tabs
│  > debug.layer.list                              │
│    [0] rb2d.shapes (enabled)                     │
│    ...                                           │
└──────────────────────────────────────────────────┘
```

Domain registration: `VisualDebuggerConsoleModule` exposes `RegisterDomain(StringCRC name)`. Each domain module calls this at `DoStart()`, which adds a tab. `UnregisterDomain(name)` at `DoStop()` removes it. The console renders only tabs for currently registered domains.

Output tab: stores command responses in a fixed ring buffer (32 entries). Cleared on console close. No ambient log — only responses to executed commands.

Warnings tab: retains the existing `ConsoleSink` → `DiaLogger` integration, capped at 64 lines, warn+ threshold.

### Domain module drawer ownership

```
RigidBody2DTestModule::DoStart()
  mShapesDrawer   = new PhysicsShapesDrawer(world, manager)
  mVelocityDrawer = new VelocityArrowsDrawer(world, manager)
  mContactDrawer  = new ContactNormalsDrawer(world, manager)
  mConstraintDrawer = new ConstraintLinesDrawer(world, manager)
  mAABBDrawer     = new PhysicsAABBDrawer(world, manager)
  manager.Register(mShapesDrawer, priority=10)
  ... (each drawer registered at incrementing priority)

RigidBody2DTestModule::DoStop()
  manager.Unregister(mShapesDrawer->GetLayerName())
  ... (each drawer unregistered)
  delete all drawers
```

Drawers are raw owning pointers inside the module — they live exactly as long as the test stage is active, same as the physics bodies they reference.

### Files touched

| File | Change |
|------|--------|
| `Dia/DiaVisualDebugger/IVisualDebugger.h` | Add `virtual void DrawImGui() {}`, change `mEnabled` to `std::atomic<bool>` |
| `Dia/DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.cpp` | Update `RenderLayerTree` to call `DrawImGui()` in collapsible section |
| `Dia/DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.h/.cpp` | Add `DrawImGui()` override |
| `Dia/DiaRigidBody2DVisualDebugger/VelocityArrowsDrawer.h/.cpp` | Add `DrawImGui()` override, promote `mArrowScale` to member with slider |
| `Dia/DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.h/.cpp` | Add `DrawImGui()` override, add `mNormalLength` member with slider |
| `Dia/DiaRigidBody2DVisualDebugger/ConstraintLinesDrawer.h/.cpp` | Add empty `DrawImGui()` override |
| `Dia/DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.h/.cpp` | Add `DrawImGui()` override, add `mFilled` bool with radio |
| `Cluiche/CluicheGameBaseline/Modules/VisualDebuggerModule.h/.cpp` | **New** — SimPU module |
| `Cluiche/CluicheGameBaseline/Modules/VisualDebuggerConsoleModule.h/.cpp` | **New** — MainPU module |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj` | Add new module files |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj.filters` | Add new module files |
| `Cluiche/CluicheTest/Modules/TestStages/RigidBody2DTestModule.h/.cpp` | Add drawer members, DoStart register, DoStop unregister |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | No change (RigidBody2DTestModule already registered) |
| `Cluiche/Assets/Stages/RigidBody2DStage/` | Wire both new modules into the stage's PU configuration |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|----------|---------|------------|
| PD-001 | StringCRC for all runtime IDs | Layer names are `StringCRC` constants (`kUniqueId` pattern). No raw string comparison. ✓ |
| PD-002 | ProcessingUnit / Phase / Module architecture | `VisualDebuggerModule` and `VisualDebuggerConsoleModule` both inherit `Dia::ApplicationFlow::Module` and register via `DIA_MODULE()`. ✓ |
| PD-004 | No STL in public APIs | Public headers of both new modules use only Dia types; `std::atomic` is in the private impl of `IVisualDebugger`. ✓ |
| PD-006 | VS project files are source of truth | Both new modules added to `.vcxproj` and `.vcxproj.filters`. ✓ |
| PD-007 | C++20 required | No C++20-exclusive features used; `std::atomic<bool>` has been available since C++11. ✓ |
| AD-001 | Three ProcessingUnits (Main/Render/Sim) | `VisualDebuggerModule` on SimPU, `VisualDebuggerConsoleModule` on MainPU — correct PU assignment. ✓ |
| AD-004 | Test levels included | RigidBody2DTestModule is a test stage; this feature only wires it more richly, doesn't change its test-stage nature. ✓ |
| AD-005 | App is testbed not product | Debug console is `#ifdef DIA_DEBUG` only; no debug overhead in Release. ✓ |
| SD-TS-001 | One manifest stage per test feature | No change to manifests — this wires an existing stage. ✓ |
| SD-TS-003 | Test stages emit metrics | `RigidBody2DTestModule` existing metrics unchanged; drawers emit no new metrics (debug-draw is not a metric). ✓ |
| SD-TS-005 | Individual stages are feature specs | This is the feature spec. ✓ |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Thread safety | Is relaxed `std::atomic<bool>` sufficient for `mEnabled`, or does the draw call need to see the toggle in the same frame it was made? | Relaxed is fine — one-frame lag on a visual toggle is invisible to the user. | Relaxed is acceptable. One-frame lag on enable/disable is imperceptible. |
| 2 | VisualDebuggerConsoleModule construction | How is the `DebugLayerManager*` pointer passed to `VisualDebuggerConsoleModule` — constructor argument, or a `SetLayerManager()` call after construction? | Constructor argument — matches how `KernelModule` canvas is injected. | Constructor argument. Avoids two-phase init; matches existing baseline injection patterns. |
| 3 | DrawImGui collapsible section | Should the `TreeNode` be open by default, or collapsed? | Collapsed by default — less visual noise when the console opens. | Collapsed by default. User expands the drawer they care about. |
| 4 | ConstraintLinesDrawer | No constraints exist in the RigidBody2D scene. Should the drawer still be registered, or conditionally skipped? | Register it anyway — it will draw nothing and its presence in the layer list is informative. | Register unconditionally. Empty draw is correct behaviour; layer list completeness is more useful than hiding it. |
| 5 | DebugLayerManager const ref on drawers | Drawers currently hold `const DebugLayerManager&` but don't use it. Should that ref be removed now that `DrawImGui()` is the control path, or left for future use (e.g. `GetDebugScale()`)? | Leave it — `GetDebugScale()` is a likely consumer once world-space → screen-space scaling is needed. | Leave the ref; `GetDebugScale()` is a realistic future consumer. |
| 6 | vcxproj placement | `VisualDebuggerModule` and `VisualDebuggerConsoleModule` live in `CluicheGameBaseline`. Should they be in a `Modules/Debug/` subfolder to keep debug modules separate from gameplay modules? | Yes — `Modules/Debug/` keeps the baseline tidy as more debug modules are added. | Yes, `Modules/Debug/` subfolder in both vcxproj and filesystem. |
| 7 | Release build | `DiaVisualDebuggerConsoleModule` uses `DiaVisualDebuggerConsole` which is `#ifdef DIA_DEBUG`. Does the module header/cpp need a full `#ifdef DIA_DEBUG` guard, or is conditional compilation inside the .cpp sufficient? | Full `#ifdef DIA_DEBUG` guard on the module — keeps the type invisible in Release and avoids empty-module overhead. | Full `#ifdef DIA_DEBUG` on both modules' header and cpp. |
| 8 | Domain registration | `RegisterDomain` / `UnregisterDomain` is called from `DoStart` / `DoStop` on SimPU domain modules but the console lives on MainPU. Is a domain name list (`DynamicArrayC<StringCRC>`) sufficient, or does it need synchronisation? | An atomic domain count + fixed-size name array (max 8 domains) is sufficient — registration only happens at stage transitions, never mid-frame. | Atomic count + fixed name array. Stage transitions are serialised by the PU phase system so no race exists at registration time. |

## Open Questions

None.
