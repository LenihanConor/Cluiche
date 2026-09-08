# Implementation Plan: Test Stage Visual Feedback

## Spec
@docs/specs/applications/cluichetest/systems/teststages/visual-feedback.md

## Status
In Progress

## Session Notes

### Spec Decisions Summary
From the full spec chain (Platform → CluicheTest App → TestStages System → Visual Feedback Feature):

- **PD-001** StringCRC for all IDs — registry keyed by `StringCRC`, HUD module `kTypeId` is `StringCRC`.
- **PD-004** No STL in public APIs — `TestResultsRegistry` public API uses `DynamicArrayC`, not `std::vector`. `std::mutex` is in the private impl only.
- **AD-001** Three PUs — HUD module runs on **MainPU** (ImGui access). Registry is a singleton written from SimPU stage modules, read from MainPU HUD.
- **AD-005** Testbed not product — HUD is developer tooling; orchestrator suppression (AC-VF7/16) keeps it out of CI.
- **SD-TS-004** All stages return to Boot — the ✕ button is the user-facing mechanism; no automatic exit on pass/fail.
- **ImGui pattern** — `DebugUIModule` already calls `Dia::ImGui::NewFrame(dt)` on MainPU. HUD's `DoUpdate` must check `mDebugUI.Get()->IsFrameActive()` before any `ImGui::*` calls, same as `BootMenuModule`.
- **Exit command** — `Dia::API::ExecuteCommandJson("dia.automation.navigate_to", {{"target","Boot"}})` is the correct call; matches what `BootMenuModule` uses for navigation.
- **Registry already exists** — `TestResultsRegistry.h/.cpp` and `TestStageHUDModule.h/.cpp` are present in `Cluiche/CluicheTest/Modules/TestStages/` and registered in `CluicheTest.vcxproj`. No new files needed for tasks 1 and 2 — only implementation needed.
- **RigidBody2DTestModule already wired** — `SetRunning`, `SetPassed`, `SetTimeout` calls already present in `RigidBody2DTestModule.cpp`. Task 6 covers remaining stage modules only.
- **Tests location** — `Cluiche/CluicheTest/Modules/TestStages/Testing/` per project convention. Will need vcxproj entries in `GoogleTests.vcxproj`.
- **Boot menu** — `BootMenuModule` (not `BootUIPageModule`) is the correct class. It already queries `TestResultsRegistry` for badge state via `GetStatusLabel()`. Task 8 is a no-op if already wired — verify first.

## Implementation Patterns

### HUD Bottom Bar (ImGui)
```cpp
void TestStageHUDModule::DoUpdate(float dt)
{
    DIA_TRACE_ZONE("TestStageHUDModule.Update", ...);
    if (!mDebugUI.Get() || !mDebugUI.Get()->IsFrameActive()) return;
    if (mAutomation.Get() && mAutomation.Get()->GetService()->IsHeartbeatActive()) return;
    RenderBottomBar();
}

void TestStageHUDModule::RenderBottomBar()
{
    ImGuiIO& io = ImGui::GetIO();
    float barHeight = 28.0f;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - barHeight));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, barHeight));
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs  // except button
                           | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##HUDBar", nullptr, flags);
    // ... stage name | checkpoints | frame counter | [X] button
    ImGui::End();
}
```

### ✕ Exit Button
```cpp
ImGui::SameLine(io.DisplaySize.x - 30.0f);
ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
if (ImGui::Button("X"))
{
    DIA_LOG_INFO("CluicheTest", "HUD exit button clicked — navigating to Boot (stage: %s, state: %s)",
        stageName.AsChar(), stateStr);
    Dia::API::ExecuteCommandJson("dia.automation.navigate_to", {{"target", "Boot"}});
}
ImGui::PopStyleColor();
```

### PASS / TIMEOUT highlight
```cpp
if (state == StageResult::State::kPassed)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.4f, 0.0f, 0.85f));
else if (state == StageResult::State::kTimeout)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.5f, 0.0f, 0.0f, 0.85f));
// ... render bar ...
if (state == kPassed || state == kTimeout)
    ImGui::PopStyleColor();
```

### TestResultsRegistry log pattern
```cpp
void TestResultsRegistry::SetPassed(const Dia::Core::StringCRC& name, unsigned int frame)
{
    // ... existing mutex + state update ...
    DIA_LOG_INFO("CluicheTest", "Stage '%s' PASSED at frame %u", name.AsChar(), frame);
}
```

### GoogleTest fixture pattern
```cpp
class TestResultsRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override { TestResultsRegistry::GetInstance().Reset(); }
};
TEST_F(TestResultsRegistryTest, SetRunning_StoresStateAndBudget) { ... }
```
`TestResultsRegistry` needs a `Reset()` method (test-only, clears `mResults`, resets `mActiveStage`) — add it under `#ifdef DIA_DEBUG` or as a public method gated by a comment.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Implement `TestStageHUDModule::DoUpdate` — bottom bar rendering: stage name, checkpoint icons (⏳/✓/✗), frame counter `N/M` | Bar visible during RigidBody2DStage run | Todo | sonnet | Stub exists; add `ModuleRef<DebugUIModule>`, check `IsFrameActive()`, render via ImGui |
| 2 | Add PASS (green) / TIMEOUT (red) background highlight to HUD bar | Bar colour changes on completion | Todo | haiku | Read state from `TestResultsRegistry`; colour persists until stage exits |
| 3 | Add ✕ exit button — far right of HUD bar, red, fires `dia.automation.navigate_to Boot`; hidden when `IsHeartbeatActive()` | Click navigates to Boot; button absent under orchestrator | Todo | haiku | AC-VF14, AC-VF15, AC-VF16 |
| 4 | Add `DIA_TRACE_ZONE` to `TestStageHUDModule::DoUpdate` | Zone visible in trace viewer | Todo | haiku | Observability |
| 5 | Add `DIA_LOG_INFO` to `TestResultsRegistry::SetPassed` and `::SetTimeout` | Log lines appear on state change | Todo | haiku | Observability |
| 6 | Add `DIA_LOG_INFO` to HUD ✕ button handler (stage name + final state) | Log line on click | Todo | haiku | Observability — part of task 3 impl |
| 7 | Add `TestStageHUDModule` entry to `rigidbody2d_stage.diaapp` manifest | HUD appears when navigating to RigidBody2DStage | Todo | haiku | MainPU, dependency on DebugUIModule + AutomationModule |
| 8 | Verify `BootMenuModule` already reads `TestResultsRegistry` for badges — add if missing | Badges visible on Boot menu after stage run | Todo | sonnet | Read `BootMenuModule.cpp` first; `GetStatusLabel()` already calls registry |
| 9 | Add `Reset()` method to `TestResultsRegistry` (test-only) | Fixture can isolate tests | Todo | haiku | Under `#ifdef DIA_DEBUG` |
| 10 | Write GoogleTests T1–T9 in `CluicheTest/Modules/TestStages/Testing/TestResultsRegistryTest.cpp` and `TestStageHUDModuleTest.cpp` | `dia run googletest` — all T1–T9 pass | Todo | sonnet | New files; need vcxproj entry in GoogleTests.vcxproj |
| 11 | Add test files to `GoogleTests.vcxproj` + `.vcxproj.filters` | Tests build | Todo | haiku | |
| 12 | `dia run cluichetest` — navigate to RigidBody2DStage, confirm HUD bar visible, click ✕, confirm Boot, confirm badge | Full E2E pass | Todo | sonnet | Verify step — must quote observed output |
