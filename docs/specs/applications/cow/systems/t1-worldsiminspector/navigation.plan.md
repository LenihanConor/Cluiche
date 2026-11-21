# Plan: T-1 Navigation (Feature 6)

**Spec:** @docs/specs/applications/cow/systems/t1-worldsiminspector/navigation.md
**Status:** Not Started
**Mockup (approved 2026-11-16):** `mockups/world-sim-inspector-v2.html`. The visual gate for the UI tasks: top-bar sync pill (In sync / Out of sync / Unknown), failure toast, disabled-with-reason navigation controls.

## Prerequisites (gate, checked in Task 0)

As of 2026-11-16 `Cluiche/CoW/Editor/` holds only the `CoWEditor` stub (`CoWEditor.h/.cpp/.vcxproj`); there is no `WorldSimInspector/` folder, no UI project and no `CoWWorldSimInspectorPlugin`. Feature 1 (`plugin-shell-and-sources.md`) is `Approved`, plan not created.

| Needs | From | Used by tasks |
|---|---|---|
| UI scaffold (Vite, React 18, zustand, `@dia/editor-ui`, Vitest, RTL), `model/` + `store/` + `views/` layout, `Source` model (`kind`, `worldId`), connection pill state (connected / disconnected / stale), `cow.session.get` world id | T-1 Feature 1 | 1-5, 8-12 |
| `CoWWorldSimInspectorPlugin : Dia::Editor::LiveConnectionPluginBase` (`Dia/DiaEditor/Plugin/LiveConnectionPluginBase.h`: `OnLivePluginLoad/Unload`, `OnGameConnected/Disconnected`, `OnLiveUpdate`) and the React-to-C++ bridge request path | T-1 Feature 1 | 6, 7 |
| Event schema entries (`kind: event`) giving group layer / value / cell / layer payload fields | T-1 Feature 2 + DiaValueSchema | 2 |
| `cells.select.cell/.category/.cells/.get`, `cells.layer.base`, `cells.view.state` | DiaCellLayers (`Dia/DiaCellLayers/CellLayerViewerDomain.cpp`, `MirrorQueries.cpp`, `ViewStatePublisher.cpp`) | 0, 5, 14 |
| `cells.history.view/live/track/untrack` | DiaCellHistory (`Dia/DiaCellHistory/HistoryViewCommands.cpp`, `CellHistoryCommands.cpp`) | 0, 6, 14 |
| Selection hash vectors `selection_hash.json` | `Dia/DiaCellLayers/Testing/Conformance/` (README lists C++ `TestSelectionFingerprint.cpp` as the C++ consumer) | 3 |

**Do not start Task 1 until Feature 1's scaffold exists.** Tasks 1-4 are pure TS and can start as soon as it does.

## Sign-offs (AD-001)

| Change | Where | Status |
|---|---|---|
| `cells.select.cells <q,r...>` command in DiaCellLayers (Dia change) | `Dia/DiaCellLayers/CellLayerViewerDomain.cpp` (`SelectOp::Cells`); spec `selection.md` lines 64-67 | Ask. Spec amended 2026-10-20 and code appears built, but no recorded AD-001 sign-off was found. Task 0 confirms with the user; if the user says it was given, record the date here |
| `CluicheEditor.vcxproj` reference to `CoWEditor`, uPlot | shared build file | Signed off 2026-11-05 for the parent system |
| Allow-list, bridge, UI | `Cluiche/CoW/Editor/WorldSimInspector/` | None (CoW tier, AD-002) |

## Implementation Patterns

### Pure TS model (Tasks 1-4), no React, no bridge
- Files in `model/`: `navigation.ts`, `selectionHash.ts`, `commands.ts`. Views import these and the store only; no command string appears in `views/` (AC 12, enforced by a Task 11 grep-style test).
- `commands.ts` is the **single command table** (`NAV_COMMANDS`, to be extended by Features 7 and 9 with `SNAPSHOT_COMMANDS`, `S25_QUERIES`). Each entry: `{ name, kind: 'command'|'query', gate: 'navigate'|'mutate'|'none', addedBy }`. `navigation.ts` builds plans only from these names.
- `navigation.ts`: `NavIntent` union, `planFor(intent, schema) -> CommandPlan { steps }`, `canNavigate(sourceA, liveState) -> { enabled, reason? }` (reasons: Not connected, Source is a file from a different world, Game stale), `syncState(lastPlan, viewState, acceptedSet) -> 'in-sync'|'out-of-sync'|'unknown'`. Event-to-command rules read the schema entry; missing fields give a shorter plan, never an invalid command.
- `syncState` compares base layer, history sample (exact when the plan targeted a sample; "live mode" when it targeted `cells.history.live`) and selection as a set via the hash of the **accepted** set (from the `cells.select.*` result), not the requested set.
- `selectionHash.ts`: order-independent 64-bit XOR of SplitMix64 mix of each `index = r * width + q`, using `BigInt`, output 16 lowercase hex. Vectors come from `Dia/DiaCellLayers/Testing/Conformance/selection_hash.json` (same fixture the C++ test uses), loaded by the Vitest, not copied.

### Allow-list as one data table (Tasks 5-6)
- **One list, one place.** The canonical list is a JSON fixture `Cluiche/CoW/Editor/WorldSimInspector/Shared/bridge_allowlist.json`: `[{ "name", "kind", "gate", "addedBy" }]`. It holds this feature's rows now (the nine navigation names plus `cells.history.track/untrack`); Feature 7 adds its six and Feature 9 adds `cow.s25.report` by appending rows, with no code change.
- **C++ side:** `WorldSimInspectorAllowList` (in `CoWEditor`) is a static table of `{name, kind, gate}`, plus `IsAllowed(name)` and `GateFor(name)`. The handler only consults this table.
- **Sync test:** one fixture, three readers. A Vitest loads the JSON and asserts it equals `commands.ts`'s table (names, kind, gate). A GoogleTest loads the same JSON (path passed via a compile-time define / the existing GoogleTests fixture dir mechanism) and asserts it equals the C++ table. Adding a name in only one place fails one of the two tests. The README note in the fixture folder explains: when a feature adds a command, edit the JSON, then both tables.
- Explicitly absent: `cow.s25.run`, `cow.s25.baseline`, `cow.s25.livesmoke` (a negative test asserts they are rejected).

### C++ bridge handler (Tasks 5-6)
- `navigate(plan)` in `CoWWorldSimInspectorPlugin`: validate every step name against the allow-list **before** sending anything (reject the whole plan if any is unlisted); check the gate (connected, world match for mutating names); send steps in order through the DiaDebugServer client path used by sibling plugins (see `Dia/DiaEconomyInspector/DiaEconomyInspectorPlugin.cpp`, `Dia/DiaAIInspector/DiaAIInspectorPlugin.cpp`); stop at first failure; return `[{step, ok, result|error}]`; log plan and failures on `[EDITOR] T1`.
- Testability: the handler takes an `ICommandSender` seam (fake in GoogleTests: scripted per-step results, records the send order). No real game needed.

### Store and views (Tasks 7-11)
- Store: `lastNavigation { plan, acceptedSelection }`, `navigationStatus`, action `navigate(intent)` (the only caller of the bridge), `resync()`, `backToLive()` (clears last target). A failed step leaves `lastNavigation` and all other state unchanged and raises a toast.
- Views: `NavigationGate` (greyed, visible, tooltip reason from `canNavigate`), `SyncPill`, `Toast`, Re-sync button, "Selected N of M" text. Visual check against the mockup.

### Verification cadence
- Per task: scoped Vitest (`npx vitest run <file>` from `Cluiche/CoW/Editor/WorldSimInspector/UI`) or `dia run googletest --filter="*WorldSimInspector*" --shards 2`.
- ONE full GoogleTests run and the E2E run at the end (Tasks 12-13). No mutation checks (decided 2026-11-12).
- `dia` CLI only; never call exes directly.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Gate: confirm T-1 Feature 1 is Done (scaffold, `Source`, plugin, bridge). Check and list each as Built / Specced-not-built / Missing, with file refs: `cells.select.cells`, `cells.select.cell/.category/.get`, `cells.layer.base`, `cells.view.state` fingerprint (`count/version/hash`), `cells.history.view/live/track/untrack`. Ask the user about the AD-001 sign-off for `cells.select.cells` and record it. Record baseline Vitest and `*WorldSimInspector*` GoogleTest counts | none (no code) | Not Started | haiku | Grep Dia/DiaCellLayers and Dia/DiaCellHistory. Findings so far: select.cells, select.get, view.state hash, history.view/live/track/untrack all in code; `cells.history.live` result shape and the view.state `history` field (via `IViewHistorySource`) to verify. Stop and report if Feature 1 is not Done |
| 1 | RED -> GREEN: `commands.ts` table + `planFor` for event, group, cell, cells, backToLive | AC 1-5: plan order per spec table; event with missing group/cell/layer fields gives shorter plan; cell event uses `select.cell`; no invalid command ever emitted | Not Started | sonnet | Fixture events from Feature 2's schema fixtures. Views must not hold strings (AC 12) |
| 2 | RED -> GREEN: `canNavigate` | AC 6-7: Live A enabled; file with matching `worldId` enabled; each reason (Not connected, different world, Game stale) with precedence | Not Started | haiku | Pure function, small |
| 3 | RED -> GREEN: `selectionHash.ts` (SplitMix64 XOR, `BigInt`) | Shared vectors from `selection_hash.json` (all named vectors; names list asserted); order independence; empty set; duplicate indices policy matches C++ | Not Started | sonnet | Uses `index = r * width + q`; hex must equal the topic's `hash` string format (16 hex) |
| 4 | RED -> GREEN: `syncState` | AC 9-10: match; each mismatch (layer, sample, selection hash); live mode after `history.live` ignores sample drift; exact sample after view; unknown before first navigation; failed step gives out-of-sync; dropped cell does not show out-of-sync (accepted set) | Not Started | sonnet | Uses Tasks 1 and 3 |
| 5 | RED -> GREEN (C++): allow-list data table + sync-test fixture `bridge_allowlist.json`; `IsAllowed`, `GateFor` | AC 11: every nav name and track/untrack accepted; near-misses (`cells.select.cellz`, `cells.snapshot.export`) rejected; `cow.s25.run/baseline/livesmoke` rejected; Vitest and GoogleTest both compare their table to the JSON | Not Started | sonnet | GoogleTests `*WorldSimInspector*`. Feature 7 and 9 extend by appending rows (see Patterns). New files need `dia docs vcxproj-add CoWEditor` |
| 6 | RED -> GREEN (C++): `navigate(plan)` handler with `ICommandSender` seam: ordered send, stop at first failure, per-step results, unlisted name rejects whole plan before any send, gating (disconnected, world mismatch) | AC 8, 11: order recorded by fake sender; failure at step 2 means step 3 not sent and result list has 2 entries; world mismatch sends nothing; track/untrack gated like navigation; log line written | Not Started | sonnet | GoogleTests. Spec assumes a bridge request path exists in `LiveConnectionPluginBase`; the header has only lifecycle virtuals, so Feature 1 must supply it (note in Task 0) |
| 7 | Store slice + `navigate` action: `lastNavigation`, `navigationStatus`, accepted-selection capture from `cells.select.cells` result, `resync`, `backToLive`, failure toast and unchanged state, T1 log lines for plan, failure and gate transitions | Vitest with fake bridge: success updates last plan; failed step leaves state unchanged and toasts "step N failed: reason"; resync resends last plan; backToLive clears target; gate transition logged once per change | Not Started | sonnet | AC 8, 10 |
| 8 | Consume `cells.view.state` topic in the store; compute sync status on each tick; on mismatch optionally fetch `cells.select.get` (chunked, 1,000 indices per call) to show what changed | Vitest: manual game change flips In sync to Out of sync; hash mismatch triggers one `select.get`; no fetch when matching | Not Started | sonnet | `kMaxSelectionChunk = 1000` in `MirrorQueries.h`; `cells.select.cells` cap is 2,048 per call per `selection.md` |
| 9 | UI: `NavigationGate`, `Toast`, `SyncPill`, Re-sync button, "Selected N of M" text | RTL AC 7, 9, 10: enabled/disabled with tooltip reason; toast on failed step; pill transitions In sync / Out of sync / Unknown; Re-sync click calls store action | Not Started | sonnet | Visual check vs `world-sim-inspector-v2.html`; screenshot per state group |
| 10 | Wire intents into existing views: event timeline click, breakdown row, mini-map cell, why-panel "Show cells", Back to live; each wrapped in `NavigationGate` | RTL AC 1-4: each click calls `navigate` with the right intent; a test scans `views/` for `cells.` strings and fails if found (AC 12) | Not Started | sonnet | Views from Features 2-4 may not exist yet; wire what exists and leave a note listing the rest for those features |
| 11 | E2E: stub game fixture with scripted history; click event sends expected command sequence; different-world file disables controls; stub rejects a step so toast shows | AC E2E list from spec | Not Started | sonnet | `dia test e2e` does not build: run `dia pipeline --target cluichetest --stage compile-code` first (BACKLOG Loose End 2026-10-29). Kill any leftover `CluicheTest.exe` |
| 12 | Gap audit and edge tests: re-read each AC against the tests; add edges (empty cell list, over-2,048 cells "Selected N of M", out-of-bounds cell dropped, disconnect mid-plan, rapid double click, world change while out-of-sync, unicode group values, stale source); then ONE full `dia run googletest --shards 2` and full Vitest | Added tests fail first where they find a bug; full suites pass | Not Started | sonnet | Explicit task per the exhaustive-test rule; no mutation checks. Report pass/fail and failing names only |
| 13 | Close-out: Observation Opportunity Scan (suggestions only: logs, traces, profile scopes, metrics, health); amend spec Dependencies table to Built where true; `dia docs spec-done navigation.md --archive-plan`; update parent features table; move backlog row with `dia docs backlog move`; tell Features 7 and 9 plans to extend `bridge_allowlist.json` | none | Not Started | haiku | Findings do not block Done |

## Notes on spec assumptions

- Spec Dependencies list `cells.select.cells`, Selection and `cells.view.state` as "Approved, not built"; code shows they are built (`CellLayerViewerDomain.cpp` `SelectOp::Cells`, `ViewStatePublisher.cpp`, `MirrorQueries.cpp`). Task 13 updates the table.
- Spec assumes `LiveConnectionPluginBase` offers the bridge request path; it does not in the header read. Feature 1 must build it.
- No `WorldSimInspector/` code, UI folder or `CoWWorldSimInspectorPlugin` exists yet.
