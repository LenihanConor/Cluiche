---
name: verify
description: Verification gate — every task completion requires fresh evidence. No "should work" allowed.
tags: [verification, completion, quality-gate]
user_invocable: false
agent_invocable: true
---

# Verification Gate

This is not a command — it's a behavioral rule that applies automatically whenever a task, feature, or implementation step is about to be marked "Done."

## The Rule

Before claiming any work is complete, you MUST:

1. **Identify the verification command** — what proves this works? (build, test filter, launch, specific output)
2. **Run it fresh** — not "I ran it earlier." Run it NOW, after all edits are saved.
3. **Read the full output** — not just "exit code 0." Read enough to confirm correctness.
4. **State the result with evidence** — quote the relevant output lines.

## Test Selection (Cost Control)

`dia run` / `dia pipeline` recompiles the full project on any code change — this is the dominant cost, and it is paid regardless of test filter. Narrowing the filter saves test *execution* time, not compile time, so it is a minor lever on its own. The primary lever is running builds less often (see Verification Batching below).

- Always use the narrowest `--filter` that proves the change(s) under verification. Never run the full unfiltered suite (`dia run googletest` / `dia pipeline --target googletest` with no `--filter`) for a task or batch.
- The full unfiltered suite is reserved exclusively for Phase 4 Finalize in `.claude/skills/implement.md`.
- When verifying a batch covering multiple tasks, use one filter that unions the affected suites (e.g. `--filter="SuiteA*:SuiteB*"`) — one combined run, not one per task.

## Verification Batching (Cost Control)

Because every `dia run`/`dia pipeline` invocation pays the full compile cost regardless of what changed, and routinely exceeds 5 minutes, verifying every task individually multiplies that fixed cost by the task count for no added safety on low-risk changes. Batch the *build*, not the *review*:

- **Eligible for batching:** tasks assigned Model = haiku, or Model = sonnet where the Test column does not require RED-GREEN proof.
- **Never batch:** Model = opus tasks, or any task under TDD RED-GREEN (`.claude/skills/tdd.md`). These verify solo, immediately, exactly as before.
- **Batch cap:** up to 5 tasks, or until cumulative diff exceeds ~150 changed lines / 5 files, whichever comes first.
- **Commits stay atomic per task regardless of batching** — see `.claude/skills/dispatch.md` § Verification Batching and `.claude/skills/implement.md` Phase 3. This is what keeps a batch bisectable: if the batch build fails, you have per-task commits to isolate against instead of needing a second full rebuild pass.
- On batch failure: do not treat it as one undifferentiated failure — bisect via the atomic commits (check the most-recently-added task first, since integration issues most often surface at the newest interaction point) before entering `.claude/skills/debug.md`.

## Long-Running Verification (Cost Control)

Essentially every `dia run`/`dia pipeline` invocation exceeds a couple of minutes. Run it via background execution (`run_in_background`) rather than blocking idle — use the wait to prepare the next task/batch's dispatch content or update the plan draft. This keeps the orchestrating conversation active through the wait instead of doing nothing with it.

## Banned Language

Never use these phrases when reporting completion:

- "This should work"
- "This will likely pass"
- "Based on the changes, this is correct"
- "Previously verified"
- "The logic is sound"
- "I'm confident this works"

Replace with: the actual output of the actual command.

## What Counts as Verification

| Change Type | Minimum Verification |
|-------------|---------------------|
| New code / modified logic | `dia run googletest --filter="RelevantSuite*"` passes |
| Build-only change (vcxproj, includes) | `dia pipeline --target <project>` succeeds |
| Test file added/modified | Run the specific test, confirm it appears in output and passes |
| Editor/UI feature | `dia run cluicheeditor`, confirm no crash + describe visible result |
| Game feature | `dia run cluichetest`, confirm no crash + describe visible result |
| Spec/doc-only change | N/A — no verification needed for pure documentation |
| Plan update | N/A — no verification needed |

## When Verification Is Impossible

If you genuinely cannot verify programmatically (e.g., visual-only UI change, behavior that requires user interaction), say so explicitly:

```
CANNOT VERIFY PROGRAMMATICALLY
What was done: <description>
What needs manual check: <specific thing to look for>
How to check: <steps for the user>
```

This is acceptable. What is NOT acceptable is pretending you verified when you didn't.

## Verification Failure

If verification fails (test doesn't pass, build breaks, unexpected output):
- Do NOT mark the task (or, if batched, any task in the batch) as done
- If the verification was batched, bisect via the atomic per-task commits first (see § Verification Batching) to isolate which task caused it
- Enter the debugging skill (Phase 1 — Investigate)
- The task(s) remain `Implemented — pending batch verification` or `In Progress` until verification passes

## Scope

This gate applies to:
- Each task row in a plan being marked `Done`, directly or via a batch verification that covers it (see § Verification Batching)
- Any commit message claiming something works
- Reporting a feature spec task as complete
- Subagent results that claim DONE (the orchestrator re-verifies)

This gate does NOT apply to:
- Intermediate edits (you don't need to verify every single line change)
- Research, spec writing, or documentation
- Plan creation or updates
