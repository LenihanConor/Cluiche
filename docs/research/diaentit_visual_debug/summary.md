# Research Summary — DiaEntity Visual Debugger & Editor Options

**Session folder:** docs/research/diaentit_visual_debug/
**Date:** 2026-05-24

## One-Line Answer

Three system targets covering all seven candidates: two DiaVisualDebugger feature specs (picking + labels), a new DiaEntityEditor system (inspector panel, query browser, mailbox monitor, watch list), and a new DiaEntityBlueprintEditor system (static blueprint authoring) — with a DiaCLI schema export as interim mitigation while the blueprint editor is built.

## Journey

1. **Explored:** DiaEntity exposes rich runtime data (entity population, reflected component fields, hierarchy, query caches, mailbox) but none of it is currently visible to a developer. Three partially-built seams already exist across DiaEntity, DiaVisualDebugger, and DiaEditor — the challenge is connecting them, not inventing them.
2. **Ideated:** 8 candidates generated (C1 dropped early as redundant to C2) spanning in-game ImGui, editor panels, viewport picking, draw overlays, query browser, mailbox monitor, watch list, and static blueprint authoring. Scopes ranged from S (<=1 week) to L (1-2 months).
3. **Evaluated:** Scored for sequencing priority rather than elimination. C3 (viewport picking, 4.80) and C4 (overlay, 4.40) scored highest. C2 (editor panel, 3.95) is the canonical long-term surface. C7 (blueprint editor, 2.70) scored lowest but user confirmed all candidates should be specced.
4. **Chose:** All remaining candidates, organised into three system targets. No candidates discarded.

## Three System Targets

### 1. DiaVisualDebugger (additions to existing Approved system)
- `debug-entity-picking` (C3, S) ✓ Approved
- `entity-labels-draw-class` (C4, S) ✓ Approved

### 2. DiaEntityEditor (new system — CluicheEditor plugin)
- `entity-inspector-panel` (C2, M)
- `query-browser-tab` (C5, S)
- `mailbox-traffic-monitor` (C6, M)
- `entity-watch-list` (C8, M)

**Home module:** `Dia/DiaEntityEditor/` ✓ System spec Approved

### 3. DiaEntityBlueprintEditor (new system — CluicheEditor plugin)
- `export-component-schema` DiaCLI prereq (S)
- `blueprint-file-editor` (C7, L)

**Home module:** `Dia/DiaEntityBlueprintEditor/`

## UI Mockup Decision

**Chosen layout:** Option B — Split panel (master-detail)
**File:** docs/research/diaentit_visual_debug/mockup_b_split.html

Persistent 30% entity list (left) + 70% detail area with sub-tabs Fields/Queries/Mailbox/Watch (right). Entity list always visible. This mockup is the visual acceptance gate for all four DiaEntityEditor feature specs.

## Key Insights from Exploration

- **Three seams already exist, zero infrastructure to invent.** `IEntityInspectable`, `SetSelectedEntityId()` / `debug.pick`, and `GameConnectionManager` subscribe model are all built.
- **Viewport picking is system-agnostic.** `DebugPrimitive::entityId` is a `uint32_t`. The picking mechanic works for any system that tags its primitives.
- **C3 + C4 are the highest-leverage first step.** S-sized, zero editor dependency, live in two weeks.
- **DiaEntityEditor is the long-term canonical surface.** C2 is the foundation; C5/C6/C8 stack on top cleanly.
- **Blueprint editor needs a DiaCLI schema export first.** Without it, C7 cannot access `ComponentTypeDesc` metadata without a running game.
- **IEntityInspectable needs additive amendments.** C5 needs query enumeration; C8 needs stable-ID lookup; `entity-labels-draw-class` needs `GetDebugName`. All additive — no breaking changes.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| 1 — In-game ImGui Inspector | Redundant once C2 (editor panel) ships. |

## Spec Sequencing

```
Done:
  debug-entity-picking          (DiaVisualDebugger)
  entity-labels-draw-class      (DiaVisualDebugger)
  DiaEntityEditor system spec   

Next:
  /spec-feature entity-inspector-panel    (DiaEntityEditor)
  /spec-feature query-browser-tab         (DiaEntityEditor)
  /spec-feature mailbox-traffic-monitor   (DiaEntityEditor)
  /spec-feature entity-watch-list         (DiaEntityEditor)
  /spec-system  DiaEntityBlueprintEditor
```

## References

- docs/research/diaentit_visual_debug/explore.md
- docs/research/diaentit_visual_debug/ideate.md
- docs/research/diaentit_visual_debug/evaluate.md
- docs/research/diaentit_visual_debug/choose.md
- docs/research/diaentit_visual_debug/mockup_b_split.html
- docs/research/entity_system/summary.md
- docs/specs/systems/dia/diaentity.md
- docs/specs/systems/dia/diavisualdebugger.md
- docs/specs/systems/dia/diaeditor.md
- docs/specs/systems/dia/diaentityeditor.md
