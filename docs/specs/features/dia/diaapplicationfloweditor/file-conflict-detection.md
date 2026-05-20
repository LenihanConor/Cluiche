# Feature Spec: File Conflict Detection

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-007 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-007 |

## Purpose

Detect when the currently-open `.diaapp` file is modified externally (by another tool, git checkout, or manual edit) and prompt the user to reload or keep their in-memory version. Prevents silent data loss from saving over externally-changed content.

## Acceptance Criteria

1. **File watch** — Monitor the open `.diaapp` file for external modifications using OS file system notifications.
2. **Conflict detection** — When external modification detected, compare file timestamp (or content hash) against last-known state.
3. **Prompt dialog** — Show a dialog: "File changed externally. Reload (lose in-editor changes) / Keep mine (overwrite on next save) / Diff (show changes)?"
4. **Reload** — Reloads file from disk, replaces in-memory model, clears undo history.
5. **Keep mine** — Dismisses dialog, keeps in-memory model. Next save overwrites external changes.
6. **Auto-detection on focus** — Also check on editor panel focus (fallback for missed FS notifications).
7. **No false positives** — Editor's own save must not trigger the conflict dialog (suppress notification during save).

## Design

### Implementation

C++ backend uses `ReadDirectoryChangesW` (Windows API) to watch the directory containing the open file. When a change notification includes the open file:

1. Compare last-modified timestamp against stored value from load/save
2. If different: notify frontend via event `onFileConflict`
3. Frontend shows conflict dialog

### Conflict Dialog

```
FileConflictDialog (modal)
├── Message: "<filename> was modified outside the editor."
├── ReloadButton ("Reload from disk") — reloads, clears undo
├── KeepButton ("Keep my version") — dismisses, flags stale
└── DiffButton ("Show diff") — shows textual diff of changes (nice-to-have)
```

### Save Suppression

During save:
1. Set `mSuppressWatch = true`
2. Write file
3. Update stored timestamp to new value
4. Set `mSuppressWatch = false`

Any notifications arriving while suppressed are discarded.

### Focus Check (Fallback)

On `OnActivate()` (panel gains focus):
- Stat the file, compare timestamp
- If changed and no pending conflict dialog → trigger conflict flow

This catches cases where FS notifications are missed (network drives, some antivirus interference).

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | File watcher using `ReadDirectoryChangesW` | Unit test: detects external write | Todo | Windows-specific |
| 2 | Timestamp comparison and conflict detection logic | Unit test: no false positive on own save | Todo | |
| 3 | Save suppression flag | Unit test: save doesn't trigger conflict | Todo | |
| 4 | Focus-check fallback | Integration test: detects change on re-focus | Todo | |
| 5 | CEF conflict event + React dialog | Manual: dialog appears, buttons work | Todo | |
| 6 | Reload path (reloads model, clears undo) | Integration test: model matches disk after reload | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-007 | C++20 required | Uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | In `Dia::ApplicationFlow::Editor::`. |
| ED-007 | React + CEF frontend | Dialog in React; watcher in C++ backend. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should the watcher also monitor imported .diastage files? | Not for this feature — Manifest Load/Save loads .diaapp only. If future work adds .diastage awareness, extend then. |
| 2 | Diff | Is the "Show diff" button MVP or nice-to-have? | Nice-to-have. MVP is Reload/Keep. Diff can be added later without changing the architecture. |
| 3 | Network | Should this work on network drives (UNC paths)? | Best-effort. ReadDirectoryChangesW works on network drives but may be unreliable. Focus-check fallback covers gaps. |
| 4 | Timing | What if the user is mid-edit when conflict is detected? | Dialog is non-blocking to ongoing interaction but modal for file operations. User finishes typing, then decides. No auto-reload. |

## Status

`Approved` — 2026-05-19
