# System Spec: DiaAssetRuntimeVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug, core

## Purpose

DiaAssetRuntimeVisualDebugger is the `IDebugDomain` implementation that makes asset load state visible in `DiaDebugPanel`. It surfaces loaded/streaming/failed counts and per-type breakdowns as panel stats.

The live implementation is `AssetRuntimeDebugDomain` in `Dia/DiaAssetRuntimeVisualDebugger/`. The `DiaAssetRuntimeVisualDebugger` drawer has a stub `Draw()` — no world-space output. The critical gap is that `GetJSONState()` is not wired to any actual load-state data; the panel card is effectively empty.

**Prerequisite:** `debug-query-api.md` in `diaassetruntime/` defines `GetLoadedAssets()` and `GetStagedAssets()` — both already implemented. These are the data source for `GetJSONState()`.

**Migration:** See `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"AssetRuntime"` |
| Display name | `"AssetRuntime"` |
| Description | `"Asset load state — loaded, streaming, failed, per-type counts"` |
| Group | `"CoreDebug"` |
| Accent | `DebugGroupAccents::kCoreDebug` (`#6b7280`) |
| `HasWorldDrawers()` | `true` (drawer exists; currently a stub — see SD-001) |
| Drawers | AssetRuntime |

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Core Debug — accent `DebugGroupAccents::kCoreDebug` (`#6b7280`) via `var(--accent)`
- **Spacing:** group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between cards, drawer rows `gap: 5px 10px`
- **Stat line:** `"Assets: L loaded, S streaming, F failed"`
- **Expanded body:**
  - Drawer toggle: AssetRuntime
  - Per-type breakdown table: one row per non-empty asset type — type name, loaded count, streaming count, failed count
  - Failed rows highlighted in accent color

## JSON State Schema

```json
{
  "drawers": [
    { "name": "AssetRuntime", "enabled": true }
  ],
  "stats": {
    "loaded":    42,
    "streaming":  5,
    "failed":     1,
    "total":     48
  },
  "byType": [
    { "type": "mesh3d",  "loaded": 18, "streaming": 3, "failed": 0 },
    { "type": "texture", "loaded": 20, "streaming": 2, "failed": 1 },
    { "type": "audio",   "loaded":  4, "streaming": 0, "failed": 0 }
  ]
}
```

## Domain ACs

In addition to all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`:

| AC | Criterion |
|----|-----------|
| D-1 | `"stats.loaded"` = count of assets in `Ready` state across all types |
| D-2 | `"stats.streaming"` = count of assets in `Pending` state |
| D-3 | `"stats.failed"` = count of assets in `Failed` state; panel highlights in accent color when > 0 |
| D-4 | `"stats.total"` = loaded + streaming + failed |
| D-5 | `"byType[]"` has one entry per non-empty asset type; derived from iterating all registered asset handlers |
| D-6 | No `DrawImGui()` call anywhere in this module |
| D-7 | Panel card layout complies with AC-16 spacing contract (see Panel Card Specification above) |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `DiaAssetRuntimeVisualDebugger.Draw()` remains a stub | No world-space anchor for asset load state. A future "hotspot" overlay (label at failed-asset world position) would require an asset→world-pos mapping that isn't architecturally available. Defer. | Accepted | No |
| SD-002 | Memory usage stats deferred | Requires `AssetHandler::GetMemoryUsageBytes()` which doesn't exist. Defer to v2. | Accepted | No |

## Open Design Questions

1. **`OnCommand("reloadFailed", {})`** — if `AssetRuntime::ReloadFailed()` is added, the panel "Failed" row could show a reload button. Defer to when that accessor is implemented.

## Status

**Status:** Approved
