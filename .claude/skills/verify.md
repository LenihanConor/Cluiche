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
- Do NOT mark the task as done
- Enter the debugging skill (Phase 1 — Investigate)
- The task remains `In Progress` until verification passes

## Scope

This gate applies to:
- Each task row in a plan being marked `Done`
- Any commit message claiming something works
- Reporting a feature spec task as complete
- Subagent results that claim DONE (the orchestrator re-verifies)

This gate does NOT apply to:
- Intermediate edits (you don't need to verify every single line change)
- Research, spec writing, or documentation
- Plan creation or updates
