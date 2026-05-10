Take the user through a structured refactor loop: audit → plan → execute → review → prove. Produces artifacts in docs/refactors/<subsystem>/ that document the diagnosis, target architecture, applied changes, findings, and invariant validation.

---

## Step 1 — Orient

Check `docs/refactors/` for existing session folders.

If the user's message mentions a subsystem name that matches (or closely resembles) an existing folder:
- Check which of `outputs/audit.json`, `outputs/plan.json`, `outputs/execute.json`, `outputs/review.json`, `outputs/prove.json` already exist
- Report which stages are complete, e.g. "I found docs/refactors/renderer/ — audit and plan are done."
- Ask: "Continue from execute, or re-run an earlier stage?"
- Jump to the appropriate step below once confirmed.

If this is a new session:
1. Ask: "What subsystem do you want to refactor?"
2. Ask: "What is the core problem or pain point? (or describe the structural smell)"
3. Ask: "Any constraints to keep in mind? (e.g. preserve behavior, no new allocations, stay in this module)"
4. Derive the subsystem slug:
   - Lowercase the subsystem name
   - Replace spaces with underscores
   - Example: "DiaApplicationFlow" → `dia_application`, "render backend" → `render_backend`
5. Present the proposed slug: "Proposed folder: docs/refactors/<slug>/. Confirm, or type a custom slug."
6. Wait for confirmation before creating any files. Then continue to Step 2.

---

## Step 2 — Audit

Goal: diagnose what is structurally wrong with the subsystem.

Before writing anything, read silently:
- @.claude/steering/tech.md
- @.claude/steering/structure.md
- @docs/specs/platform/Cluiche.md

Survey the subsystem and write `docs/refactors/<slug>/outputs/audit.json` and `docs/refactors/<slug>/reports/audit.md`.

**audit.md structure:**

```
# Refactor Audit — <Subsystem>

**Session date:** YYYY-MM-DD
**Folder:** docs/refactors/<slug>/

## Subsystem Summary
[What this subsystem does and its current scope]

## Strengths
[What is working well — don't remove these]

## Debt
[Structural problems: ownership issues, unclear lifetimes, mixed concerns]

## Duplication
[Repeated logic that should be centralized]

## Hidden Coupling
[Dependencies that are implicit, undocumented, or surprising]

## Missing Tests
[Invariants that are unprotected]

## Likely Invariants to Preserve
[Behavioral contracts that must survive the refactor]

## Recommended Cleanup Order
[Ordered list — what to tackle first for maximum impact with minimum risk]
```

**audit.json shape:**
```json
{
  "command": "audit",
  "status": "ok",
  "data": {
    "subsystem": "<slug>",
    "summary": "...",
    "strengths": [],
    "debt": [],
    "duplication": [],
    "hidden_coupling": [],
    "missing_tests": [],
    "likely_invariants": [],
    "recommended_cleanup_order": []
  }
}
```

After writing, print: "Audit complete. Proceed to planning? (yes / no / re-run audit)"
Wait for confirmation before continuing.

---

## Step 3 — Plan

Goal: define the target architecture and a staged migration strategy.

Read `docs/refactors/<slug>/outputs/audit.json`.

Write `docs/refactors/<slug>/outputs/plan.json` and `docs/refactors/<slug>/reports/plan.md`.

**plan.md structure:**

```
# Refactor Plan — <Subsystem>

**Input:** docs/refactors/<slug>/outputs/audit.json

## Refactor Goal
[One sentence: what we are trying to achieve]

## Current Problem
[Brief restatement of the core structural issue from audit]

## Target Shape
[Description of what the subsystem should look like after the refactor]

### Boundary Changes
[What moves in or out of the subsystem]

### Responsibilities Before vs After
| Responsibility | Before | After |
|----------------|--------|-------|

## Affected Systems
[Other subsystems or specs that will change]

## API / Interface Changes
[Public headers, method signatures, or data structures that will change]

## Migration Phases
| Phase | Name | Description |
|-------|------|-------------|
| 1 | ... | ... |

## Minimal Viable Slice
[The smallest phase that delivers a meaningful improvement]

## Risks and Rollback Points
[What could go wrong and how to back out]

## Acceptance Criteria
[How we know the refactor succeeded]
```

After writing, print: "Plan complete — <N> migration phases. Proceed to execute? (yes / no / re-run plan)"
Wait for confirmation.

---

## Step 4 — Execute (human-gated)

Goal: apply the refactor in staged phases, then update affected specs.

Read `docs/refactors/<slug>/outputs/plan.json`.

Present the following before touching any files:

```
== HUMAN APPROVAL REQUIRED ==

Phase:    <N> — <phase name>
Files:    <list of files to be edited>
Spec(s):  <spec files whose architecture sections will be updated>

Proceed with phase <N>? (yes / no / show plan)
```

Wait for explicit "yes" before making any edits.

**On approval:**
1. Apply the code edits for this phase, file by file
2. Flag and pause at any manual-review hotspot — do not auto-edit lifetime-sensitive or hot-path code
3. After code edits, locate the relevant spec(s) in `docs/specs/systems/` and `docs/specs/features/`
4. Rewrite only architecture/responsibilities/API sections that now describe the old shape
5. Do NOT touch: spec status, traceability table, binding decisions, AI review questions
6. Print a summary of what changed in each spec

Write `docs/refactors/<slug>/outputs/execute.json`:
```json
{
  "command": "execute",
  "status": "ok",
  "data": {
    "phase": 1,
    "phase_name": "...",
    "files_edited": [],
    "manual_review_hotspots": [{"file": "...", "reason": "..."}],
    "specs_updated": [{"spec_file": "...", "sections_changed": []}],
    "follow_on_phases": []
  }
}
```

Print: "Phase <N> applied. Run review now? (yes / no / run next phase)"
Wait for confirmation.

---

## Step 5 — Review

Goal: adversarially inspect the applied change for problems.

Read `docs/refactors/<slug>/outputs/plan.json` and staged diff (or ask user to point at changed files).

Write `docs/refactors/<slug>/outputs/review.json` and `docs/refactors/<slug>/reports/review.md`.

**review.md structure:**

```
# Refactor Review — <Subsystem>

**Input:** Phase <N> changes

## Findings

### High Severity
- **<Title>**
  Evidence: <file:lines>
  Fix: <what to do>

### Medium Severity
- ...

### Low Severity
- ...

## Summary
[One paragraph on overall change health — only note what is wrong]
```

**review.json shape:**
```json
{
  "command": "review",
  "status": "ok",
  "data": {
    "findings": [
      {
        "severity": "high|medium|low",
        "title": "...",
        "evidence": [],
        "fix": "..."
      }
    ]
  }
}
```

Print the findings table after writing.
Print: "Review complete. Proceed to prove? (yes / no / address findings first)"
Wait for confirmation.

---

## Step 6 — Prove (human-gated)

Goal: validate that the important invariants still hold.

Read `docs/refactors/<slug>/outputs/plan.json` (acceptance criteria) and `docs/refactors/<slug>/outputs/review.json`.

Present the claims to validate:

```
== HUMAN SIGN-OFF REQUIRED ==

Claims to validate:
  1. <invariant from plan acceptance criteria>
  2. <invariant from plan acceptance criteria>
  ...

For each claim, I will look for evidence in: tests, code structure, review findings.
Proceed? (yes / no)
```

Wait for explicit "yes".

Write `docs/refactors/<slug>/outputs/prove.json` and `docs/refactors/<slug>/reports/prove.md`.

**prove.md structure:**

```
# Refactor Prove — <Subsystem>

**Input:** docs/refactors/<slug>/outputs/plan.json acceptance criteria

## Claims

### Claim: <invariant>
**Assessment:** supported | partial | unsupported | falsified
**Evidence for:** [...]
**Evidence against:** [...]
**Missing evidence:** [...]
**Next checks:** [...]
```

After writing, produce `docs/refactors/<slug>/artifacts/summary.md`:

```
# Refactor Summary — <Subsystem>

**Session folder:** docs/refactors/<slug>/
**Date:** YYYY-MM-DD

## One-Line Outcome
[What changed and why it is better]

## Loop Completed
1. **Audited:** [2-sentence summary of what was wrong]
2. **Planned:** [Target shape and number of phases]
3. **Executed:** [Phases applied, files changed, specs updated]
4. **Reviewed:** [Finding count by severity]
5. **Proved:** [Claims validated vs outstanding]

## Specs Updated
| Spec | Sections Changed |
|------|-----------------|

## Remaining Work
[Any outstanding phases, open findings, or missing evidence]
```

Print: "Refactor loop complete. Summary written to docs/refactors/<slug>/artifacts/summary.md"

---

## Cluiche Context (read before generating any content)

Engine modules available:
- DiaCore — containers, type system, CRC, memory, logging, JSON
- DiaMaths — vectors, matrices, shapes (pure linear algebra, no deps)
- DiaGeometry2D — 2D shapes, intersection, Grid/Quadtree/BVH
- DiaGraphics — ICanvas, Frame, rendering abstraction
- DiaWindow — Win32 window management
- DiaInput — keyboard, mouse, event queue
- DiaUI / DiaUIAwesomium / DiaUICEF / DiaUIUltralight — UI systems
- DiaApplicationFlow — ProcessingUnit, Phase, Module lifecycle
- DiaAPI — command/plugin framework
- DiaSFML — SFML integration layer
- DiaLogger — logging channels and sinks
- DiaRigidBody2D — 2D rigid-body physics
- DiaSoftBody2D — 2D soft-body physics (PBD)
- DiaWebSocket — WebSocket server/client
- DiaEditor — editor plugin framework
- DiaPython — embedded Python scripting

Applications: CluicheTest, CluicheEditor, GoogleTests

Binding platform constraints:
- PD-001: StringCRC for identifiers, not raw strings
- PD-002: ProcessingUnit/Phase/Module pattern
- PD-003: Component system (IComponent/IComponentObject)
- PD-004: No STL containers in public APIs — use DiaCore containers
- PD-005: x64 Windows only
- PD-007: C++20 required (/std:c++20)
