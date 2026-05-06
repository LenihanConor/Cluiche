Review implementation work against its spec. Runs 8 passes, outputs a concise punch list.

Usage: `/review <spec-path> [options]`

Arguments:
- `<spec-path>`: Path to the feature or system spec (e.g., `docs/specs/features/my-feature.md`)
- `--diff=<range>`: Git diff range (default: diff of current branch against its base)
- `--pass=<name>`: Run only one pass (spec, tests, architecture, product, performance, threading, binding, api)
- `--severity=<level>`: Only show findings at this level or above (critical, important, minor)

---

## Instructions for Claude

### Step 1 — Gather Context

1. Read the spec at `<spec-path>`. Extract:
   - All Acceptance Criteria (ACs)
   - Binding decisions from the compliance table
   - Task list
   - Parent spec references (read parent for binding decisions if needed)
2. Get the diff:
   - Default: `git diff $(git merge-base HEAD master)..HEAD` (all commits on this branch)
   - Or use `--diff=<range>` if specified
3. Identify all files changed and their modules.

### Step 2 — Run Passes

Run each pass against the diff + spec. For each finding, record: severity, pass name, one-line finding, file:line (if applicable), one-line suggestion.

**Severity rules:**
- **Critical** — Blocks merge. Broken behavior, data loss risk, crash, race condition, spec AC not met.
- **Important** — Fix before merge. Missing test coverage, wrong module boundary, performance issue on hot path, API misuse risk.
- **Minor** — Note for later. Naming inconsistency, style nit, opportunity for improvement. Does NOT block.

#### Pass 1: Spec Compliance
- For each AC in the spec: is it implemented? Is it tested?
- Flag any AC that is not exercised by the diff or by a test.
- Flag behavior that contradicts the spec.

#### Pass 2: Test Exhaustiveness
- For each new public method/class: is there at least one test?
- For each AC: is there a test that proves it?
- Check test types against component classification (per `/gen-tests` logic):
  - Math/numeric → golden-value + invariant?
  - Container → stress + boundary?
  - Physics → determinism + conservation?
  - State machine → illegal transition + rapid transition?
- Flag untested edge cases visible in the implementation (empty input, overflow, null, max capacity).

#### Pass 3: Architecture
- Is new code in the right module? Check `dia.*.architecture.module.md` responsibilities.
- Does it introduce dependencies that violate the module's declared dependency list?
- Does it duplicate logic that already exists elsewhere?
- Are lifetimes and ownership clear?
- Does the file/class structure follow existing patterns in the same module?

#### Pass 4: Product
- Would a real user hit an edge case the spec didn't cover?
- Is there a failure mode that produces no feedback (silent failure)?
- If there's UI: is the interaction obvious without documentation?
- Does the feature compose well with existing features?

#### Pass 5: Performance
- Any allocation on a per-frame or hot-path code path?
- Any O(n^2) or worse where linear is achievable?
- Any cache-unfriendly access pattern (pointer chasing, scattered reads)?
- Any unnecessary copy where a reference/move suffices?
- If it's a container: is the growth strategy reasonable?

#### Pass 6: Thread Safety
- Does new code access state shared across ProcessingUnits or Phases?
- If yes: is synchronization in place (mutex, atomic, queue)?
- Is `QueuePhaseTransition()` used instead of `TransitionPhase()` for cross-thread transitions?
- Any lock ordering issues (potential deadlock)?
- Any fire-and-forget writes to shared state?

#### Pass 7: Binding Decisions
- PD-001: StringCRC for identifiers? (no raw string comparison for IDs)
- PD-002: ProcessingUnit/Phase/Module pattern respected?
- PD-003: Component system used where entities are involved?
- PD-004: No STL containers in public API headers?
- PD-005: x64 Windows assumptions valid?
- PD-007: C++20 features only?

#### Pass 8: API Surface (conditional — skip if no new public headers)
- Is the public interface minimal? Anything that could be private/internal?
- Can a caller misuse the API easily? (e.g., wrong call order, forgetting to call Init)
- Are preconditions documented via DIA_ASSERT?
- Is the naming consistent with adjacent APIs in the same module?
- Would this be painful to change later? (if yes, flag for extra scrutiny)

### Step 3 — Output

Produce the review in this exact format:

```markdown
## Review: <Spec Title>

**Spec:** <spec-path>
**Diff:** <range or "current branch vs master">
**Files changed:** <count>
**Verdict:** PASS | PASS WITH ISSUES | BLOCKED

| # | Severity | Pass | Finding | Location | Suggestion |
|---|----------|------|---------|----------|------------|
| 1 | Critical | ... | ... | file:line | ... |
| 2 | Important | ... | ... | file:line | ... |
| 3 | Minor | ... | ... | file:line | ... |

### Unverified ACs
- [ ] AC<n>: <description> — <what's missing: implementation, test, or both>

### Summary
<2-3 sentences: overall health, what blocks, what's fine>
```

**Format rules:**
- One row per finding. No multi-sentence findings — split into separate rows.
- Location is `file:line` or `—` if it's an absence (missing test, missing AC).
- Sort by severity (Critical first), then by pass order within severity.
- If a pass finds nothing: do not mention it. No "Pass 5: no issues found."
- If ALL passes find nothing: Verdict is PASS, table is empty, summary says "Clean."

**Verdict logic:**
- **BLOCKED** — Any Critical finding exists
- **PASS WITH ISSUES** — No Critical, but Important findings exist
- **PASS** — Only Minor findings or none

### Step 4 — Offer Next Steps

After the table, ask:

```
Fix critical/important findings now? (yes / no / pick specific #s)
```

If yes: address findings in severity order (Critical first). Run verification gate after each fix.
