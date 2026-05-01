---
name: refactor-audit
description: Run the audit stage of a refactor session — diagnose structural problems and write audit.json + reports/audit.md
tags: [refactor, audit, diagnosis]
user_invocable: true
agent_invocable: true
---

# Refactor: Audit Stage

Diagnose the structural health of a subsystem. This is a diagnosis step — not a solution step.

## Usage

```
/refactor-audit <subsystem_slug>
/refactor-audit <subsystem_slug> --lens <ownership|coupling|duplication|tests|performance>
```

## Instructions for Claude

### 1. Resolve the Session

If a subsystem slug is provided, check whether `docs/refactors/<slug>/` exists.

If no slug is provided:
- List all subfolders in `docs/refactors/` if any exist
- Ask: "Which subsystem do you want to audit? (or type a new subsystem name)"

Derive the slug if a full name is given:
- Lowercase, replace spaces with underscores
- Example: "DiaApplication" → `dia_application`

If `docs/refactors/<slug>/outputs/audit.json` already exists:
- Warn: "audit.json already exists for this session. Regenerate? (yes/no)"
- Wait for confirmation before overwriting.

### 2. Read Context

Read silently before generating anything:
- @.claude/steering/tech.md
- @.claude/steering/structure.md

Also read any relevant source files for the subsystem. Look in:
- `Dia/<SubsystemName>/` — primary module files
- Relevant `.h` and `.cpp` files for the subsystem's public API and core logic
- Any existing spec: `docs/specs/systems/dia/<slug>.md` or `docs/specs/features/dia/<slug>/`

### 3. Audit the Subsystem

Diagnose across these dimensions (adjust emphasis based on `--lens` if provided):

- **What the subsystem is trying to do** — its stated purpose and actual behavior
- **Current structure** — key classes, ownership graph, data flow
- **Debt** — architectural drift, unclear ownership, lifetime issues, mixed concerns
- **Duplication** — logic repeated across files or modules that should be centralized
- **Hidden coupling** — implicit dependencies, undocumented assumptions, surprising ties to other subsystems
- **Missing tests** — invariants that are unprotected or untested
- **Likely invariants** — behavioral contracts that a refactor must preserve

When `--lens` is provided, focus the report on that dimension but still cover others briefly.

### 4. Write Artifacts

Create `docs/refactors/<slug>/outputs/audit.json`:

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

Create `docs/refactors/<slug>/reports/audit.md`:

```markdown
# Refactor Audit — <Subsystem>

**Session date:** YYYY-MM-DD
**Folder:** docs/refactors/<slug>/
**Lens:** <lens or "general">

## Subsystem Summary
[What this subsystem does and its current scope]

## Strengths
[Bulleted list — do not remove these in the refactor]

## Debt
[Bulleted list of structural problems with file references where possible]

## Duplication
[Bulleted list — name the locations]

## Hidden Coupling
[Bulleted list — name the dependency and why it is surprising]

## Missing Tests
[Bulleted list — name the invariant that is unprotected]

## Likely Invariants to Preserve
[Bulleted list — these are the claims that prove must validate later]

## Recommended Cleanup Order
[Ordered list — highest impact, lowest risk first]
```

### 5. Report

Print:
```
Audit complete for <subsystem>.
  Debt items:     <N>
  Invariants:     <N>
  Cleanup order:  <first item>

Next step: /refactor-plan <slug>
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplication, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20

When auditing: flag any violation of PD-001 through PD-007 as debt. Note which Dia module the subsystem belongs to and whether its boundaries are clean relative to its declared responsibilities.
