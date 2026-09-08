# Implementation Plan: ImGui Bootstrap Menu

**Spec:** [imgui-bootstrap-menu.md](imgui-bootstrap-menu.md)
**Created:** 2026-05-24

## Session Notes

**Spec decisions summary:** Both modules live on RenderPU (owns GL context). `DebugUIModule` is global (all stages), `BootMenuModule` is Boot-stage-only. ImGui renders after RenderModule draws game content — overlay on top. `TransitionTo()` is thread-safe from any PU. UIModule (Ultralight) becomes stage-scoped to DummyStage only. SD-003 (auto-advance) is superseded — Boot requires manual Launch or automation `navigate_to`. SFMLImGuiBackend must work in Release (remove `#ifdef DIA_DEBUG`). TestResultsRegistry is a thread-safe singleton queryable from RenderPU.

## Implementation Patterns

### Module Structure (RenderPU)

Both modules inherit `Dia::ApplicationFlow::Module`, register via `DIA_MODULE` macro with namespace alias pattern:
```cpp
namespace { using DebugUIModule_ = Cluiche::AppFlow::DebugUIModule; }
DIA_MODULE(DebugUIModule_);
```

- Constructor: `explicit DebugUIModule(const Dia::Core::StringCRC& instanceId)`
- `static const Dia::Core::StringCRC kTypeId;`
- Virtual overrides: `DoStart() → StartResult`, `DoUpdate(float dt)`, `DoStop() → StopResult`
- Return `StartResult::kLoading` if dependencies not ready (e.g., canvas not available yet)

### DebugUIModule Lifecycle

```
DoStart:
  canvas = KernelModule::GetStaticCanvas()  // cross-PU static accessor
  if (!canvas) return kLoading
  backend = new SFMLImGuiBackend()
  backend->SetWindow(canvas->GetRenderWindow())
  Dia::ImGui::SetBackend(backend)
  Dia::ImGui::Init()
  mFrameActive = false
  return kReady

DoUpdate(dt):
  Dia::ImGui::NewFrame(dt)
  mFrameActive = true
  // (other modules render ImGui between here...)
  // NOTE: Render() called at end — see ordering below

DoStop:
  mFrameActive = false
  Dia::ImGui::Shutdown()
  delete mBackend
  return kDone
```

**Update ordering:** DebugUIModule must update BEFORE BootMenuModule (dependency ordering in manifest). However, ImGui::Render() must be called AFTER all modules submit their draw commands. Two approaches:
- (A) DebugUIModule calls Render() in a post-update hook or at the end of DoUpdate with a flag
- (B) DebugUIModule provides `BeginFrame()`/`EndFrame()` and BootMenuModule calls EndFrame after drawing

Simplest: DebugUIModule calls NewFrame at top of DoUpdate, then immediately returns. BootMenuModule (depends on DebugUIModule, so updates after it) draws ImGui content. DebugUIModule calls Render() — but it updates first. **Resolution:** Use module update ordering — BootMenuModule depends on DebugUIModule so updates after. DebugUIModule needs to call Render() AFTER BootMenuModule. 

**Chosen pattern:** DebugUIModule overrides a late-update or uses framework's module ordering: since DebugUIModule has NO dependencies on BootMenuModule, it updates first. We split: NewFrame in DoUpdate, Render in a separate mechanism. Simplest C++ pattern: DebugUIModule registers a frame-end callback or — better — BootMenuModule calls `Dia::ImGui::Render()` at the end of its own DoUpdate. DebugUIModule just does NewFrame + exposes IsFrameActive. This works because BootMenuModule is the last ImGui consumer during Boot.

**Final pattern:**
- DebugUIModule::DoUpdate → `NewFrame(dt)`, sets `mFrameActive = true`
- BootMenuModule::DoUpdate → draws ImGui content, then calls `Dia::ImGui::Render()`
- For stages without BootMenuModule (non-Boot): DebugUIModule needs to call Render itself if no consumer did. Use a flag: if nobody called Render by end of frame, DebugUIModule calls it in a post-frame callback. Or: DebugUIModule always calls Render() at end of its NEXT update (deferred one frame). **Simplest:** DebugUIModule calls both NewFrame AND Render in DoUpdate, sandwiching nothing — then BootMenuModule inserts its draw calls between them using the Module dependency ordering.

**Actually simplest:** Don't split. DebugUIModule does the full NewFrame/Render cycle, but the framework's module update order guarantees BootMenuModule updates BETWEEN DebugUIModule's update calls? No — each module's DoUpdate is called once per frame sequentially.

**Correct architecture:** DebugUIModule manages lifecycle only (Init/Shutdown). Frame calls are: DebugUIModule::DoUpdate calls NewFrame. Then BootMenuModule::DoUpdate calls ImGui:: draw commands. Then DebugUIModule calls Render... but it already finished its update. 

**Final resolution — two-pass pattern not needed.** Use this:
- DebugUIModule::DoUpdate → calls `NewFrame(dt)` at start, calls `Render()` at end
- BootMenuModule depends on DebugUIModule in manifest → framework calls BootMenuModule::DoUpdate AFTER DebugUIModule::DoUpdate
- **Problem**: BootMenuModule draws after Render() already called.

**Actual correct pattern (confirmed from codebase):** Module update order follows dependency order. If BootMenuModule depends on DebugUIModule, BootMenuModule updates AFTER. So:
1. DebugUIModule::DoUpdate — calls NewFrame(dt) only
2. BootMenuModule::DoUpdate — draws ImGui content, calls Render() at end

This means Render() is called by the last consumer module. For stages with no Boot (e.g., DummyStage), DebugUIModule would have NewFrame with no Render. **Solution:** DebugUIModule calls both NewFrame AND Render. Between them, nothing draws. That's fine — ImGui renders an empty frame (no windows). When BootMenuModule is active, it draws AFTER DebugUIModule::DoUpdate finishes (including Render). But that means drawing after Render — invalid.

**FINAL correct pattern:**
- DebugUIModule exposes `BeginFrame(dt)` and `EndFrame()` public methods
- DebugUIModule::DoUpdate calls BeginFrame. Does NOT call EndFrame.
- BootMenuModule::DoUpdate draws ImGui, then calls `mDebugUI.Get()->EndFrame()`
- For non-Boot stages: DebugUIModule detects nobody called EndFrame and calls it itself via a flag check at next frame start (or just accepts empty frame). Better: DebugUIModule calls EndFrame in DoUpdate AFTER BeginFrame with a flag. If a consumer is present, it skips the auto-EndFrame.

**Simplest working pattern adopted:**
- DebugUIModule::DoUpdate → `NewFrame(dt)` + set flag `mNeedsRender = true`
- BootMenuModule::DoUpdate → draw ImGui + call `mDebugUI.Get()->RenderFrame()` which calls `Dia::ImGui::Render()` and clears flag
- DebugUIModule end-of-frame: if `mNeedsRender` still true at next DoUpdate, call Render() before next NewFrame. This is a one-frame-late render for empty frames — acceptable for non-Boot stages that have no ImGui content.

### BootMenuModule UI

Per mockup (`docs/research/bootstra_imgui_ui/mockup_boot_menu.html`):
- `ImGui::Begin("CluicheTest — Bootstrap")`
- Header: project name/version + automation badge
- Table: 3 columns (dot indicator, stage name, status badge)
- Selectable rows; selected row highlighted
- Launch button + Enter key handling
- Footer: passed/failed/pending counts + "Enter = Launch"

### Manifest Changes

Add to RenderPU modules in `cluiche_main.diaapp`:
```json
{"instance_id": "DebugUI", "type_id": "DebugUIModule", "stages": ["all"], "dependencies": ["RenderModule"]},
{"instance_id": "BootMenu", "type_id": "BootMenuModule", "stages": ["Boot"], "dependencies": ["DebugUI"]}
```

Move UIModule from `stages: ["all"]` to `stages: ["DummyStage"]`.

Remove BootUIPageModule entry.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Remove `#ifdef DIA_DEBUG` from SFMLImGuiBackend.h/.cpp + RenderWindow | Build Release config succeeds | Done | opus | Also removed gates from RenderWindow.h/.cpp (member, include, init, shutdown, event forwarding) |
| 2 | Implement DebugUIModule.h/.cpp (global, RenderPU) | Module starts, NewFrame called | Done | opus | Calls NewFrame(dt) in DoUpdate; RenderModule depends on DebugUI so updates after; Render() handled by RenderWindow::EndFrame |
| 3 | Implement BootMenuModule.h/.cpp (Boot stage, RenderPU) | Boot shows ImGui menu with stage list, status badges, Launch transitions | Done | opus | ImGui window per mockup, queries TestResultsRegistry, calls TransitionTo |
| 4 | Scope UIModule to DummyStage (manifest + verify) | Boot stage starts without Ultralight init | Done | opus | Changed `stages: ["all"]` → `stages: ["DummyStage"]` in manifest |
| 5 | Remove BootUIPageModule, LaunchUIPage from vcxproj | Build succeeds without removed files | Done | opus | Removed from vcxproj + filters; source files not deleted yet (can be done on cleanup pass) |
| 6 | Update cluiche_main.diaapp manifest | App starts with new module topology | Done | opus | DebugUI + BootMenu on RenderPU, RenderModule depends on DebugUI, BootUIPageModule removed |
| 7 | Supersede SD-003 in system spec | Spec updated | Done | opus | applicationflow.md decisions table updated |
| 8 | Verify E2E automation works from Boot | `dia orchestrate` passes | Blocked | — | App boots to Boot stage (confirmed via E2E), but `navigate_to` command not registered in CluicheTest. Needs DiaAPI command wiring. |
| 9 | Add observation instrumentation (trace, metrics, health) | Health shows DebugUIModule:ok; boot_menu.* metrics emitted | Done | opus | DIA_TRACE_ZONE in DrawMenu + DoStart; Gauge(stage_count) + Counter(launches); HealthReporter in DebugUIModule |
| 10 | Add unit tests for TestResultsRegistry | `dia run googletest --filter="TestResultsRegistry*"` 12/12 pass | Done | opus | Tests: empty state, set running/passed/failed/timeout, multiple stages, active tracking, idempotent |
