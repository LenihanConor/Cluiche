# Feature Spec: Validation Fix Suggestions

## Parent System
@docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md

## Builds On
@docs/specs/applications/dia/systems/diaapplicationfloweditor/real-time-validation.md

## Mockup
@docs/specs/applications/dia/systems/diaapplicationfloweditor/mockups/validation-fix-suggestions.html

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia/dia.md | AD-003 |
| System | @docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md | ED-007, ED-013 |
| Feature (upstream) | @docs/specs/applications/dia/systems/diaapplicationfloweditor/real-time-validation.md | AC #5 (location), AC #6 (click-to-navigate) |

## Purpose

`real-time-validation.md` defined what gets checked and where errors/warnings appear. This spec defines **what users can do about them** — closing the loop from "found a problem" to "fixed in one action" without leaving the validation list.

Two capabilities:
1. **Click-to-navigate** — clicking any issue selects the offending PU/module/stream and switches to the appropriate tab/inspector. (Resolves `real-time-validation.md` AC #6, which was unimplemented.)
2. **One-click fix** — for issues with an obvious mechanical resolution (e.g. `UNKNOWN_STREAM_IN_READS`: the read points at a stream that doesn't exist; the obvious fix is to remove the read), the issue row exposes a "Fix" button that dispatches the corresponding command.

The C++ validator is the single source of truth for both targets and fix actions. The React client is "stupid": it renders what C++ tells it and dispatches what C++ supplied. Adding, changing, or removing rules never touches JS code.

## Acceptance Criteria

1. **Targets carried by validator** — Each `ValidationIssue` carries an explicit target descriptor (`targetKind` + `targetPuId` + `targetModuleId` + `targetStreamId` strings). When a rule has no meaningful target (e.g. `INITIAL_STAGE_INVALID`), `targetKind` is empty and the row stays clickless.
2. **Click-to-navigate** — Clicking an issue row whose `targetKind` is non-empty:
   - `pu` → switches to Process Units tab, selects the PU in the sidebar.
   - `module` → switches to Process Units tab, selects the parent PU (Module Inspector wiring is out of scope here; see Open Questions).
   - `stream` → switches to Streams tab, selects the row.
3. **Suggested fix command** — Each `ValidationIssue` may carry an optional `suggestedActionLabel` (≤63 chars) and a `suggestedCommand` JSON object. When both are present, the issue row renders an inline `Fix` button labelled with `suggestedActionLabel` (e.g. `Remove read`, `Set 'all' stage`, `Remove stream`).
4. **Fix dispatches verbatim** — Clicking `Fix` calls `bridgeRequest('manifest.applyCommand', issue.suggestedCommand)` exactly as supplied by C++. The React client does no rule-specific logic, no payload massaging, no validation of the command shape.
5. **Coverage** — At MVP, the rules below carry suggested fixes:
   | Rule | Fix label | Command |
   |------|-----------|---------|
   | `UNKNOWN_STREAM_IN_READS` | `Remove read` | `RemoveModuleRead` |
   | `UNKNOWN_STREAM_IN_WRITES` | `Remove write` | `RemoveModuleWrite` |
   | `ORPHAN_READER_STREAM` | `Remove stream` | `RemoveStream` |
   | `ORPHAN_WRITER_STREAM` | `Remove stream` | `RemoveStream` |
   | `ORPHAN_MODULE` | `Assign to 'all' stages` | `SetModuleStages` (stages = `["all"]`) |
   | `STAGE_REF_INVALID` | `Remove invalid stage ref` | `SetModuleStages` (stages = current minus invalid) |
   The remaining rules (`DEPENDENCY_CYCLE`, `PAYLOAD_TYPE_MISSING`, `DUPLICATE_INSTANCE_ID`, `INITIAL_STAGE_INVALID`, `STREAM_PU_INVALID`, `STREAM_SELF_LOOP`) navigate only — they have no single mechanical fix.
6. **Visual differentiation** — Issue rows with `targetKind != ""` show a hover state and a chevron (`›`). Rows with a `suggestedCommand` show the Fix button on the right; the button uses the existing chip-style affordance from `PUInspector` (subdued text, hover brightens). No fix button when `suggestedCommand` is null.
7. **Re-validate after fix** — After a Fix is dispatched, the existing `manifest.applyCommand` → `validation.run` chain re-runs validation; the issue list refreshes from the new result. No client-side optimistic removal.
8. **JSON wire format** — `validation.run` response and `validation.result` push payloads include the new fields per issue:
   ```json
   { "ruleId": 2, "severity": "error", "message": "...",
     "targetKind": "module", "targetPuId": "MainPU", "targetModuleId": "RenderModule", "targetStreamId": "",
     "suggestedActionLabel": "Remove read",
     "suggestedCommand": { "commandType": "RemoveModuleRead", "puId": "MainPU", "instanceId": "RenderModule", "streamId": "BadStream" } }
   ```
   Missing target fields default to empty strings; missing `suggestedCommand` is `null`.

## Design

### C++ data model changes

`Dia/DiaApplicationEditor/V2/ManifestValidator.h`:

```cpp
enum class ValidationTargetKind : unsigned char { None, PU, Module, Stream };

struct ValidationIssue
{
    ValidationRuleId   ruleId;
    ValidationSeverity severity;
    char message[256];

    // NEW — navigation target
    ValidationTargetKind targetKind = ValidationTargetKind::None;
    char targetPuId[64]     = {};
    char targetModuleId[64] = {};
    char targetStreamId[64] = {};

    // NEW — optional one-click fix
    char suggestedActionLabel[64] = {};
    // Stored as a small fixed payload — flat key/value pairs are sufficient
    // for the 6 commands above. See `SuggestedCommand` below.
    SuggestedCommand suggestedCommand = {};
};

// Discriminated by `commandType`. Empty string means "no fix".
struct SuggestedCommand
{
    char commandType[32]  = {};   // "" = none
    char puId[64]         = {};
    char instanceId[64]   = {};
    char streamId[64]     = {};
    // For SetModuleStages: comma-separated stage list. Decoded by serializer.
    char stagesCSV[256]   = {};
};
```

The string buffers keep `ValidationIssue` POD-friendly and dodge dynamic allocation — consistent with the rest of the editor state. CSV is the simplest stable wire format for `SetModuleStages`; the JSON serializer splits it on the way out.

### Validator changes

Each rule that already knows the offending IDs (it does — they're in the message string today) populates the target fields and, where applicable, the suggested command. Example for `UNKNOWN_STREAM_IN_READS`:

```cpp
ValidationIssue issue = MakeIssue(ValidationRuleId::UnknownStreamInReads,
                                  ValidationSeverity::Error, msg);
issue.targetKind = ValidationTargetKind::Module;
strncpy_s(issue.targetPuId,     pu.instanceId.AsChar(), _TRUNCATE);
strncpy_s(issue.targetModuleId, mod.instanceId.AsChar(), _TRUNCATE);
strncpy_s(issue.targetStreamId, mod.reads[r].AsChar(),   _TRUNCATE);

strncpy_s(issue.suggestedActionLabel, "Remove read", _TRUNCATE);
strncpy_s(issue.suggestedCommand.commandType, "RemoveModuleRead", _TRUNCATE);
strncpy_s(issue.suggestedCommand.puId,        pu.instanceId.AsChar(),   _TRUNCATE);
strncpy_s(issue.suggestedCommand.instanceId,  mod.instanceId.AsChar(),  _TRUNCATE);
strncpy_s(issue.suggestedCommand.streamId,    mod.reads[r].AsChar(),    _TRUNCATE);
result.issues.Add(issue);
```

This requires `StringCRC` keys to round-trip back to their original string form. `StringCRC::AsChar()` already exists for the engine's interned table; for IDs typed by the user via the editor, the string is already in the manifest. **Risk:** if a manifest is loaded from JSON where `StringCRC::AsChar()` returns `nullptr` (interning miss), we fall back to a hex `0x%08X` representation — the navigate target won't match the React side and the row stays clickless. Documented in AI Review #2.

### JSON serialization

`HandleValidationRun` in `DiaApplicationFlowEditorPlugin.cpp` adds:

```cpp
item["targetKind"]     = TargetKindToString(issue.targetKind);     // "", "pu", "module", "stream"
item["targetPuId"]     = issue.targetPuId;
item["targetModuleId"] = issue.targetModuleId;
item["targetStreamId"] = issue.targetStreamId;
item["suggestedActionLabel"] = issue.suggestedActionLabel;
if (issue.suggestedCommand.commandType[0] != '\0')
{
    Json::Value cmd;
    cmd["commandType"] = issue.suggestedCommand.commandType;
    if (issue.suggestedCommand.puId[0])       cmd["puId"]       = issue.suggestedCommand.puId;
    if (issue.suggestedCommand.instanceId[0]) cmd["instanceId"] = issue.suggestedCommand.instanceId;
    if (issue.suggestedCommand.streamId[0])   cmd["streamId"]   = issue.suggestedCommand.streamId;
    if (issue.suggestedCommand.stagesCSV[0])
    {
        Json::Value stages(Json::arrayValue);
        // split CSV → array
        cmd["stages"] = stages;
    }
    item["suggestedCommand"] = cmd;
}
else
{
    item["suggestedCommand"] = Json::nullValue;
}
```

### React changes

**`useValidationStoreV2.ts`** — extend `ValidationIssueV2`:

```ts
export interface ValidationIssueV2 {
    ruleId: number;
    severity: 'error' | 'warning';
    message: string;
    targetKind: '' | 'pu' | 'module' | 'stream';
    targetPuId: string;
    targetModuleId: string;
    targetStreamId: string;
    suggestedActionLabel: string;
    suggestedCommand: Record<string, unknown> | null;
}
```

**`AppV2.tsx`** — promote selection state so ValidationBar can drive it:

```ts
type Selection =
    | { type: 'pu';     puId: string }
    | { type: 'stream'; streamId: string }
    | null;

// Pass setSelection + setActiveTab into ValidationBarV2 via props.
```

`StreamsTab` already keeps its own `selectedId`; for navigation we accept that the stream is selected via a controlled `selectedId` prop or via a shared store. To stay minimal, lift `selectedId` into a tiny `useSelectionStoreV2` (PU + stream) — this also removes the awkward "selection lives in two places" duplication AppV2 has today.

**`ValidationBarV2.tsx`** — single switch in the row's onClick:

```tsx
const onIssueClick = (issue: ValidationIssueV2) => {
    if (issue.targetKind === 'pu')     { setActiveTab('graph');   selectPU(issue.targetPuId); }
    if (issue.targetKind === 'module') { setActiveTab('graph');   selectPU(issue.targetPuId); }
    if (issue.targetKind === 'stream') { setActiveTab('streams'); selectStream(issue.targetStreamId); }
};

const onFixClick = (issue: ValidationIssueV2, e: React.MouseEvent) => {
    e.stopPropagation();
    if (issue.suggestedCommand) bridgeRequest('manifest.applyCommand', issue.suggestedCommand);
};
```

That is the entirety of the rule-specific logic in JS — twelve lines.

### ValidationBar visual layout (per issue row)

```
┌────────────────────────────────────────────────────────────────────┐
│ [✕] UNKNOWN_STREAM_IN_READS  RenderModule reads BadStream …  ›  [Remove read] │
└────────────────────────────────────────────────────────────────────┘
   severity   ruleId monospace    message (truncated)        chevron   fix btn
```

- Severity dot: red `●` for error, amber `●` for warning.
- RuleId rendered in monospace, gray.
- Message truncates with ellipsis; full text on hover via `title`.
- Chevron (`›`) only when `targetKind != ""`.
- Fix button only when `suggestedCommand != null`. Click stops propagation so it doesn't also fire navigate.
- Row hover background `#2d2d2d` when navigable; static when not.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Extend `ValidationIssue` + `SuggestedCommand` structs in `ManifestValidator.h` | Builds | Todo | sonnet | Pure data-model change |
| 2 | Populate target fields in all 12 rules in `ManifestValidator.cpp` | Unit test (GoogleTest): each rule emits expected target | Todo | sonnet | Reuses existing rule code paths |
| 3 | Populate `suggestedCommand` for the 6 fixable rules (AC #5) | Unit test: `UnknownStreamInReads` emits `RemoveModuleRead` payload | Todo | sonnet | |
| 4 | JSON serialization in `HandleValidationRun` (and `validation.result` notify) | Round-trip test: JS receives all fields | Todo | sonnet | Single function edit |
| 5 | Extend `ValidationIssueV2` TS type + store passthrough | Type-check passes | Todo | haiku | Mechanical |
| 6 | New `useSelectionStoreV2` (puId + streamId), wire AppV2 to it | `AppV2.test.tsx`: existing selection tests still pass | Todo | sonnet | Modest refactor |
| 7 | `ValidationBarV2` row onClick → navigate (3 cases) | `ValidationBarV2.test.tsx`: stream-target click sets activeTab=streams + selection | Todo | sonnet | |
| 8 | `ValidationBarV2` Fix button → `bridgeRequest('manifest.applyCommand', cmd)` | Test: click fires bridge with verbatim suggestedCommand | Todo | sonnet | |
| 9 | Visual states (chevron, hover, severity dot, fix-btn styling) | Manual: matches mockup | Todo | sonnet | |
| 10 | Update `real-time-validation.md` AC #6 status note pointing to this feature | Doc only | Todo | haiku | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Targets carry both the StringCRC (semantically) and the user-visible string form for UI matching. |
| PD-007 | C++20 required | New structs use designated initializers and trailing default member initializers — C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | All new types in `Dia::ApplicationFlow::Editor::`. |
| ED-007 | React + CEF frontend | Logic stays in C++; React only renders + dispatches. Reinforces the "stupid client" boundary. |
| ED-013 | Validation always-on, debounced 500ms | Unchanged. Fix dispatch goes through normal `manifest.applyCommand` which already triggers re-validation. |
| SD-014 | Full validation at load | Unchanged. New fields piggyback on existing rule output. |

## Open Questions

1. **Module Inspector navigation** — AC #2 currently routes `targetKind=module` to the PU Inspector for the parent PU, because there's no Module Inspector route in `AppV2` today (the Module Inspector exists but isn't wired into the main shell — it's reached only via PUInspector card click in some contexts). Wiring full Module navigation is a separate concern. For MVP: navigate to the parent PU; mention the offending module in the validation message text. Revisit when Module Inspector gets a proper route.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Wire format | `suggestedCommand` is sent as opaque JSON. Doesn't this mean a typo in C++ produces a silent bridge failure? | Yes — but that's already true for every command the editor dispatches. The bridge `manifest.applyCommand` handler validates `commandType` and rejects unknowns with `ok:false`. We rely on existing tests (`PUInspector.test.tsx`, `ModuleInspector.test.tsx`) plus a new round-trip test for each fixable rule to catch typos at CI time. |
| 2 | StringCRC reverse lookup | What happens if `StringCRC::AsChar()` returns `nullptr` for a target ID (e.g. an ID introduced in a manifest the engine never interned)? | `targetKind` stays `None`, the row stays clickless, no Fix button. The validation message is unaffected (it formats the hex value). User can still see the issue and fix it manually — no crash, just a graceful loss of the fix-suggestion affordance. Acceptable for MVP. |
| 3 | Concurrent fix clicks | If a user clicks two Fix buttons rapidly, can the second one fire against stale targets? | The first fix triggers re-validation; the issue list rerenders before the user can realistically double-click. If they do, the second `applyCommand` either succeeds (idempotent, e.g. removing an already-removed stream) or returns `ok:false` (the C++ command validates preconditions). No client-side guard needed. |
| 4 | Localization | Hardcoded English labels (`Remove read`, `Set 'all' stage`) live in C++. What if we ever localize? | The labels are already separated into `suggestedActionLabel` rather than inferred from `commandType`, which is the right shape for future localization (swap the C++ string for a key + lookup). Not in MVP. |
| 5 | Fixability gap | `DEPENDENCY_CYCLE` has no obvious one-click fix — but the user usually wants to remove one of the cycle's dependencies. Should we suggest the last-added one? | No. "Last-added" isn't tracked, and removing an arbitrary dep is more destructive than navigating + letting the user pick. Stays navigate-only. |
| 6 | Save coupling | If a Fix button leaves a manifest that still has errors, does save remain blocked per real-time-validation.md AC § Save Integration? | Yes — the save-blocking logic doesn't change. Fix buttons reduce the issue list one entry at a time; save remains blocked until errors hit zero. |

## Status

`Done` — 2026-05-20

Plan: [validation-fix-suggestions.plan.md](validation-fix-suggestions.plan.md)
