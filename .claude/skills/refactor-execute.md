---
name: refactor-execute
description: Run the execute stage of a refactor session — apply a migration phase to code and update affected spec bodies (human-gated)
tags: [refactor, execute, migrate, specs]
user_invocable: true
agent_invocable: false
---

# Refactor: Execute Stage

Apply one migration phase from the refactor plan: edit source files, then update the architecture/API sections of affected specs to reflect the new shape.

This step is **human-gated**. Claude must present a summary of intended changes and wait for explicit approval before editing any file.

## Usage

```
/refactor-execute <subsystem_slug> --phase <N>
/refactor-execute <subsystem_slug>
```

If `--phase` is omitted, Claude will ask which phase to run.

## Instructions for Claude

### 1. Resolve the Session

Check that both of these exist:
- `docs/refactors/<slug>/outputs/audit.json`
- `docs/refactors/<slug>/outputs/plan.json`

If either is missing, print the missing prerequisite and stop:
```
Missing: docs/refactors/<slug>/outputs/plan.json
Run /refactor-plan <slug> first.
```

If `docs/refactors/<slug>/outputs/execute.json` already exists, read it to determine which phases have already been applied. Warn if the requested phase was already applied.

### 2. Read Context

Read:
- `docs/refactors/<slug>/outputs/plan.json`
- `docs/refactors/<slug>/reports/plan.md`
- @.claude/steering/tech.md
- @.claude/steering/structure.md

Identify the requested phase from `data.migration_phases`. List the files named in that phase entry.

### 3. Present the Approval Summary

Before touching any file, present this summary and wait for "yes":

```
== HUMAN APPROVAL REQUIRED ==

Subsystem:  <slug>
Phase:      <N> — <phase name>
Goal:       <phase description>

Code files to edit:
  - <file path> — <what will change>
  - ...

Spec file(s) to update:
  - <spec path> — sections: <Architecture / Responsibilities / Public API>

Manual review hotspots (will NOT be auto-edited):
  - <file:lines> — <reason>

Proceed with phase <N>? (yes / no / show full plan)
```

Wait for explicit "yes". Do not proceed on any other response.

If user types "show full plan": print `reports/plan.md` content, then re-present the approval summary.

### 4. Apply Code Changes

For each file in the phase:

1. Read the current file content
2. Apply only the edits described in the plan for this phase
3. Do not refactor beyond the phase scope — no opportunistic cleanup
4. When you reach a manual-review hotspot, stop and print:
   ```
   MANUAL REVIEW REQUIRED
   File:   <file:lines>
   Reason: <reason from plan>
   Describe the intended change above and ask: "Apply this manually or skip? (apply / skip)"
   ```
   Wait for the user's response before continuing.

After all code edits are complete, print a summary:
```
Code changes applied:
  <file> — <brief description of what changed>
  ...
```

### 5. Update Affected Specs

After code is applied, locate relevant spec(s):
- Search `docs/specs/systems/dia/` and `docs/specs/features/dia/` for specs that describe this subsystem
- If multiple candidates are found, present them and ask which to update
- If none are found, ask: "No spec found for this subsystem. Provide a path or skip spec update? (path / skip)"

For each identified spec:
1. Read the spec file
2. Identify sections that now describe the *old* architecture (Architecture, Responsibilities, Public API, Interface sections)
3. Rewrite only those sections to reflect the post-phase shape
4. Do NOT modify:
   - Status field
   - Traceability table
   - Binding Decisions compliance table
   - AI Review Questions
   - Any section not describing architecture/API
5. Print a summary of what changed:
   ```
   Spec updated: docs/specs/systems/dia/<name>.md
     Sections rewritten: Architecture, Public API
     Sections preserved: Status, Traceability, Binding Decisions
   ```

### 6. Write execute.json

Write or update `docs/refactors/<slug>/outputs/execute.json`:

```json
{
  "command": "execute",
  "status": "ok",
  "data": {
    "phase": 1,
    "phase_name": "...",
    "files_edited": [],
    "manual_review_hotspots": [
      {"file": "...", "lines": "...", "reason": "...", "resolution": "applied|skipped"}
    ],
    "specs_updated": [
      {"spec_file": "...", "sections_changed": []}
    ],
    "follow_on_phases": []
  }
}
```

Write `docs/refactors/<slug>/reports/execute.md`:

```markdown
# Refactor Execute — <Subsystem> Phase <N>

**Phase:** <N> — <name>
**Date:** YYYY-MM-DD

## Code Changes
| File | Change |
|------|--------|

## Manual Review Hotspots
| File | Reason | Resolution |
|------|--------|------------|

## Specs Updated
| Spec | Sections Changed |
|------|-----------------|

## Follow-on Phases
[Remaining phases from plan]
```

### 7. Report

Print:
```
Phase <N> applied.
  Files edited:    <N>
  Specs updated:   <N>
  Hotspots:        <N> (applied: X, skipped: Y)

Next step: /refactor-review <slug>
  (or run: /refactor-execute <slug> --phase <N+1> to apply the next phase first)
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplication, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20

When applying code changes: flag any edit that would violate a PD constraint as a manual-review hotspot. When updating specs: the spec is the contract — only update sections that are now factually incorrect given the code change. Never adjust the spec's intent or goals.
