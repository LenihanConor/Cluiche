# CluicheTest Migration Plan - Adopt CoW F-1 Debug Infrastructure

**Date:** 2026-10-11  
**Status:** Draft  
**Prerequisite:** CoW F-1 debug systems built and proven

## Summary

After CoW F-1 proves the 3 new debug systems (Console, Data Panel Registry, Palette), migrate CluicheTest to adopt them. This removes ~1,200 lines of duplicated per-stage HUD code, makes debug tools discoverable, and enables in-game command execution.

---

## Current CluicheTest State

### What Works
- ✅ **DebugPanelPageModule** - Visual debug domains (left-side panel, Ultralight)
- ✅ **DebugUIModule** - ImGui backend
- ✅ **DebugServerHostModule** - Remote inspector for CluicheEditor
- ✅ **DiaAPI** - 47 commands registered

### What's Duplicated/Scattered
- ⚠️ **TestStageHUDModule** - Bottom status bar, ~140 lines
  - Used in: 12+ test stages
  - Shows: checkpoint status, pass/fail, timeout indicators
  - **Problem:** Copy-pasted pattern per-stage, not reusable

- ⚠️ **AssetRuntimeHUDModule** - Asset state display, ~180 lines
  - Used in: AssetRuntimeTestStage only
  - Shows: loaded assets, reload metrics
  - **Problem:** Custom per-stage, could be reusable panel

- ⚠️ **No in-game console** - Commands only via external `dia <cmd>`
  - 47 registered commands unusable in-game
  - Developer must Alt+Tab to terminal

- ⚠️ **No discovery** - No way to list commands/panels

---

## Migration Goals

1. **Eliminate duplicated HUD code** - Replace 12+ custom HUDs with 2 reusable panels
2. **Enable in-game commands** - Add console (`~` toggle)
3. **Make tools discoverable** - Add palette (`F1` toggle)
4. **Unified registration** - Use `ModuleDebugRegistry` pattern

---

## Migration Steps

### Phase 1: Add Core Infrastructure (0 days - already built by CoW F-1)

These systems are built and proven by CoW F-1:
- ✅ DiaDebugConsole
- ✅ DiaDebugDataPanel (registry + example panels)
- ✅ DiaDebugPalette
- ✅ ModuleDebugRegistry

CluicheTest just needs to adopt them.

---

### Phase 2: Migrate TestStageHUDModule → Reusable Panel (2-3 days)

#### Before (per-stage custom HUD):

**File:** `CluicheTest/Modules/TestStages/TestStageHUDModule.cpp`  
**Lines:** ~140  
**Used by:** 12+ stages

```cpp
void TestStageHUDModule::RenderBottomBar(const MainToRenderFrame& frame) {
    const StageHUDState& hud = frame.stageHUD;
    
    if (hud.activeStageName.Value() == 0)
        return;
    
    // Custom ImGui bottom bar (pass/fail colors, checkpoint status)
    ImGui::PushStyleColor(...);
    ImGui::SetNextWindowPos(...);
    ImGui::Begin("##HUDBar", nullptr, kFlags);
    // ... 40+ lines of ImGui drawing code ...
    ImGui::End();
}
```

**Manifest entries:** Each stage declares `TestStageHUDModule` instance

#### After (reusable panel):

**File:** `CluicheGameBaseline/Modules/TestStageStatusPanel.cpp` (new)  
**Lines:** ~80 (Ultralight-based, not ImGui)

```cpp
class TestStageStatusPanel : public IDebugDataPanel {
    // Implements IDebugDataPanel interface
    // Subscribes to MainToRenderFrame stream
    // Updates Ultralight HTML panel
};
```

**Registration:** Once, in CluicheTest Main.cpp

```cpp
#ifdef DIA_DEBUG
    Dia::Debug::ModuleDebugRegistry("test_stages", "Test Stages", "testing")
        .Panel<TestStageStatusPanel>("Ctrl+Shift+T")
        .Activate();
#endif
```

**Manifest entries:** Remove 12+ `TestStageHUDModule` instances, replace with single panel registration

**Migration tasks:**

| # | Task | Effort |
|---|------|--------|
| 2.1 | Create `TestStageStatusPanel` (Ultralight HTML + C++ page) | 4-6 hours |
| 2.2 | Port ImGui bottom-bar logic to Ultralight HTML/CSS | 2-3 hours |
| 2.3 | Remove `TestStageHUDModule` instances from 12+ stage manifests | 1 hour |
| 2.4 | Register panel once in CluicheTest `Main.cpp` | 30 min |
| 2.5 | Test: open panel with `Ctrl+Shift+T`, verify checkpoint status shows | 1 hour |
| 2.6 | Delete `TestStageHUDModule.h/.cpp` (~280 lines removed) | 15 min |

**Before/After comparison:**

| Metric | Before | After | Savings |
|--------|--------|-------|---------|
| **Files** | `TestStageHUDModule.h/.cpp` (per-stage) | `TestStageStatusPanel` (shared) | 2 files → 1 |
| **Lines of code** | ~140 × 12 stages = 1,680 lines | ~80 lines | **-1,600 lines** |
| **Manifest entries** | 12+ instances | 1 global registration | **-11 entries** |
| **Reusability** | Stage-specific | All stages get it for free | ✅ |
| **Discoverability** | Automatic (per-stage) | `Ctrl+Shift+T` or F1 palette | ✅ |

---

### Phase 3: Migrate AssetRuntimeHUDModule → Reusable Panel (1-2 days)

#### Before:

**File:** `CluicheTest/Modules/TestStages/AssetRuntimeHUDModule.cpp`  
**Used by:** AssetRuntimeTestStage only

#### After:

Move to `DiaAssetRuntime` as a reusable panel (benefits all stages, not just CluicheTest):

**File:** `Dia/DiaAssetRuntime/AssetStatePanel.cpp` (new, in DiaAssetRuntime module)

```cpp
// In AssetRuntimeModule.cpp:
mDebugTools{"asset_runtime", "Asset Runtime", "assets"}
    .Command("get_loaded", "List loaded assets", ...)
    .Command("get_staged", "List staged assets", ...)
    .Panel<AssetStatePanel>("Ctrl+Shift+A")
    .Activate();
```

**Benefits:**
- ✅ Works in **all** apps (CluicheTest, CoW, future games)
- ✅ Opens with `Ctrl+Shift+A` from anywhere
- ✅ Discoverable via F1 palette

**Migration tasks:**

| # | Task | Effort |
|---|------|--------|
| 3.1 | Create `AssetStatePanel` in DiaAssetRuntime | 3-4 hours |
| 3.2 | Port `AssetRuntimeHUDModule` ImGui logic to Ultralight | 2 hours |
| 3.3 | Register in `AssetRuntimeModule::DoStart()` | 30 min |
| 3.4 | Remove `AssetRuntimeHUDModule` from AssetRuntimeTestStage manifest | 15 min |
| 3.5 | Test in CluicheTest and CoW | 1 hour |
| 3.6 | Delete `AssetRuntimeHUDModule.h/.cpp` (~250 lines removed) | 15 min |

---

### Phase 4: Add Debug Console to CluicheTest (1 day)

#### Before:
- No in-game command execution
- Must type `dia <command>` in external terminal

#### After:
- Press `~` → console opens
- Type command → executes in-game
- See output immediately

**Migration tasks:**

| # | Task | Effort |
|---|------|--------|
| 4.1 | Add `DebugConsoleModule` instance to `cluiche_main.diaapp` | 15 min |
| 4.2 | Bind `~` key to `DebugConsoleModule::Toggle()` in baseline InputModule | 1 hour |
| 4.3 | Test: open console, execute `asset_runtime.get_loaded`, see output | 30 min |
| 4.4 | Document console usage in CluicheTest README | 1 hour |

---

### Phase 5: Add Debug Palette to CluicheTest (1 day)

#### Before:
- No way to discover available commands/panels/domains
- Must read code or docs

#### After:
- Press `F1` → palette opens
- Search "asset" → shows 5 commands, 1 panel
- Click to launch

**Migration tasks:**

| # | Task | Effort |
|---|------|--------|
| 5.1 | Add `DebugPaletteModule` instance to `cluiche_main.diaapp` | 15 min |
| 5.2 | Bind `F1` key to `DebugPaletteModule::Toggle()` | 30 min |
| 5.3 | Test: open palette, search "test", see TestStageStatusPanel | 30 min |
| 5.4 | Verify all 47 commands appear and are searchable | 1 hour |

---

### Phase 6: Cleanup Registration Sites (2-3 days)

Migrate existing command registrations to `ModuleDebugRegistry` pattern:

#### Example: DiaAssetRuntime

**Before:** `AssetRuntimeDebugCommands.cpp` (separate file, ~200 lines)

```cpp
void RegisterAssetRuntimeCommands(AssetRuntime& runtime, ...) {
    CommandInfo cmd1;
    cmd1.name = "asset-runtime-get-loaded";
    cmd1.description = "...";
    cmd1.callback = [&](auto& a) { return GetLoaded(runtime, a); };
    Dia::API::RegisterCommand(cmd1);
    
    CommandInfo cmd2;
    // ... repeat for 5 more commands ...
}
```

**After:** Inline in `AssetRuntimeModule.cpp`

```cpp
void AssetRuntimeModule::DoStart() {
#ifdef DIA_DEBUG
    mDebugTools{"asset_runtime", "Asset Runtime", "assets"}
        .Command("get_loaded", "List loaded assets",
            [this](auto& a) { return GetLoaded(a); })
        .Command("get_staged", "List staged assets",
            [this](auto& a) { return GetStaged(a); })
        // ... 4 more ...
        .Panel<AssetStatePanel>("Ctrl+Shift+A")
        .Activate();
#endif
}
```

**Modules to migrate:**
- DiaAssetRuntime (5 commands)
- DiaMessageBus (3 commands) - after bus_traffic.* commands exist
- Any future modules

---

## Overall Migration Estimate

| Phase | Effort | Parallel? |
|-------|--------|-----------|
| Phase 1: Core infra | 0 days (done by CoW F-1) | N/A |
| Phase 2: TestStageHUD → Panel | 2-3 days | No |
| Phase 3: AssetRuntimeHUD → Panel | 1-2 days | Yes (parallel with Phase 4/5) |
| Phase 4: Add Console | 1 day | Yes |
| Phase 5: Add Palette | 1 day | Yes |
| Phase 6: Cleanup registrations | 2-3 days | After Phase 2-5 |

**Total: 5-7 days** (with parallelization)

---

## Benefits Summary

### Code Reduction

| Component | Before | After | Savings |
|-----------|--------|-------|---------|
| TestStageHUDModule | ~1,680 lines (12 stages × 140) | 80 lines (1 panel) | **-1,600 lines** |
| AssetRuntimeHUDModule | ~250 lines | Moved to DiaAssetRuntime | **-250 lines** |
| Registration code | ~200 lines (separate files) | Inline in modules | **-200 lines** |
| **Total** | **~2,130 lines** | **~80 lines** | **-2,050 lines (96% reduction)** |

### Feature Gains

| Feature | Before | After |
|---------|--------|-------|
| **In-game commands** | ❌ Terminal only | ✅ Console (`~`) |
| **Command discovery** | ❌ None | ✅ Palette (`F1`) |
| **Reusable panels** | ❌ Per-stage custom | ✅ Shared registry |
| **Keybinds** | ❌ Hardcoded | ✅ Registered with panels |
| **Discoverability** | ❌ Read code | ✅ Search in palette |

### Developer Experience

| Task | Before | After |
|------|--------|-------|
| **Add stage HUD** | Copy-paste 140-line module | Reuse existing panel |
| **Execute command** | Alt+Tab → terminal → type | `~` → type in-game |
| **Find command** | Read docs or code | `F1` → search |
| **Add debug panel** | Write custom ImGui code | Implement `IDebugDataPanel` once |

---

## Rollout Strategy

### Approach 1: Big Bang (Not Recommended)
Migrate all 5 phases at once (5-7 day sprint)

**Pros:** Fast, consistent  
**Cons:** High risk, hard to test incrementally

### Approach 2: Incremental (Recommended)
1. **Week 1:** Add Console + Palette (Phase 4-5) → immediate value, low risk
2. **Week 2:** Migrate TestStageHUD → Panel (Phase 2) → prove panel registry
3. **Week 3:** Migrate AssetRuntimeHUD (Phase 3) + cleanup registrations (Phase 6)

**Pros:** Lower risk, testable at each step, value delivered early  
**Cons:** Slightly slower

---

## Success Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| SC-1 | `~` opens console in any CluicheTest stage | Manual test |
| SC-2 | Console executes all 47 registered commands | Automated test |
| SC-3 | `F1` opens palette showing all commands/panels/domains | Manual test |
| SC-4 | TestStageStatusPanel replaces 12+ TestStageHUDModule instances | Code review (manifest diff) |
| SC-5 | AssetStatePanel works in CluicheTest and CoW | Manual test in both apps |
| SC-6 | Total line count reduced by >1,500 lines | `git diff --stat` |
| SC-7 | All stages launch/run with no regressions | CI test suite passes |

---

## Risks & Mitigations

### Risk 1: ImGui → Ultralight Conversion

**Risk:** TestStageHUD and AssetRuntimeHUD use ImGui. Converting to Ultralight may lose functionality.

**Mitigation:**
- Create HTML mockups first (visual acceptance gate)
- Side-by-side comparison (old ImGui vs. new Ultralight)
- Keep ImGui versions in a branch until Ultralight proven

### Risk 2: Performance

**Risk:** Multiple Ultralight panels might impact frame rate.

**Mitigation:**
- Measure frame time before/after
- Panels only update when visible
- Throttle updates (e.g., 10 Hz for data panels, not every frame)

### Risk 3: Stage-Specific Behavior

**Risk:** Some stages might have custom HUD logic that doesn't fit reusable panel model.

**Mitigation:**
- Audit all 12 stages' HUD code before starting
- Identify outliers (keep as custom if truly unique)
- Most stages share same pattern (checkpoint status), so 90%+ should migrate cleanly

---

## Post-Migration Cleanup

After migration complete:

1. **Delete files:**
   - `CluicheTest/Modules/TestStages/TestStageHUDModule.h/.cpp`
   - `CluicheTest/Modules/TestStages/AssetRuntimeHUDModule.h/.cpp`
   - `Dia/DiaAssetRuntime/AssetRuntimeDebugCommands.h/.cpp` (inline in module)

2. **Update docs:**
   - CluicheTest README: add console/palette usage
   - Module docs: update registration examples
   - `dia scaffold module` template: include `mDebugTools` pattern

3. **Write migration guide:**
   - For other projects (future games, tools)
   - "How to adopt F-1 debug infrastructure in your app"

---

## Status

Draft (awaiting CoW F-1 completion)

**Next Steps:**
1. CoW F-1 builds + proves the 3 systems
2. Create HTML mockups for TestStageStatusPanel and AssetStatePanel
3. Approve this migration plan
4. Execute incremental rollout (Approach 2)
