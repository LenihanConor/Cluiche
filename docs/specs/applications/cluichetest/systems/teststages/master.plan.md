# Master Implementation Plan: Visual Feedback + Visual Debugger Module

## Specs
- Phase 1: @docs/specs/applications/cluichetest/systems/teststages/visual-feedback.md
- Phase 2: @docs/specs/applications/cluichetest/systems/teststages/visual-debugger-module.md

## Status
Todo

---

## Session Notes

### Binding constraints (Platform → App → System → Feature)
- **PD-001** StringCRC for all IDs. No raw string comparison anywhere.
- **PD-002** Module lifecycle: `DoStart` / `DoUpdate` / `DoStop` + `DIA_MODULE()` macro for every module.
- **PD-004** No STL in public API headers. `std::mutex`, `std::atomic` in private impl only.
- **PD-006** VS project files are source of truth — every new `.h`/`.cpp` needs a vcxproj + filters entry.
- **AD-001** Three PUs (Main/Render/Sim). `TestStageHUDModule` on MainPU. `VisualDebuggerModule` on MainPU for this stage (single-PU stage — see PU note below).
- **AD-005** Testbed not product — HUD and debug console are `#ifdef DIA_DEBUG` only.
- **SD-TS-004** Exit is always user-triggered via ✕ button, never automatic.
- **ImGui guard** — always check `mDebugUI.Get()->IsFrameActive()` before any `ImGui::*` call.
- **Orchestrator suppression** — `AutomationService::IsHeartbeatActive()` returns true when pytest is driving. Both HUD and ✕ button must be hidden in that case.

### PU note — RigidBody2DStage
`rigidbody2d_stage.diaapp` has a single `MainPU` with `dedicated_thread: false`. There is no separate SimPU in this stage. `VisualDebuggerModule` is therefore placed on **MainPU** for this stage. `std::atomic<bool>` on `IVisualDebugger::mEnabled` still applies — it's the correct pattern for future stages that do use dedicated threads. Verify whether `AutomationModule` and `DebugUIModule` are injected globally or must be added to this diaapp.

### What already exists (do not re-implement)
- `TestResultsRegistry.h/.cpp` — fully implemented
- `TestResultsRegistryTests.cpp` — 10 tests covering T1–T6 of the spec (Create/Destroy pattern, not Reset())
- `TestStageHUDModule.h/.cpp` — files exist, `DoUpdate` is a stub
- `RigidBody2DTestModule.cpp` — already calls `SetRunning`, `SetPassed`, `SetTimeout`
- `BootMenuModule` — already has `GetStatusLabel()` reading `TestResultsRegistry`; verify badges render correctly

### Key code patterns

#### HUD bottom bar
```cpp
void TestStageHUDModule::DoUpdate(float dt)
{
    DIA_TRACE_ZONE("TestStageHUDModule.Update", Dia::Observation::Trace::Category::kApplication);
    if (!mDebugUI.Get() || !mDebugUI.Get()->IsFrameActive()) return;
    auto* svc = mAutomation.Get() ? mAutomation.Get()->GetService() : nullptr;
    if (svc && svc->IsHeartbeatActive()) return;   // hidden under orchestrator
    RenderBottomBar();
}

void TestStageHUDModule::RenderBottomBar()
{
    const ImGuiIO& io = ImGui::GetIO();
    const float barH = 28.0f;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - barH));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, barH));
    ImGui::SetNextWindowBgAlpha(0.85f);
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##HUDBar", nullptr, kFlags);
    // stage name | checkpoint icons | frame N/M | [X]
    ImGui::End();
}
```

#### Checkpoint icons
```cpp
// For each checkpoint name from AutomationService:
auto result = svc->GetCheckpointResult(StringCRC("rigid_body.all_settled"));
const char* icon = result.pending ? u8"⏳" : (result.passed ? u8"✓" : u8"✗");
ImVec4 col = result.pending ? yellow : (result.passed ? green : red);
ImGui::TextColored(col, "%s %s", icon, checkpointName);
```

#### PASS / TIMEOUT highlight
```cpp
auto* reg = &TestResultsRegistry::GetInstance();
const StageResult* r = reg->GetResult(reg->GetActiveStage());
if (r && r->state == StageResult::State::kPassed)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.35f, 0.0f, 0.85f));
else if (r && r->state == StageResult::State::kTimeout)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.45f, 0.0f, 0.0f, 0.85f));
bool pushedColour = r && (r->state == kPassed || r->state == kTimeout);
// ... render ...
if (pushedColour) ImGui::PopStyleColor();
```

#### ✕ exit button
```cpp
ImGui::SameLine(io.DisplaySize.x - 32.0f);
ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.08f, 0.08f, 1.0f));
ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
if (ImGui::Button(" X "))
{
    DIA_LOG_INFO("CluicheTest", "HUD exit — stage '%s' state %d",
        reg->GetActiveStage().AsChar(), (int)r->state);
    Dia::API::ExecuteCommandJson("dia.automation.navigate_to", {{"target", "Boot"}});
}
ImGui::PopStyleColor(2);
```

#### IVisualDebugger extension
```cpp
// IVisualDebugger.h
virtual void DrawImGui() {}                       // new — default no-op
virtual void SetEnabled(bool e) { mEnabled.store(e, std::memory_order_relaxed); }
virtual bool IsEnabled() const  { return mEnabled.load(std::memory_order_relaxed); }
private:
    std::atomic<bool> mEnabled{true};             // was plain bool
```

#### VisualDebuggerModule (SimPU / MainPU)
```cpp
class VisualDebuggerModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit VisualDebuggerModule(const Dia::Core::StringCRC& instanceId);
    Dia::Debug::DebugLayerManager& GetLayerManager() { return mManager; }
protected:
    StartResult DoStart() override;
    void DoUpdate(float dt) override;   // calls mManager.Draw(frameData)
    StopResult DoStop() override;
private:
    Dia::Debug::DebugLayerManager mManager;
};
```

#### VisualDebuggerConsoleModule (MainPU)
```cpp
class VisualDebuggerConsoleModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    VisualDebuggerConsoleModule(const Dia::Core::StringCRC& instanceId,
                                Dia::Debug::DebugLayerManager* manager);
    void RegisterDomain(Dia::Core::StringCRC name);
    void UnregisterDomain(Dia::Core::StringCRC name);
protected:
    StartResult DoStart() override;
    void DoUpdate(float dt) override;   // renders console if IsFrameActive()
    StopResult DoStop() override;
private:
    Dia::Debug::DebugLayerManager*       mManager = nullptr;
    Dia::Debug::DiaVisualDebuggerConsole mConsole;
    // domain tab list: fixed array, atomic count
};
```

#### RigidBody2D drawer ownership in RigidBody2DTestModule
```cpp
// DoStart — after SetupScene():
auto& mgr = mVisualDebugger.Get()->GetLayerManager();
mShapesDrawer    = new PhysicsShapesDrawer(*world, mgr);
mVelocityDrawer  = new VelocityArrowsDrawer(*world, mgr);
mContactDrawer   = new ContactNormalsDrawer(*world, mgr);
mConstraintDrawer= new ConstraintLinesDrawer(*world, mgr);
mAABBDrawer      = new PhysicsAABBDrawer(*world, mgr);
mgr.Register(mShapesDrawer,    10);
mgr.Register(mVelocityDrawer,  20);
mgr.Register(mContactDrawer,   30);
mgr.Register(mConstraintDrawer,40);
mgr.Register(mAABBDrawer,      50);
mVisualConsole.Get()->RegisterDomain(StringCRC("RigidBody2D"));

// DoStop — before body cleanup:
auto& mgr = mVisualDebugger.Get()->GetLayerManager();
mVisualConsole.Get()->UnregisterDomain(StringCRC("RigidBody2D"));
mgr.Unregister(mShapesDrawer->GetLayerName());
// ... unregister all 5 ...
delete mShapesDrawer; mShapesDrawer = nullptr;
// ... delete all 5 ...
```

#### diaapp module entry pattern
```json
{
    "instance_id": "VisualDebuggerModule",
    "type_id": "VisualDebuggerModule",
    "stages": ["RigidBody2DStage"],
    "dependencies": [],
    "reads": [],
    "writes": []
},
{
    "instance_id": "VisualDebuggerConsoleModule",
    "type_id": "VisualDebuggerConsoleModule",
    "stages": ["RigidBody2DStage"],
    "dependencies": ["DebugUIModule", "VisualDebuggerModule"],
    "reads": [],
    "writes": []
},
{
    "instance_id": "TestStageHUDModule",
    "type_id": "TestStageHUDModule",
    "stages": ["RigidBody2DStage"],
    "dependencies": ["DebugUIModule", "AutomationModule"],
    "reads": [],
    "writes": []
}
```

---

## Tasks

### Phase 1 — Visual Feedback (HUD bar, badges, ✕ button)

| # | Task | Spec ACs | Status | Model | Notes |
|---|------|----------|--------|-------|-------|
| 1 | Implement `TestStageHUDModule::DoUpdate` — bottom bar: stage name, checkpoint icons (⏳/✓/✗), frame counter `N/M` | AC-VF1–VF4, VF7, VF8 | Todo | sonnet | Add `ModuleRef<DebugUIModule>` to header; check `IsFrameActive()`; check `IsHeartbeatActive()` for suppression; read registry for frame data |
| 2 | Add PASS (green) / TIMEOUT (red) window background tint to HUD bar | AC-VF5, VF6 | Todo | haiku | Read `TestResultsRegistry` state; `ImGui::PushStyleColor` before `Begin`, pop after `End` |
| 3 | Add ✕ exit button (far right, red) — fires `dia.automation.navigate_to Boot`; hidden under orchestrator | AC-VF14, VF15, VF16 | Todo | haiku | `ImGui::SameLine`; `Dia::API::ExecuteCommandJson`; suppressed when `IsHeartbeatActive()` |
| 4 | Add `DIA_TRACE_ZONE` to `TestStageHUDModule::DoUpdate`; add `DIA_LOG_INFO` to `TestResultsRegistry::SetPassed` / `::SetTimeout`; add `DIA_LOG_INFO` to ✕ button handler | — | Todo | haiku | Observability; fold into tasks 1 and 3 during implementation |
| 5 | Wire `TestStageHUDModule` into `rigidbody2d_stage.diaapp` — add module entry on MainPU with deps on `DebugUIModule` + `AutomationModule` | AC-VF1 | Todo | haiku | Verify `DebugUIModule` + `AutomationModule` are available in the stage's PU context |
| 6 | Verify `BootMenuModule` badge rendering — read `BootMenuModule.cpp`, confirm `GetStatusLabel()` result appears next to stage names in ImGui table; add if missing | AC-VF9–VF13 | Todo | sonnet | Likely a verify-only step — `GetStatusLabel()` already exists |
| 7 | Write GoogleTests T7–T9 in `Cluiche/Tests/GoogleTests/CluicheTest/TestStageHUDModuleTests.cpp` | — | Todo | sonnet | T7: HUD hidden when heartbeat active; T8: renders when not; T9: ✕ fires navigate command. Add file to GoogleTests.vcxproj + filters |
| 8 | `dia run googletest --filter="TestResultsRegistry*:TestStageHUD*"` — all tests pass | — | Todo | haiku | Quote pass/fail output |
| 9 | `dia run cluichetest` — navigate to RigidBody2DStage, confirm HUD bar visible with checkpoint + frame counter, let it pass, confirm green bar, click ✕, confirm Boot menu, confirm pass badge | AC-VF1–VF6, VF9–VF13 | Todo | sonnet | Verify step — quote observed behaviour |

### Phase 2 — Visual Debugger Module (console + drawers)

| # | Task | Spec ACs | Status | Model | Notes |
|---|------|----------|--------|-------|-------|
| 10 | `IVisualDebugger.h` — add `virtual void DrawImGui() {}`; change `mEnabled` from `bool` to `std::atomic<bool>` with relaxed load/store in `SetEnabled`/`IsEnabled` | AC-VDM-05, VDM-06 | Todo | haiku | `#ifdef DIA_DEBUG` guard already present; add `#include <atomic>` |
| 11 | Add `DrawImGui()` overrides to all 5 RigidBody2D drawers: `PhysicsShapesDrawer` (sleeping checkbox), `VelocityArrowsDrawer` (scale slider), `ContactNormalsDrawer` (length slider), `ConstraintLinesDrawer` (empty), `PhysicsAABBDrawer` (fill/outline radio) | AC-VDM-10 | Todo | haiku | Each drawer: add member for the control value + `DrawImGui()` override in .h and .cpp |
| 12 | Restructure `DiaVisualDebuggerConsole` to tabbed layout: domain tabs (top) with Draw Layers + Stats collapsing headers; global command input; Output / Warnings bottom tabs | AC-VDM-07 | Todo | sonnet | Large change to `DiaVisualDebuggerConsole.h/.cpp`; `ConsoleSink` moves to Warnings tab; Output tab = REPL ring buffer (32 entries); domain tab list driven by `RegisterDomain`/`UnregisterDomain` |
| 13 | Create `Cluiche/CluicheGameBaseline/Modules/Debug/VisualDebuggerModule.h/.cpp` — SimPU module, owns `DebugLayerManager`, calls `Draw(frameData)` in `DoUpdate` | AC-VDM-01, VDM-04 | Todo | sonnet | New files in `Modules/Debug/` subfolder; exposes `GetLayerManager()`; `#ifdef DIA_DEBUG` wraps the impl |
| 14 | Create `Cluiche/CluicheGameBaseline/Modules/Debug/VisualDebuggerConsoleModule.h/.cpp` — MainPU module, takes `DebugLayerManager*` in constructor, owns `DiaVisualDebuggerConsole`, calls `console.Render()` when `IsFrameActive()` | AC-VDM-02, VDM-03, VDM-04 | Todo | sonnet | Constructor injection — no static accessor; exposes `RegisterDomain`/`UnregisterDomain`; `#ifdef DIA_DEBUG` wraps entirely |
| 15 | Add both new modules to `CluicheGameBaseline.vcxproj` + `.vcxproj.filters` under `Modules\Debug` filter | AC-VDM-04 (PD-006) | Todo | haiku | Two `ClCompile` + two `ClInclude` entries each |
| 16 | `RigidBody2DTestModule.h/.cpp` — add 5 drawer members + `ModuleRef<VisualDebuggerModule>` + `ModuleRef<VisualDebuggerConsoleModule>`; `DoStart` creates + registers drawers + calls `RegisterDomain`; `DoStop` unregisters + deletes drawers + calls `UnregisterDomain` | AC-VDM-08, VDM-09 | Todo | sonnet | Drawer ptrs are raw owning; delete before body cleanup in DoStop |
| 17 | Add `VisualDebuggerModule` + `VisualDebuggerConsoleModule` + `TestStageHUDModule` entries to `rigidbody2d_stage.diaapp`; confirm dependency order | AC-VDM-01–VDM-03 | Todo | haiku | Both on MainPU for this stage (single-PU); `VisualDebuggerConsoleModule` depends on `VisualDebuggerModule` + `DebugUIModule` |
| 18 | `dia run cluichetest` — navigate to RigidBody2DStage, press debug console toggle, confirm: 5 `rb2d.*` layers listed under RigidBody2D tab, shapes/arrows/normals/AABBs visible, disable a layer → primitives disappear, DrawImGui controls work | AC-VDM-11–VDM-13 | Todo | sonnet | Verify step — quote observed behaviour |
