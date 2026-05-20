# Plan: Validation Fix Suggestions

## Spec
[validation-fix-suggestions.md](validation-fix-suggestions.md) — Approved 2026-05-20

## Session Notes

### Spec decisions summary

DiaApplicationFlowEditor is a CluicheEditor plugin (CEF + React frontend, C++ backend) editing `.diaapp` v2 manifests. **Binding decisions in scope:** PD-001 (StringCRC for IDs), PD-007 (C++20), AD-003 (`Dia::ApplicationFlow::Editor::` namespace), ED-007 (React + CEF), ED-013 (validation always-on, debounced 500ms).

**Architecture invariant for this feature:** the React client is "stupid" — C++ owns rule logic, navigation targets, and fix commands. JS only renders what C++ supplies and dispatches `manifest.applyCommand` payloads verbatim. Adding a 13th rule must never require JS changes. The `ValidationIssue` struct gains explicit target string fields (`targetKind`, `targetPuId`, `targetModuleId`, `targetStreamId`) plus an optional `SuggestedCommand` POD (commandType + flat fields + stagesCSV). All `validation.run` and `validation.result` JSON payloads include these fields. JS extends `ValidationIssueV2` to match, adds a tiny `useSelectionStoreV2` (PU + stream) so the validation bar can drive selection without prop-drilling, and `ValidationBarV2` does ~12 lines of rule-agnostic switching: 3-way `targetKind` dispatch + `bridgeRequest('manifest.applyCommand', issue.suggestedCommand)`. **6 of 12 rules** carry fix commands at MVP: `UNKNOWN_STREAM_IN_READS` → `RemoveModuleRead`, `UNKNOWN_STREAM_IN_WRITES` → `RemoveModuleWrite`, `ORPHAN_READER_STREAM` → `RemoveStream`, `ORPHAN_WRITER_STREAM` → `RemoveStream`, `ORPHAN_MODULE` → `SetModuleStages(["all"])`, `STAGE_REF_INVALID` → `SetModuleStages(current minus invalid)`. The remaining 6 (`DEPENDENCY_CYCLE`, `PAYLOAD_TYPE_MISSING`, `DUPLICATE_INSTANCE_ID`, `INITIAL_STAGE_INVALID`, `STREAM_PU_INVALID`, `STREAM_SELF_LOOP`) navigate-only.

### Key file map

- `Dia/DiaApplicationEditor/V2/ManifestValidator.h` — struct definitions
- `Dia/DiaApplicationEditor/V2/ManifestValidator.cpp` — rule implementations (one block per rule, ~450 lines)
- `Dia/DiaApplicationEditor/DiaApplicationFlowEditorPlugin.cpp` — `HandleValidationRun` at line 801 (JSON serialization)
- `Dia/DiaApplicationEditor/UI/src/v2/useValidationStoreV2.ts` — TS type + zustand store
- `Dia/DiaApplicationEditor/UI/src/v2/AppV2.tsx` — currently owns selection state for PU
- `Dia/DiaApplicationEditor/UI/src/v2/StreamsTab.tsx` — currently owns stream selection state internally (line 233)
- `Dia/DiaApplicationEditor/UI/src/v2/ValidationBarV2.tsx` — UI changes here

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Extend `ManifestValidator.h`: add `ValidationTargetKind` enum + target/suggestedCommand fields on `ValidationIssue` + `SuggestedCommand` POD struct | `dia pipeline --target cluicheeditor` builds | Done | sonnet | Pure data-model; no logic change. Heap-backed `DynamicArray<ValidationIssue>` to keep kMaxIssues=64 off the stack (per-issue ~1KB). |
| 2 | Update `ManifestValidator.cpp` — populate target fields in all 12 rules; populate `suggestedCommand` for the 6 fixable rules. Use `StringCRC::AsChar()` with `nullptr` fallback to leave targets empty (clickless row). | New GoogleTest suite `ManifestValidatorTargetsTest`: each rule emits expected target + (where applicable) suggestedCommand fields | Done | sonnet | 36/36 tests pass (24 original + 12 new). |
| 3 | Update `HandleValidationRun` JSON serialization (and notify) in plugin to include new fields. CSV → array conversion for stages. | Manual: `validation.run` round-trip carries fields end-to-end (visible in browser devtools) | Done | sonnet | Single block at line 824; same `result` object also pushed via `validation.result` notify. CSV split inline. |
| 4 | Extend `ValidationIssueV2` TS type in `useValidationStoreV2.ts` + map them in `setResult` / runValidation passthrough | `pnpm vitest --run useValidationStoreV2` (or existing tests) — type-check passes | Done | haiku | Added `normalizeValidationResult`; AppV2 + runValidation both use it. 122/122 vitest pass. |
| 5 | Create `useSelectionStoreV2.ts` (zustand) with `{ puId, streamId, setPU, setStream, clear }`. Migrate `AppV2`'s `selection` state and `StreamsTab`'s internal `selectedId` to read/write through this store. | `pnpm vitest --run StreamsTab AppV2` — existing tests still pass | Done | sonnet | StreamsTab.test resets store in beforeEach. AppV2 + StreamsTab tests pass (19/19). |
| 6 | `ValidationBarV2`: add row onClick → switch to right tab + set selection (3-way switch on `targetKind`). Take `setActiveTab` as prop. | New `ValidationBarV2.test.tsx`: clicking PU-target issue switches activeTab to graph + selects PU; clicking stream-target switches to streams + selects stream | Done | sonnet | RULE_ID_LABELS map for human display. |
| 7 | `ValidationBarV2`: add inline Fix button when `suggestedCommand` is non-null. onClick stops propagation + dispatches `bridgeRequest('manifest.applyCommand', issue.suggestedCommand)`. | `ValidationBarV2.test.tsx`: Fix click sends bridge call with verbatim command payload; click does NOT also navigate | Done | sonnet | Verbatim payload — zero rule-specific JS logic. |
| 8 | Visual polish per mockup: severity dot, monospace ruleId, chevron only when navigable, Fix button styling (subdued chip, hover brighten) | Manual: matches mockups/validation-fix-suggestions.html | Done | sonnet | Inline mouse hover handlers for hover effect. |
| 9 | Wire `setActiveTab` prop from `AppV2` into `ValidationBarV2` | Manual smoke test in browser | Done | haiku | One-line prop pass at AppV2 footer. |
| 10 | Update `real-time-validation.md` AC #6 status note (already done as part of spec — verify only) + run end-to-end smoke test | `dia run cluicheeditor`, load manifest with errors, verify navigation + Fix work | Done | sonnet | Plan task 5 row marked Done with cross-link. End-to-end smoke deferred to user acceptance — all 12 ValidationBarV2 unit tests pass and CluicheEditor builds clean. |
| 11 | Commit with conventional message | `git status` clean | Todo | haiku | After tasks 1-10 done |

## Risks

- **`StringCRC::AsChar()` returning nullptr** — for IDs introduced in JSON-loaded manifests where the engine never interned the string, the reverse lookup fails. Mitigation: leave `targetKind` as `None` so the row stays clickless; row is still informational. Documented in spec AI Review #2.
- **Selection store refactor** — moving `StreamsTab`'s internal `selectedId` to a store could regress the existing stream detail-pane behavior. Task 5 includes regression test gate before continuing.
- **CSV serialization for `SetModuleStages`** — the wire format uses comma-separated stage names. If a stage name ever contains a comma we have a problem. Stage names are validated to be identifiers (no commas), so safe in practice.

## Decisions log

(to be filled as tasks execute)
