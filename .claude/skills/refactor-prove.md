---
name: refactor-prove
description: Run the prove stage of a refactor session — validate that important invariants still hold after the refactor (human-gated)
tags: [refactor, prove, validation]
user_invocable: true
agent_invocable: false
---

# Refactor: Prove Stage

Validate that the important invariants and acceptance criteria still hold after the refactor. This is a sign-off step — not a search for new problems (that is review's job).

This step is **human-gated**. Claude presents the claims to validate and waits for approval before proceeding.

## Usage

```
/refactor-prove <subsystem_slug>
/refactor-prove <subsystem_slug> --claim "<specific claim to validate>"
```

If `--claim` is provided, validate only that claim. Otherwise validate all acceptance criteria from the plan.

## Instructions for Claude

### 1. Resolve the Session

Check that all of these exist:
- `docs/refactors/<slug>/outputs/plan.json`
- `docs/refactors/<slug>/outputs/execute.json`
- `docs/refactors/<slug>/outputs/review.json`

If any are missing, print the missing prerequisite and stop.

If there are unresolved high-severity findings in `review.json`, warn:
```
Warning: <N> high-severity finding(s) remain unresolved in review.json.
Proving invariants before addressing these may give a false sense of completion.
Proceed anyway? (yes / no)
```
Wait for response.

If `docs/refactors/<slug>/outputs/prove.json` already exists:
- Warn: "prove.json already exists. Regenerate? (yes/no)"
- Wait for confirmation.

### 2. Read Context

Read:
- `docs/refactors/<slug>/outputs/plan.json` — extract `data.acceptance_criteria`
- `docs/refactors/<slug>/outputs/review.json` — for any findings that bear on the claims
- `docs/refactors/<slug>/outputs/execute.json` — for files edited and specs updated

Read the current state of the edited source files listed in `execute.json`.

### 3. Present Claims and Request Approval

Present the claims to validate:

```
== HUMAN SIGN-OFF REQUIRED ==

Claims to validate (from plan acceptance criteria):
  1. <claim>
  2. <claim>
  ...

For each claim I will look for evidence in:
  - Code structure of edited files
  - Test files (if any exist for this subsystem)
  - Review findings
  - Build rules and include graph (where relevant)

Proceed? (yes / no / add claim)
```

Wait for "yes". If user says "add claim", ask for the claim text and add it to the list, then re-present.

### 4. Validate Each Claim

For each claim, look for:

**Evidence for:**
- Code that directly enforces the invariant (e.g. a lifetime guard, an assertion, a clear ownership rule)
- Tests that cover the behavior
- Review findings that confirm the change was clean in this area

**Evidence against:**
- Code paths that bypass or ignore the invariant
- Review findings that flag a correctness problem in this area
- Files that were NOT updated but still reference the old shape

**Missing evidence:**
- Areas the code does not reach (e.g. resize path, stress scenario, multi-threaded path)
- Tests that don't exist yet
- Scenarios that require runtime verification beyond static analysis

**Assessment:**
- `supported` — strong evidence, no counter-evidence, gaps are minor
- `partial` — some evidence, some gaps, no active counter-evidence
- `unsupported` — insufficient evidence either way
- `falsified` — active counter-evidence that the invariant does not hold

### 5. Write Artifacts

Create `docs/refactors/<slug>/outputs/prove.json`:

```json
{
  "command": "prove",
  "status": "ok",
  "data": {
    "claims": [
      {
        "claim": "...",
        "assessment": "supported|partial|unsupported|falsified",
        "evidence_for": [],
        "evidence_against": [],
        "missing_evidence": [],
        "next_checks": []
      }
    ]
  }
}
```

Create `docs/refactors/<slug>/reports/prove.md`:

```markdown
# Refactor Prove — <Subsystem>

**Date:** YYYY-MM-DD
**Claims validated:** <N>

## Claims

### Claim: <invariant>
**Assessment:** supported | partial | unsupported | falsified
**Evidence for:**
- ...
**Evidence against:**
- ...
**Missing evidence:**
- ...
**Next checks:**
- ...

[Repeat for each claim]

## Overall Assessment
[One paragraph: is the refactor safe to keep? What remains uncertain?]
```

After writing prove.md, write `docs/refactors/<slug>/artifacts/summary.md`:

```markdown
# Refactor Summary — <Subsystem>

**Session folder:** docs/refactors/<slug>/
**Date:** YYYY-MM-DD

## One-Line Outcome
[What changed and why it is better in one sentence]

## Loop Completed
1. **Audited:** [2-sentence summary of what was wrong]
2. **Planned:** [Target shape, N migration phases]
3. **Executed:** [Phases applied, N files changed, N specs updated]
4. **Reviewed:** [N findings: H high, M medium, L low]
5. **Proved:** [N claims: X supported, Y partial, Z unsupported/falsified]

## Specs Updated
| Spec | Sections Changed |
|------|-----------------|

## Remaining Work
[Outstanding phases, open findings, unsupported/falsified claims, missing evidence to collect]
```

### 6. Report

Print the claim assessment table, then:

```
Prove complete.
  Supported:    <N>
  Partial:      <N>
  Unsupported:  <N>
  Falsified:    <N>

Summary written to docs/refactors/<slug>/artifacts/summary.md

[If any falsified claims:]
  WARNING: <N> claim(s) falsified — the refactor should not be merged until these are resolved.

[If all supported:]
  Refactor loop complete. All invariants hold.
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplicationFlow, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20

When proving: a claim about PD compliance should check actual source, not just the plan's intent. A claim about test coverage should look for actual test files in `Cluiche/Tests/UnitTests/` or adjacent to the subsystem. Do not assess a claim as "supported" based solely on the plan saying it should be true.
