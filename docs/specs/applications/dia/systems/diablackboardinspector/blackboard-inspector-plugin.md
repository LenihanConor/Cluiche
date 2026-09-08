# Feature Spec: DiaBlackboardInspectorPlugin

## Parent System
@docs/specs/applications/dia/systems/diablackboardinspector/diablackboardinspector.md

**Status:** `Approved`

---

## Problem Statement

The `BlackboardInspectorSource` (Feature 2) pushes a `"blackboard.state"` JSON payload whenever board structure changes, but nothing in the editor consumes it. An editor plugin is needed that subscribes to this topic and renders a live, collapsible board list showing slot keys, field values, and observer names.

---

## Solution Overview

A new `Dia/DiaBlackboardInspector/` module — a static library containing one editor plugin class and its HTML/JS panel.

### DiaBlackboardInspectorPlugin

```cpp
// Dia/DiaBlackboardInspector/DiaBlackboardInspectorPlugin.h

namespace Dia::Editor {

class DiaBlackboardInspectorPlugin final : public LiveConnectionPluginBase
{
public:
    DiaBlackboardInspectorPlugin();

protected:
    void OnLivePluginLoad()      override;
    void OnLivePluginUnload()    override;
    void OnGameConnected()       override;
    void OnGameDisconnected()    override;

private:
    void OnBlackboardStateUpdate(const Json::Value& payload);
};

} // namespace Dia::Editor
```

Constructor passes metadata to `LiveConnectionPluginBase`:
```cpp
DiaBlackboardInspectorPlugin()
    : LiveConnectionPluginBase({
        "Blackboard Inspector", "1.0.0",
        "Live view of all registered blackboards, slots, and observers",
        "dia://plugins/blackboardinspector/index.html",
        Dia::Editor::LayoutMode::kDockable,
        nullptr, nullptr, false
      }, "blackboard_inspector")
{}
```

In the `.cpp`: `REGISTER_EDITOR_PLUGIN(DiaBlackboardInspectorPlugin, "DiaBlackboardInspector")`

### OnLivePluginLoad

```cpp
void DiaBlackboardInspectorPlugin::OnLivePluginLoad()
{
    RegisterGameTopic(
        Dia::Core::StringCRC{"blackboard.state"},
        [this](const Json::Value& payload) { OnBlackboardStateUpdate(payload); });
}
```

### OnBlackboardStateUpdate

Forwards the full payload to the HTML panel:
```cpp
GetBridge()->NotifyUIDataChanged("blackboard_inspector.state", payload);
```

The JS side receives this via the standard `window.DiaEditor_onDataChanged` hook.

### React/Vite UI panel

**Files:** `Dia/DiaBlackboardInspector/UI/` (React + Vite + TypeScript + Zustand)

Follows the same React/Vite pattern as all other `LiveConnectionPluginBase` inspectors. Built output is deployed from `dist/`. Uses `@dia/editor-ui` shared component library (`ConnectionStatus`, `EmptyState`, `theme`).

**Panel behaviour:**
- On `blackboard_inspector.state` data push: update board list in Zustand store; UI re-renders from store
- On `blackboard_inspector.connection_state` push: `setConnected(true/false)` — shows/hides disconnected overlay
- Board card: collapsible with label, id, slot count badge, observer count badge; chevron toggles expanded state
- Slot rows: `key` (`#9cdcfe`), `type` (`#4ec9b0`), `value` (monospace; dim italic when `"[no serializer]"`) with recursive JSON syntax highlighting
- Observer chips: teal chips per observer name; muted `none` chip when empty
- Filter input: client-side text filter applied to board `label` and `id` fields
- Expand/collapse all buttons: propagate `expandOverride` prop to all `BoardCard` instances

**Connection state:** Disconnected overlay rendered by the React app itself (absolute-positioned, `data-testid="disconnect-overlay"`) — consistent with `DiaEntityInspector`, `DiaAssetRuntimeInspector`, and `DiaApplicationFlowInspector`.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaBlackboardInspector/DiaBlackboardInspectorPlugin.h` | Plugin class declaration |
| `Dia/DiaBlackboardInspector/DiaBlackboardInspectorPlugin.cpp` | Implementation + `REGISTER_EDITOR_PLUGIN` macro |
| `Dia/DiaBlackboardInspector/UI/package.json` | React/Vite/Zustand/TypeScript dependencies |
| `Dia/DiaBlackboardInspector/UI/vite.config.ts` | Vite build config (root=src, outDir=../dist) |
| `Dia/DiaBlackboardInspector/UI/tsconfig.json` | TypeScript compiler options |
| `Dia/DiaBlackboardInspector/UI/src/index.html` | Minimal HTML shell |
| `Dia/DiaBlackboardInspector/UI/src/main.tsx` | React entry point + `injectThemeVars()` |
| `Dia/DiaBlackboardInspector/UI/src/App.tsx` | EIRM-003 bridge wiring, disconnect overlay, board list |
| `Dia/DiaBlackboardInspector/UI/src/store.ts` | Zustand store (connected, boards, filterText) |
| `Dia/DiaBlackboardInspector/UI/src/types.ts` | BoardEntry, SlotEntry, ObserverEntry |
| `Dia/DiaBlackboardInspector/UI/src/components/BoardCard.tsx` | Collapsible board card |
| `Dia/DiaBlackboardInspector/UI/src/components/SlotTable.tsx` | Slot rows with JSON syntax highlighting |
| `Dia/DiaBlackboardInspector/UI/src/components/ObserverList.tsx` | Observer chips |
| `Dia/DiaBlackboardInspector/UI/src/App.test.tsx` | Vitest + RTL tests |
| `Dia/DiaBlackboardInspector/UI/src/test/setup.ts` | Vitest/jsdom setup |
| `Dia/DiaBlackboardInspector/DiaBlackboardInspector.vcxproj` | Static library; Debug/Release/Asan/Ubsan x64 |
| `Dia/DiaBlackboardInspector/DiaBlackboardInspector.vcxproj.filters` | IDE filter file |
| `Dia/DiaBlackboardInspector/Docs/dia.blackboardinspector.architecture.module.md` | YAML module doc |
| `Cluiche/Cluiche.sln` | Register new project under the editor solution folder |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | Plugin registers under name `"DiaBlackboardInspector"` via `REGISTER_EDITOR_PLUGIN` | Build + `dia run cluicheeditor` — plugin appears in plugin browser |
| 2 | Plugin subscribes to `"blackboard.state"` topic in `OnLivePluginLoad` | Code review |
| 3 | On `blackboard_inspector.state` data update, panel renders one collapsible row per board | Manual: connect editor to running game, verify boards appear |
| 4 | Each board row shows `label`, `id`, slot count badge, observer count badge | Manual |
| 5 | Expanding a board row shows slot `.prop-row` entries with key, type, and value columns | Manual |
| 6 | Slots with a serializer show a populated value; slots without show `[no serializer]` in dim italic | Manual |
| 7 | Observer chips appear below slots for each board; `none` chip shown when observers list is empty | Manual |
| 8 | Filter input hides boards whose `label` and `id` do not match the filter string (client-side) | Manual |
| 9 | Expand-all and collapse-all buttons correctly toggle all board rows | Manual |
| 10 | Panel is a React/Vite app using `@dia/editor-ui` (`ConnectionStatus`, `EmptyState`, `theme`) — consistent with `DiaEntityInspector`, `DiaAssetRuntimeInspector`, `DiaApplicationFlowInspector` | Code review |
| 11 | `DiaBlackboardInspector.vcxproj` does not override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` | Code review |
| 12 | `dia pipeline --target cluicheeditor` passes — plugin compiles into the editor | Build verification |
| 13 | Panel URL `"dia://plugins/blackboardinspector/index.html"` (Vite build output) resolves correctly in the editor frame | Manual: open panel in editor, no 404 |
| 14 | `dia.blackboardinspector.architecture.module.md` exists with `layer`, `deps`, and `public_api` sections | Code review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `DiaBlackboardInspector` module skeleton — directory, vcxproj, vcxproj.filters, module.md, Cluiche.sln entry | — | Static library; follow DiaEntityInspector as reference |
| 2 | Implement `DiaBlackboardInspectorPlugin.h/.cpp` — constructor, `OnLivePluginLoad`, `OnLivePluginUnload`, `OnGameConnected`, `OnGameDisconnected`, `OnBlackboardStateUpdate`, `REGISTER_EDITOR_PLUGIN` | 1 | No sub-controllers needed in v1 |
| 3 | Add `.h/.cpp` to `DiaBlackboardInspector.vcxproj` + `.filters` | 2 | Use `dia docs vcxproj-add` |
| 4 | Write `UI/index.html` panel — board list, slot rows, observer chips, filter, expand/collapse; use mockup as acceptance gate | 2 | Match `mockup/blackboard-inspector.html` style exactly |
| 5 | `dia pipeline --target cluicheeditor` — build passes | 3, 4 | — |
| 6 | Manual E2E: `dia run cluichetest`, connect editor, verify panel shows registered boards | [blackboard-inspector-source](blackboard-inspector-source.md) done | Full vertical slice smoke test |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-005 | x64 only | `DiaBlackboardInspector.vcxproj` targets x64 exclusively |
| PD-006 | VS project files are source of truth | `DiaBlackboardInspector.vcxproj` + `.filters` manually maintained |
| PD-007 | C++20 required | Compiled under `/std:c++20` |
| PD-008 | `Directory.Build.props` owns build paths | `DiaBlackboardInspector.vcxproj` does not override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` |
| PD-009 | Generated output under `Cluiche/out/` | Panel HTML deployed to `Cluiche/out/CluicheEditor/plugins/blackboardinspector/` by editor pipeline |
| AD-001 | Module YAML frontmatter | `dia.blackboardinspector.architecture.module.md` required |
| AD-003 | Namespace `Dia::<Module>::` | Plugin class in `Dia::Editor::` namespace, matching all other editor plugins |
