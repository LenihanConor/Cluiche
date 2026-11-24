# Loose Ends triage, 2026-11-19

Triage of the 68 rows left in `docs/BACKLOG.md` "Loose Ends" after the first cleanup pass (13 rows removed as fixed, superseded or duplicate). Rule applied: CLAUDE.md "Gaps and Loose Ends" (fix now, promote to a row, or drop). Priority is High, Medium or Low.

Outcome: 68 rows in, 3 kept. The rest were fixed or closed, merged into 13 new Task rows in the backlog (11 Dia, 2 CoW), or dropped.

## High

| Item | Outcome |
|---|---|
| `FindPath` `PathResult` holds 256 cells, no overflow guard in Release | **Fixed.** `PathResult::kMaxCells`; `FindPath` returns "no path" for a longer route instead of writing past the array. Two boundary tests (257+ cells fails, exactly 256 succeeds). |
| Existing-code robustness: `UnicodeDecodeError` in `precommit_cmd._check_manifests` | **Fixed.** Now reported as a manifest error. |
| Existing-code robustness: `ListCommandInfos`/`ListJsonCommandInfos` out of bounds at the 257th command | **Closed, not a bug.** The registry itself caps at 256, the same size as the output array, so the overflow is unreachable. |
| Existing-code robustness: `GetCommand` pointer outlives its lock, palette keeps a dangling `ValueParam*`, `MetricRegistry::Reset` leaves dangling info pointers | **Promoted:** "Command and metric registry lifetime hardening". |
| DiaCore `String<N>` printf-style constructors and `operator=` | **Promoted:** "DiaCore `String<N>` audit". |
| Economy inspector sources call `NotifySubscribers` from SimPU | **Promoted:** "Economy inspector SimPU publish race". |

## Medium

| Item | Outcome |
|---|---|
| GoogleTests Release run has ~51 failures | **Promoted:** "GoogleTests Release failures". |
| `SLOW_RegionRepair...` fails, plus "zone left under its range minimum" | **Promoted (merged):** "S-1 region zone shortfall decision". The test fails because 9 archipelago zones end below the range minimum (seed 514 first); the AC4 amendment only reports total shortfall. It is a design decision (does per-zone shortfall set `limitedBy`?), not a code fix. |
| Sim-hosted error replies are `success:true` on the wire | **Promoted:** "Sim-hosted error replies `success:false`". |
| Inspector UIs missing from deploy | **Promoted:** "Inspector UI deploy". |
| E2E: `geometry2d` fails on first entry, five scenarios fail alone, CellLayers re-entry flake, `dia test e2e` does not build, no CoW E2E scenario | **Promoted (merged):** "E2E scenario health pass". |
| DiaDeferredStep F4: `ManifestSaver` may drop `requires_tick_after`; stream write path perf pass (AC1 5 ns never met) | **Promoted:** "DiaDeferredStep leftovers". The other F4 items are accepted and dropped. |
| Game-scoped plugins: `Save()` must be implemented by T-1 and T-2 | **Moved** into the T-1 and T-2 backlog rows; the row is dropped. |
| Profile subject-name table is 256 entries and process-wide | **Promoted:** "Profile subject-name table cap". |
| S-1 panel: show last Regenerate outcome and `busy`; "layer unavailable" after import | **Promoted (merged):** "S-1 panel busy and unavailable layers". The panel is built and `lastOutcome` exists in its progress payload; `busy` and unavailable-layer handling were not confirmed, so the task starts with a check. |
| AutomationService checkpoint callbacks read SimPU state from Main | **Promoted:** "AutomationService checkpoint runner" (design already in the old row; preserved in the task). |
| `TypeRegistry` fixed module-type table | **Dropped.** The assert now prints type and capacity; the table is 128 and CluicheTest uses 64. |
| `dia docs plan` rejects `Pending`; `dia docs spec-done` cannot parse a backticked status; `vcxproj-add` in a bash loop | **Closed.** Plan accepts `Pending` as an alias and `spec-done` handles the backticked `## Status` form (both already in the code). The bash-loop row was a shell quoting quirk, not a CLI bug. |
| `SimAnnounceResyncIntegration` suite runs over 500 ms | **Fixed.** The two 192-name tests are now `SLOW_SimAnnounceResyncIntegration`; the other 25 stay in default runs. |
| DiaValueSchema Feature 7: describe the existing components | **Promoted:** "Describe existing components". The remaining Feature 6 and 7 follow-ups and the pinned behaviours are dropped. |

## Low

Dropped (the detail stays here for anyone who wants it back):

- **S-1 world-gen follow-ups:** archipelago AC3/AC4, every world `nodeLimited`, no retry path on real worlds, purity tuning, terrain tuning leftovers, module and commands follow-ups. All accepted untuned at their sign-offs; the E2E gap moved to the E2E pass.
- **S-1 observation instrumentation (5 rows):** generation timing, hand-off, validator and retry, export/import, `Place`/`CheckLayout`. Low findings belong in plan notes, not the backlog (CLAUDE.md). Nothing blocks; pick them up in an observation pass if wanted.
- **CellField:** hillshade faceting, small open decisions, `KernelCellFieldRegistry` stopgap. Noted in the CellField spec.
- **Engine and tooling notes:** DiaScalarField cannot serve grid simulations, axial `HexFlowAdapter` formulas, `SaveContext` array/`uint64_t` writers, `dia perf convert` field names, Job Observation tests without a RED run, DiaCellLayers selection and symlink pin follow-ups, Sim-Hosted Queries follow-ups, DiaCellHistory follow-ups, DiaDeferredStep F5 follow-ups, Perf Inspector follow-ups, DiaValueSchema pinned behaviours, Features 6 and 7 follow-ups, `#16/#17` manual checks. (Revisit: Perf Inspector counter seams `capUse`/`peakQueueDepth` read 0 until a sampler exists.)
- **Ideas with no deliverable:** RenderTechnique asset type, Camera2D controller, split GoogleTests per app (do at the third app), A1 breadth extensions (already in A1's Future Extensions), A4 influence API review for dragons.
- **Already recorded elsewhere:** S-4 prerequisites, mockups owed, consumer requirements, E1 SaveLoad changes, `new-cow-stage` trial, F-1 follow-ups.
- **Deferred rows (4):** event replay, `reflect` stage, tablet and touch, status badges.

Kept in Loose Ends:

- **Frame-capture changes outside CellField scope**: needs the user to sign off under AD-001 or revert.
- **GDD leftovers from the world-sim fold-back**: belongs to the GDD session, which owns the DesignDocs edits.
- **Perf Inspector's `cow.diaobservation` `trace.enabled` edit** is an uncommitted change waiting on the user's decision (the rest of that row was dropped).

## Fixed in this pass

| Change | Files |
|---|---|
| `FindPath` guard | `Dia/DiaPathfinding/FindPath.h`, `PathResult.h`, `Tests/GoogleTests/DiaPathfinding/TestPathfindingBoundary.cpp` |
| `precommit` manifest decode error | `Dia/DiaCLI/dia_cli/commands/docs/precommit_cmd.py` |
| `SimAnnounceResync` SLOW split | `Tests/GoogleTests/CluicheGameBaseline/TestSimAnnounceResyncIntegration.cpp` |
| Backlog cleanup | `docs/BACKLOG.md`, `docs/specs/applications/cow/dia-needs.md` (need 18) |
