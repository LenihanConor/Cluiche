# Plan: World-Sim Kit, Draoi Half

**Spec:** @docs/specs/features/draoi/worldsim-kit-draoi.md
**Status:** Not Started (spec Draft; do not run `/implement` until the user approves the split)

**Origin:** split out of `worldsim-kit.plan.md` (2026-11-21). Source tasks keep their old numbers in the Notes column so history still resolves. None of the source rows had been built, so no row is Done.

**Dependencies.**
- **Core extraction gate.** DraoiHexWorld core must be extracted (grid and world types, `CellVocabulary.h`, `BiomeRegistry`, `ZoneTable`) **before** Tasks D4 to D10 start. The prerequisites (`hexworld-core-prerequisites.plan.md`) are Done; the extraction spec and plan are still to be written (backlog: "extract DraoiHexWorld core"). Tasks D2 and D3 are Dia-only and **can start now**.
- `guarantee-set-injection.plan.md` (In Progress): D4's `terrain.is_protected` source reads data-declared site kinds, so this should be Done or at least Task 1 (`GuaranteeSpec`, `SiteRegistry` data kinds) landed.
- `grid-search-helpers.plan.md` Done (`MultiSourceSteps`, `HexNeighbours`, `TreeGraph`) before D9.
- `biome-registry.plan.md` Done before the palette task D6 and the `biome.*` fixtures in D4.
- `parameter-files.plan.md` is **not** needed here: the codec takes `params_hash` and ranges as arguments.
- AD-001: the DiaCondition changes (D2, D3) were signed off 2026-11-18. Nothing else here touches Dia. Opus is never assigned (feedback_confirm_opus).

## Implementation Patterns

Package `DraoiHexWorld`, namespace `Draoi::HexWorld`; real path is fixed by the core extraction spec (assumed `Draoi/DraoiHexWorld/`). Files:
```
World/WorldCellFields.h/.cpp   World/CellSelector.h/.cpp
Save/HexWorldSaveCodec.h/.cpp
Testing/                        fixture hex worlds (6 x 4, 3 x 3 flow tree), fake field contributions
Dia/DiaCondition/ConditionExpr.h/.cpp, IConditionContext.h     (D2, D3 only)
Cluiche/Tests/GoogleTests/DiaCondition/TestConditionValidateContext.cpp
Cluiche/Tests/GoogleTests/Draoi/HexWorld/   TestWorldCellFields, TestCellSelector, TestSelectorClauses, TestHexWorldSaveCodec
```
Files are added with `dia docs vcxproj-add` and the package with `dia scaffold module`; never by hand. No STL in public APIs. No CoW include, no `Modules`, no bus, no DiaSaveGame, no DiaDebugServer, no `StepChain`/`IStepStage` in Draoi code (AC 24, checked by `dia check deps`).

Behaviour is exactly the spec's Design; the detailed API shapes carried over from the old plan:
- `WorldCellFields : IConditionContext`: `struct FieldReader { float (*getFloat)(const void*, uint32_t); bool (*getBool)(const void*, uint32_t); const void* ctx; }`; `Register(slot, field, reader)`, `Has`, `SetCell`, `Clear`; a pointer to a "reads forbidden" flag (Debug assert), set by the caller.
- `CellSelectorSet`: `ConditionExpr::LoadFromJson` selectors; derived fields registered under `slot: "derived"`; `distance` via `MultiSourceSteps` over `HexNeighbours`; `upstream` via `MultiSourceSteps` over a `TreeGraph` from `terrain.flow_direction`; resolve to a sorted list (pointer and count into one pooled array); caps 32 and 16.
- Codec: `SavedKind { F32, U8, U16 }`, `SavedScope { PerCell, PerBasin }`, `SavedField { name, kind, scope, min, max }` (range **supplied by the caller**), `SaveError { String32 field; String256 reason; }`, `WriteHeader`, `WriteArray`, `Validate(const Json::Value&, const SavedField*, uint32_t, const Caps&, SaveError&)`; arrays use `append(double)` with 17 digits, `-0.0` written as `+0.0`; the codec takes `const BiomeRegistry&`.
- Tests: table-driven faults (one row per fault, inline JSON); answers computed by hand in comments and asserted as literals; determinism by `memcmp`; non-finite values built in code.

Verification cadence: narrow `dia run googletest --filter="<suite>" --shards 2` per task; the scoped filter below runs only in the verification task; no full suite per feature, no sanitizer, no mutation checks. Perf is a Debug ballpark: 32 selectors on a 104 x 64 world in well under a second.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| D0 | Prerequisite check, read only: has DraoiHexWorld core been extracted (path, module doc, namespace)? status of `guarantee-set-injection.plan.md`, `grid-search-helpers.plan.md`, `biome-registry.plan.md`; the real field/ranges lookup for `terrain.is_protected` (kinds from `guarantees.json`); update this plan's paths | Findings in notes | Pending | haiku | Mechanical, TDD-exempt. Gate for D4 onward. Old Task 0 (g), (m) partly |
| D1 | Scaffold: `dia scaffold module` for the package areas, empty file pairs, test folder, vcxproj entries | Solution builds with the empty files | Pending | haiku | Mechanical, TDD-exempt. Needs D0 (core extracted). Old Task 1 |
| D2 | RED then GREEN: `IConditionContext::Has(slot, field)` defaulted virtual in `Dia/DiaCondition/IConditionContext.h` | AC 16 (`ConditionValidateContext*`): a context not overriding `Has` behaves as before; existing implementers compile unchanged | Pending | sonnet | **Can start now (Dia only).** Old Task 5a. AD-001 sign-off 2026-11-18. Commit |
| D3 | RED then GREEN: `ConditionExpr::Validate(const IConditionContext&)` overload | AC 11, 16: a fake context knowing two pairs; unknown slot, unknown field and a nested `and`/`or`/`not` each fail naming the leaf; existing `Validate(ConditionRegistry&)` tests pass unchanged | Pending | sonnet | **Can start now.** Old Task 5. After D2. Ask before escalating; extend `DiaCondition/Testing/`, do not duplicate |
| D4 | RED then GREEN: `WorldCellFields`: register, `Has`, `SetCell`, `GetFloat`/`GetBool`, `Clear`, reads-forbidden flag assert | AC 10 (core): two fixture fields evaluate an `and` of a float compare and a bool to the hand-computed set on a 6 x 4 world; unknown pair returns 0/false; `Has` agrees with registration; flag set and read asserts (Debug death test, or skipped with a note if no assert hook) | Pending | sonnet | Old Task 14. Needs D1, D3. Handles are CRC pairs resolved at use |
| D5 | RED then GREEN: codec header, arrays, Validate with caller-supplied ranges, plus angular fields `[0, 2π]` inclusive | AC 5, 20: header keys; 17-digit round trip; `-0.0` written `+0.0`; table of faults naming the field (missing key, `cell_count != w*h`, 0, 65,537, cap exceeded, length mismatch, non-finite, out-of-range, integer kind overflow, negative `step_day`); changing a caller range flips the verdict; `2π` accepted, above rejected; `ready:false` accepted with only `ready` | Pending | sonnet | Old Tasks 15 and 15a (the sidecar-reading half of 15a is the CoW plan's). Needs D1 |
| D6 | RED then GREEN: `biome_palette` write once per record, per-cell indices, remap through `ResolveOrFallback`, hash recorded, one Info on a hash mismatch with a clean remap | AC 21: registry A written, registry B (row appended, row reordered) loaded gives the same ids; unknown id falls back with one warning per id; hash stored; exactly one Info | Pending | sonnet | Old Task 16, Draoi half of 16a. Needs `biome-registry.plan.md` Done and D5; until then a stub registry with the same three methods (mark in notes) |
| D7 | RED then GREEN: `CellSelectorSet` load and validate: parse `derived`/`selectors`, validate through `Validate(IConditionContext&)`, caps and cycle detection | AC 12 (failures): table rows naming the item: unknown slot, unknown field, unknown selector in a derived field, a cycle, `maxSteps` 0, a 33rd selector, a 17th derived field; a valid block loads | Pending | sonnet | Old Task 25. Needs D3, D4 |
| D8 | RED then GREEN: `CellSelectorSet` resolve of plain selectors: sorted deterministic lists on the 6 x 4 world; `all` predefined | AC 12 (resolve): list equals the hand-computed set; two resolves byte-identical (`memcmp`); a selector matching nothing gives an empty list; Debug perf ballpark (32 selectors, 104 x 64) | Pending | sonnet | Old Task 26 |
| D9 | RED then GREEN: derived fields `distance` and `upstream` | AC 12 (derived): `distance {from: river}` matches hand-computed steps, unreachable +infinity; `upstream {of, maxSteps: 6}` is 1 exactly within 6 steps along flow toward a match on a 3 x 3 flow tree; `<=` on a derived field; a derived field depending on another resolves in order | Pending | sonnet | Old Task 27. Needs `grid-search-helpers.plan.md` Done, else BLOCKED: do not hand-roll a search |
| D10 | RED then GREEN: clause coverage: one fixture expression per today's A4 and A5 clause (`elevation`/`latitude {min,max}`, `waterSource`, `river`, `terrain.is_protected`, `basin lake\|dry\|any` via `hydro.basin_kind`, `distanceToWater`, `distanceToRiver`, `upstreamOfProtected`) | AC 13: each resolves to the set the clause's spec text defines, by hand; `any` equals `lake` union `dry` | Pending | sonnet | Old Task 28. `hydro.basin_kind` is a fixture contribution (A5 does not exist). If a clause's A4/A5 text is ambiguous, record it under open items; do not guess |
| D11 | Boundary check: `dia check deps` on the package; grep for CoW, bus, save, debug-server and step-chain includes | AC 24: pass, no forbidden include | Pending | haiku | Mechanical |
| D12 | Exhaustive test pass: gap audit of AC 5, 10-13, 16, 20, 21, 24 against the written tests, then edge tests: a 65,536-cell record accepted, 65,537 rejected; a palette with duplicate ids; a selector referencing `all`; a derived field with an empty `from` set; a registry `Clear` between two resolves; `Has` for a never-registered slot | Gap list in notes; edge tests green | Pending | sonnet | No mutation checks (project rule). Old Task 36 (Draoi half) |
| D13 | Observation Opportunity Scan over touched files, five pillars | Suggestions in notes | Pending | haiku | Non-blocking. Likely: `DIA_PROFILE_SCOPE` around `CellSelectorSet::Resolve` and codec Validate. Draoi stays free of bus/health wiring (PD-012): suggestions are limited to profiling and logging |
| D14 | Scoped verification: `dia run googletest --filter="*CellFields*:*CellSelector*:*SelectorClauses*:*HexWorldSaveCodec*:*Condition*" --shards 2`; `dia check deps`; `dia docs precommit` | AC 5, 10-13, 16, 20, 21, 24 quoted by name; pass/fail and failing names only | Pending | haiku | No full suite, no sanitizer |
| D15 | Close-out: `dia docs plan <this plan> <n> --status Done` per row, then `dia docs spec-done docs/specs/features/draoi/worldsim-kit-draoi.md`; update the Draoi backlog row and the CoW half's status line | Spec Done, plan Done | Pending | haiku | **Do not run until all rows are Done.** Commit with the attribution lines |

**AC to task coverage.** AC 5: D5. AC 10: D4. AC 11: D3. AC 12: D7, D8, D9. AC 13: D10. AC 16: D2, D3. AC 20: D5. AC 21: D6. AC 24: D11. **Order:** D2, D3 now; D0, D1 after core extraction; then D4, D5, D6, D7 to D10, D11 to D15.

## Open items

1. Package path and module-doc names wait on the core extraction spec (D0).
2. `terrain.is_protected` source: after the generic special sites feature, kinds come from data. Confirm D4's fixture uses the data-declared kinds rather than the six C++ kind names.
3. The CoW half's S-1 contribution task is where `terrain.*` readers live until the terrain package is extracted (spec ODQ 2).
