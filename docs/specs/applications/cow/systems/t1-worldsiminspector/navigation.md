# Feature Spec: T-1 Navigation

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/time-series-and-events.md (Feature 2)

**Gameplay Domains:** debug

## Summary

Lets T-1 move the live game's viewer to what the user is looking at. Clicking an event, a breakdown row, a mini-map cell or "show cells" becomes a short list of DiaCellLayers / DiaCellHistory commands sent to the game. The feature provides one typed navigation model, one bridge call that sends the commands, gating by source and world, failure feedback, and an in-sync indicator. It is read-and-navigate only: no tuning, no simulation control.

## Problem

Seeing a spike in a chart is only half the job; the user then needs the game to show *that place at that time*. Without navigation, they would read a sample and cell off the chart and re-create the view by hand. The mapping from "I clicked this" to the right commands should live in one place so every view behaves the same.

## Goals

1. **One navigation model.** Views express an *intent*; a pure model turns it into commands. Views never build command strings.
2. **Intents covered:** event, group row, cell, cell list ("show cells"), back to live.
3. **Safe by gating.** Navigation is enabled only when Source A is Live, or a file whose `world` id matches the live game's loaded world.
4. **Visible failures.** A rejected or failed command gives a toast with the reason, a log line, and leaves state unchanged.
5. **Sync status.** The toolbar shows whether the game's view matches the last navigation, with a Re-sync action.
6. **Only navigation, plus recording config.** Nothing else is ever sent from T-1 except `cells.history.track` / `cells.history.untrack` (amended 2026-10-20, Feature 3): they change what DiaCellHistory records, never the simulation. See the allow-list below.

## Non-Goals

- Tuning, editing, or sim control commands (T-2, testbeds).
- Navigation views of the hex mini-map and the why panel's contribution list (Features 3 and 4 build those views; they only call this feature's model).
- Navigating Source B in compare mode (Source A only).
- Undo of a navigation.
- Following the game's own selection back into T-1 (the selection mirror only drives the sync status).

## Architecture

```
model/navigation.ts   (pure TS)
  intent → CommandPlan { steps: Command[] }
  type NavIntent =
    | { kind: 'event', event }
    | { kind: 'group', groupLayer, value }
    | { kind: 'cell', q, r, sample }
    | { kind: 'cells', cells: Cell[], sample }
    | { kind: 'backToLive' }
  canNavigate(sourceA, liveState) → { enabled, reason? }
  syncState(lastPlan, viewState) → 'in-sync' | 'out-of-sync' | 'unknown'
store/       lastNavigation, navigationStatus
views/       NavigationGate (disabled + tooltip wrapper), SyncPill, Toast
bridge       navigate(plan) → per-step results      (C++: CoWWorldSimInspectorPlugin)
```

Views call `navigation.ts` and the store. Only the store action `navigate(intent)` talks to the bridge.

### Intent to commands

| Intent | Commands sent (in order) |
|---|---|
| event | `cells.history.view <sample>`, then `cells.select.category <group layer> <value>` (or `cells.select.cell <q> <r>` for a cell event), then `cells.layer.base <event layer>` |
| group row | `cells.select.category <group layer> <value>` |
| cell | `cells.select.cell <q> <r>`, `cells.history.view <sample>` |
| cells | `cells.select.cells <q,r…>` (new DiaCellLayers command, see Dependencies), then `cells.history.view <sample>` |
| backToLive | `cells.history.live` |

Event-to-command rules come from the event's schema entry (`kind: event`): which payload fields name the group layer, value, cell and layer. An event missing the fields it needs produces a plan with fewer steps, never an invalid command.

### Gating

`canNavigate` returns enabled when the live game is connected and either Source A is Live or Source A is a file with `worldId` equal to the game's loaded world. Otherwise it returns a reason: *Not connected*, *Source is a file from a different world*, or *Game stale*. Disabled controls stay visible and greyed, with a tooltip showing the reason.

### Execution and failures

The bridge sends the steps in order and returns one result per step. On the first failure it stops, and the toast says which step failed and why (for example *layer not registered*). State in T-1 is unchanged. Each plan and result is logged.

### Sync status

`cells.view.state` reports the game's base layer, selection and history sample. After a navigation, the pill compares that state to the last plan's target:

- **In sync** when they match.
- **Out of sync** when the user changes the game view by hand, or a step failed.
- **Unknown** before any navigation.

What counts as a match (resolved 2026-10-20):
- Compare only what the user can see: base layer, history sample, and the selection as a **set** (order ignored).
- After `cells.history.live`, in sync means the game reports live mode, not a specific sample, so a live game running ahead is still in sync.
- After navigating to a specific sample, the sample must match exactly; the game's view is paused there, so any drift means a manual change.
- The selection is compared against the set the game **accepted** (from the command result), so a cell the game dropped does not show Out of sync. The comparison uses the selection fingerprint in `cells.view.state` (`count`, `version`, `hash`; DiaCellLayers Feature 5): T-1 computes the same order-independent 64-bit hash (XOR of the SplitMix64 mix of each cell index, `index = r * width + q`) from the accepted set and compares it with the topic's. If they differ it can fetch the real set with `cells.select.get` to show what changed (amended 2026-10-20).

The Re-sync button resends the last plan. "Back to live" clears the last target.

### Logging and observation

Channel `[EDITOR] T1`. Logged: each plan with its steps, step failures, gate transitions (enabled to disabled and back). No other commands exist on this path.

## Acceptance Criteria

1. Clicking an event sends, in order, the history view, the selection and the base-layer commands from the table, using the fields named by the event's schema entry.
2. Clicking a breakdown row sends only `cells.select.category` with the right group layer and value.
3. A cell intent sends `cells.select.cell` then `cells.history.view`; a cell-list intent sends `cells.select.cells` then `cells.history.view`.
4. Back to live sends only `cells.history.live`.
5. An event whose schema lacks the needed payload fields produces a shorter plan, never an invalid command.
6. Navigation controls are enabled for a Live Source A, and for a file whose `world` id matches the game's world.
7. For a file from a different world, a disconnected game or a stale live source, the controls are visible, disabled and carry a tooltip giving the reason.
8. If a step fails, later steps are not sent, a toast names the failing step and reason, a log line is written, and T-1 state is unchanged.
9. The sync pill shows In sync after a successful navigation, Out of sync after the game view is changed by hand, and Unknown before any navigation.
10. Re-sync resends the last plan; back to live clears the last target.
11. Nothing other than the commands in the table, plus `cells.history.track` and `cells.history.untrack`, can be sent through the bridge call (unlisted command names are rejected by the C++ handler). Track/untrack are gated like navigation, and T-1 untracks only layers it tracked itself.
12. Views contain no command strings; all commands come from `navigation.ts`.

## Test Strategy

- **Vitest (model/):** each intent's plan from fixture events (full, missing fields); `canNavigate` for every reason; `syncState` for match, mismatch, unknown.
- **React Testing Library:** `NavigationGate` enabled/disabled and tooltip; toast on failure; sync pill transitions; Re-sync button.
- **GoogleTests (C++):** the bridge handler sends steps in order, stops at the first failure, returns per-step results, and rejects commands outside the allow-list; gating when the world id does not match.
- **E2E:** stub game with fixture history; clicking an event sends the expected command sequence; loading a different world's file disables the controls; a stub that rejects a step shows the toast.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All code in `Cluiche/CoW/Editor/WorldSimInspector/` |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| Tool tiers (Inspector) | Inspector reads a live game; it does not tune it | Allow-list of navigation commands plus recording config (`track`/`untrack`), which affects observation, not simulation (AC 11; amended 2026-10-20) |
| UI/business separation | Typed model first; UI depends only on it | `navigation.ts` owns commands; views never build them |
| Editor actions via DiaAPI | Actions are DiaAPI commands | Every step is a `cells.*` command from DiaCellLayers / DiaCellHistory; `cells.select.cells` is a new one requested of DiaCellLayers (Dependencies) |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaCellLayers Feature 2 (Selection) | `cells.select.cell`, `.category`, `.cells` | Approved, not built |
| **DiaCellLayers Feature 2: new `cells.select.cells <q,r…>`** | Select an explicit list of cells (used by "show cells"). Replaces the earlier idea of building it from `cells.select.query`, whose expression language cannot name cells. The game enforces its own size limit and reports the accepted set in the result | **Requested; needs Dia spec change and AD-001 sign-off** |
| DiaCellHistory Feature 5 | `cells.history.view`, `.live` | Approved, not built |
| DiaCellLayers Feature 5 | `cells.view.state` topic (selection `count`/`version`/`hash`), `cells.select.get`, and the shared hash test vectors | Approved, not built |

## Resolved Decisions

1. **Explicit cell lists** (2026-10-20). "Show cells" uses a new `cells.select.cells` command. The system spec's "cells.select.query as a cell list" was wrong: the query language is an expression language. No region-fallback is built in v1; it would be an unmeasured optimisation. Revisit only if `cells.select.cells` proves heavy in practice.
2. **Selection size cap** (2026-10-20). Owned by DiaCellLayers; T-1 sends the top contributing cells and shows what the game accepted ("Selected N of M").
3. **Sync tolerance** (2026-10-20). See Sync status above.
4. **Step order** (2026-10-20). History view, then selection, then base layer. A one-frame flash is accepted for now; a batch-apply request goes on the DiaCellLayers Feature 2 review.

## Open Design Questions

1. **Batch apply.** Whether DiaCellLayers can apply a navigation plan in one frame so the game never renders a half-navigated state. Request only; not a blocker.
2. **Cap value.** The accepted-cell limit is DiaCellLayers' to set; T-1 needs only to show "Selected N of M". Check the number once Feature 2 is specced.

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs `cells.select.cells` in DiaCellLayers Feature 2 (spec amended 2026-10-20; Dia change needs AD-001 sign-off at build time).
