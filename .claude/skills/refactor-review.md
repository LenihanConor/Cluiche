---
name: refactor-review
description: Run the review stage of a refactor session — adversarial inspection of applied changes, findings only
tags: [refactor, review, quality]
user_invocable: true
agent_invocable: true
---

# Refactor: Review Stage

Inspect the applied refactor changes and report problems. This step is adversarial and specific — it produces findings only, not praise.

## Usage

```
/refactor-review <subsystem_slug>
/refactor-review <subsystem_slug> --phase <N>
```

If `--phase` is omitted, review all changes applied so far in the session.

## Instructions for Claude

### 1. Resolve the Session

Check that `docs/refactors/<slug>/outputs/execute.json` exists. If it does not:
- Print: "execute.json not found for docs/refactors/<slug>/. Run /refactor-execute <slug> first."
- Stop.

If `docs/refactors/<slug>/outputs/review.json` already exists:
- Warn: "review.json already exists for this session. Regenerate? (yes/no)"
- Wait for confirmation before overwriting.

### 2. Read Context

Read:
- `docs/refactors/<slug>/outputs/plan.json` — for the acceptance criteria and constraints
- `docs/refactors/<slug>/outputs/execute.json` — for the list of files edited
- `docs/refactors/<slug>/outputs/audit.json` — for the original debt and invariants
- @.claude/steering/tech.md

Read each file listed in `execute.json → data.files_edited`. Inspect the current state of those files — not a diff, the actual code.

### 3. Review the Changes

Apply an adversarial lens. Ask for each changed file and each edit:

- **Correctness**: Does this change introduce a bug, a use-after-free, an uninitialized value, or a broken invariant?
- **Ownership**: Is resource ownership explicit? Could this introduce a leak or double-free?
- **Coupling**: Does this introduce a new hidden dependency or a circular include?
- **Hot-path safety**: Does this add an allocation, virtual dispatch, or STL container on a path that was previously cheap?
- **PD compliance**: Does this violate PD-001 through PD-007?
- **Spec consistency**: Do the spec updates match the code changes? Is anything in the spec now factually wrong?
- **Completeness**: Are there call sites, tests, or build rules that still reference the old shape?

Do NOT comment on style, naming preference, or things that are a matter of taste. Report only genuine problems.

### 4. Classify Findings

Severity definitions:
- **High** — correctness bug, broken invariant, resource leak, PD violation, or change that silently alters behavior
- **Medium** — incomplete migration (old path still present when it should be gone), missed call site, or spec section that is now wrong
- **Low** — latent risk that isn't currently broken but will cause problems if the refactor continues

### 5. Write Artifacts

Create `docs/refactors/<slug>/outputs/review.json`:

```json
{
  "command": "review",
  "status": "ok",
  "data": {
    "phase_reviewed": 1,
    "findings": [
      {
        "severity": "high|medium|low",
        "title": "...",
        "evidence": ["file.cpp:120-130"],
        "fix": "..."
      }
    ]
  }
}
```

Create `docs/refactors/<slug>/reports/review.md`:

```markdown
# Refactor Review — <Subsystem>

**Input:** Phase <N> changes
**Date:** YYYY-MM-DD

## Findings

### High Severity
- **<Title>**
  Evidence: `<file:lines>`
  Fix: <what to do>

### Medium Severity
- ...

### Low Severity
- ...

## Summary
[One paragraph on the overall health of the change — only what is wrong, not what is right]
```

If there are zero findings, write:
```
## Findings

No findings. The applied changes appear correct against the plan's constraints and acceptance criteria.
```

### 6. Report

Print the findings count table, then:
```
Review complete.
  High:   <N>
  Medium: <N>
  Low:    <N>

Next step: /refactor-prove <slug>
  (address high-severity findings before proving)
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplicationFlow, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20

When reviewing: treat any PD constraint violation as a high-severity finding regardless of how small it appears. Treat any spec section that now contradicts the code as a medium-severity finding.
