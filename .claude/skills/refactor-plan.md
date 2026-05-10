---
name: refactor-plan
description: Run the plan stage of a refactor session — define the target architecture and staged migration strategy
tags: [refactor, plan, architecture]
user_invocable: true
agent_invocable: true
---

# Refactor: Plan Stage

Turn the audit diagnosis into a target architecture and a phased migration strategy.

## Usage

```
/refactor-plan <subsystem_slug>
/refactor-plan <subsystem_slug> --goal "<explicit migration objective>"
```

## Instructions for Claude

### 1. Resolve the Session

If no slug is provided:
- List all subfolders in `docs/refactors/`
- Ask: "Which refactor session do you want to plan for?"

Check that `docs/refactors/<slug>/outputs/audit.json` exists. If it does not:
- Print: "audit.json not found for docs/refactors/<slug>/. Run /refactor-audit <slug> first."
- Stop.

If `docs/refactors/<slug>/outputs/plan.json` already exists:
- Warn: "plan.json already exists for this session. Regenerate? (yes/no)"
- Wait for confirmation before overwriting.

### 2. Read Context

Read:
- `docs/refactors/<slug>/outputs/audit.json`
- `docs/refactors/<slug>/reports/audit.md` (if present)
- @.claude/steering/structure.md

If `--goal` is provided, use it as the primary migration objective. Otherwise, derive the goal from the audit's `recommended_cleanup_order[0]`.

### 3. Design the Plan

Produce:

- **Target shape** — what the subsystem should look like after the refactor. Be concrete: name the classes, interfaces, or ownership model that should exist.
- **Boundary changes** — what moves in or out of the subsystem
- **Responsibilities before vs after** — use a table
- **Affected systems** — other modules, specs, or call sites that will need updating
- **API/interface changes** — public headers, method signatures, data structures
- **Migration phases** — ordered stages, each small enough to be applied and reviewed independently
- **Minimal viable slice** — the first phase that delivers a meaningful improvement on its own
- **Risks and rollback points** — what could break and how to recover
- **Acceptance criteria** — how we know the refactor succeeded (these become the claims for `prove`)

Keep phases small. A phase that touches more than ~5 files is probably too large to review safely.

### 4. Write Artifacts

Create `docs/refactors/<slug>/outputs/plan.json`:

```json
{
  "command": "plan",
  "status": "ok",
  "data": {
    "refactor_goal": "...",
    "current_problem": "...",
    "target_shape": {
      "description": "...",
      "boundaries": []
    },
    "affected_systems": [],
    "api_changes": [],
    "migration_phases": [
      {"id": 1, "name": "...", "description": "...", "files": []}
    ],
    "minimal_slice": [],
    "risks": [],
    "rollback_points": [],
    "acceptance_criteria": []
  }
}
```

Create `docs/refactors/<slug>/reports/plan.md`:

```markdown
# Refactor Plan — <Subsystem>

**Input:** docs/refactors/<slug>/outputs/audit.json
**Goal:** <refactor goal>

## Current Problem
[Brief restatement of the core structural issue]

## Target Shape
[What the subsystem should look like after the refactor]

### Boundary Changes
[What moves in or out]

### Responsibilities Before vs After
| Responsibility | Before | After |
|----------------|--------|-------|

## Affected Systems
[Bulleted list of other modules/specs touched]

## API / Interface Changes
[Bulleted list of public header or signature changes]

## Migration Phases
| Phase | Name | Description | Files |
|-------|------|-------------|-------|

## Minimal Viable Slice
[The first phase that delivers a meaningful improvement]

## Risks and Rollback Points
[Bulleted list]

## Acceptance Criteria
[Bulleted list — these become the claims for /refactor-prove]
```

### 5. Report

Print:
```
Plan complete for <subsystem>.
  Migration phases: <N>
  Minimal slice:    Phase 1 — <name>
  Acceptance criteria: <N> claims

Next step: /refactor-execute <slug> --phase 1
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplicationFlow, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20

When planning: ensure each migration phase is compatible with the PD constraints. Flag any phase that would require temporarily violating a constraint and describe the rollback point.
