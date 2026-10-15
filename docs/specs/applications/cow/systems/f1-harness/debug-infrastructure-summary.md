# CoW F-1 Debug Infrastructure Summary

**Date:** 2026-10-11  
**Status:** Ready for review

## What F-1 Drives

CoW F-1 Harness testbed exposes **3 gaps in in-game debug tooling** that drive building 3 new Dia systems:

| Gap | System | Mockup | Spec | What It Does |
|-----|--------|--------|------|--------------|
| No in-game command execution | **DiaDebugConsole** | [debug-console.html](mockups/debug-console.html) | [debug-console.md](debug-console.md) | Text console for executing DiaAPI commands (toggle: `~`) |
| No registry for data panels | **DiaDebugDataPanel** | [bus-traffic-panel.html](mockups/bus-traffic-panel.html) | [debug-data-panel.md](debug-data-panel.md) | Registry for in-game data tables/logs (proof: Bus Traffic Viewer) |
| No debug tool discovery | **DiaDebugPalette** | [debug-palette.html](mockups/debug-palette.html) | [debug-palette.md](debug-palette.md) | Searchable launcher for commands/panels/domains (toggle: `F1`) |

---

## Mockup Previews

### 0. Multi-Panel Layout (Full System in Action)
[**→ View mockup: multi-panel-layout.html**](mockups/multi-panel-layout.html)

Shows the complete expandable UX when multiple systems have registered their debug tools:
- **Left dock:** Visual Debug Domains + Blackboard Inspector (stacked)
- **Right dock:** Bus Traffic Viewer
- **Floating:** Entity List (draggable)
- **Bottom:** Console (docked)
- **Center:** Game viewport

**Demonstrates:** How panels dock/float, resize, and coexist without conflict.

---

### 1. Debug Console (`~` to toggle)
![Console preview: multi-tab interface with output log, command input, autocomplete, history sidebar]

**Features:**
- Command input with TAB autocomplete
- Multi-tab: Output, History, Commands, Help
- Log filtering (All, Info, Warnings, Errors)
- Recent commands + quick actions sidebar
- Context panel (stage, tick, command count)

**F-1 Proof:** Execute `bus_traffic.start recording.dat` in-game, see output immediately

---

### 2. Bus Traffic Viewer Panel
![Panel preview: sortable table of messages with stats bar]

**Features:**
- Live message table (tick, type, router, pass, count, size)
- Toolbar: Live/Pause, Primary/Reaction filters
- Stats bar: total messages, msg/sec, unique types/routers
- Recording indicator (pulsing red badge)
- Sortable columns

**F-1 Proof:** View live bus traffic without external tools; verify F-1 Test 6 (recorded traffic survives the run)

---

### 3. Debug Palette (`F1` to toggle)
![Palette preview: searchable list with category tabs]

**Features:**
- Fuzzy search across all debug tools
- Category tabs (All, Commands, Panels, Domains)
- Keyboard navigation (↑↓ + Enter)
- Shows keybinds and descriptions
- Grouped results (commands, panels, domains)

**F-1 Proof:** Search "bus" → discover all bus-related commands and panels

---

## Technology Stack

All 3 systems use **Ultralight** (HTML/CSS/JS) for consistency:
- Matches existing `DiaDebugPanel` (visual debug domains)
- Allows rich UI (tables, search, keyboard nav)
- Supports drag-to-move + docking

---

## Implementation Order

**Recommended build sequence:**

1. **DiaDebugConsole** (5-7 days)
   - Core infrastructure: command execution, log capture
   - Proves DiaAPI → Ultralight bridge
   - Needed by: Debug Palette (pre-fills commands)

2. **DiaDebugDataPanel** (4-6 days)
   - Registry + `IDebugDataPanel` interface
   - Bus Traffic Viewer proof
   - Validates panel architecture

3. **DiaDebugPalette** (3-5 days)
   - Depends on: Console + Panel registry
   - Unified discovery across all 3 systems

**Total estimate:** 12-18 days

---

## How They Work Together

```
User presses F1
  ↓
Debug Palette opens
  ↓
User types "bus"
  ↓
Palette shows:
  - bus_traffic.start (Command)
  - bus_traffic.stop (Command)
  - bus_traffic.status (Command)
  - Bus Traffic Viewer (Panel)
  ↓
User selects "bus_traffic.start"
  ↓
Palette closes, Debug Console opens with "bus_traffic.start" pre-filled
  ↓
User types " recording.dat", presses Enter
  ↓
Console executes command, shows output:
  "Bus traffic recorder started: C:/out/CoW/Debug/recording.dat"
  ↓
User presses F1 again, types "bus traffic viewer"
  ↓
Selects "Bus Traffic Viewer" panel
  ↓
Panel opens, shows live table of messages
```

---

## What Exists vs. What's Missing

### ✅ Exists (Infrastructure)
- `DiaAPI` - command registry + execution
- `DiaUI` - Ultralight rendering
- `DiaObservation` - log sinks
- `IDebugDomain` - visual debug draw
- `DebugServer` - remote editor connection

### ❌ Missing (New Systems)
- `DiaDebugConsole` - **NEW** (spec written)
- `DiaDebugDataPanel` - **NEW** (spec written)
- `DiaDebugPalette` - **NEW** (spec written)

---

## Next Steps

1. **Review mockups** - Open HTML files in browser, validate UX
2. **Review specs** - Check ACs, design, dependencies
3. **Approve specs** - Mark status from `Draft` → `Approved`
4. **Write plans** - Task breakdown for each system
5. **Build** - Implement in order (Console → Panel → Palette)
6. **Verify** - F-1 testbed proves all 3 with bus_traffic stand-ins

---

## Files Created

**Mockups:**
- `docs/specs/applications/cow/systems/f1-harness/mockups/multi-panel-layout.html` ⭐ (full system)
- `docs/specs/applications/cow/systems/f1-harness/mockups/debug-console.html`
- `docs/specs/applications/cow/systems/f1-harness/mockups/bus-traffic-panel.html`
- `docs/specs/applications/cow/systems/f1-harness/mockups/debug-palette.html`

**Feature Specs:**
- `docs/specs/applications/cow/systems/f1-harness/debug-console.md`
- `docs/specs/applications/cow/systems/f1-harness/debug-data-panel.md`
- `docs/specs/applications/cow/systems/f1-harness/debug-palette.md`

**Updated:**
- `docs/specs/applications/cow/systems/f1-harness/f1-harness.md` (clarified what F-1 drives)

**Additional Docs:**
- `docs/specs/applications/cow/systems/f1-harness/registration-improvements.md` (cleanup existing patterns)
- `docs/specs/applications/cow/systems/f1-harness/cluichetest-migration-plan.md` (adopt F-1 infrastructure)
