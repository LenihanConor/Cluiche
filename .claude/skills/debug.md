---
name: debug
description: Structured debugging process — investigate → hypothesize → test → verify. Enforces Three-Fix Ceiling and single-variable changes.
tags: [debugging, troubleshooting, build-failure, test-failure]
user_invocable: true
agent_invocable: true
---

# Systematic Debugging

Use this skill when something breaks: build failure, test failure, runtime crash, unexpected behavior. Follow the phases in order. Do NOT skip to "try random fixes."

## When This Activates

- Build fails during implementation
- Tests fail unexpectedly
- Runtime crash or assertion
- Behavior diverges from spec
- A subagent reports BLOCKED due to a failure

## Phase 1 — Investigate (gather evidence before touching anything)

1. **Read the full error output** — not just the first line. Scroll to the root cause.
2. **Identify the failure boundary** — which file, which line, which module?
3. **Check recent changes** — what was the last edit before the failure? (`git diff` or conversation context)
4. **Reproduce reliably** — run the failing command again to confirm it's consistent, not flaky.

Report to the user:
```
FAILURE: <one-line summary>
WHERE: <file:line or module>
LAST CHANGE: <what was edited before the break>
FULL ERROR: <key lines from output>
```

Do NOT propose a fix yet.

## Phase 2 — Hypothesize (form exactly one theory)

Based on the evidence from Phase 1, state a single hypothesis:

```
HYPOTHESIS: <what you think is wrong and why>
PREDICTED FIX: <the one change you would make>
PREDICTED RESULT: <what you expect to see if the hypothesis is correct>
```

Rules:
- One hypothesis at a time. Not "it could be A or B."
- The hypothesis must explain ALL the evidence, not just part of it.
- If you can't form a hypothesis, go back to Phase 1 and gather more evidence.

## Phase 3 — Test (apply the single fix)

1. Make exactly ONE change — the predicted fix from Phase 2.
2. Run the verification command (build, test, launch — whatever reproduces the failure).
3. Read the FULL output.

**Single-variable rule:** Never change two things at once. If you change the fix AND something else, you won't know which one mattered.

## Phase 4 — Verify (did it work?)

Compare the result to your prediction:

- **Hypothesis confirmed:** The fix worked and the output matches your prediction. Report success with evidence. Move on.
- **Hypothesis partially confirmed:** The original error is gone but a new one appeared. Start a new Phase 1 with the new error (this counts as a new fix attempt).
- **Hypothesis falsified:** The same error persists or the output doesn't match your prediction. Revert the change. Return to Phase 1 with new information (you now know what it ISN'T).

## Three-Fix Ceiling

After 3 failed fix attempts for the same root issue:

**STOP. Do not attempt a 4th fix.**

Instead, report:

```
THREE-FIX CEILING REACHED

Attempts:
1. <hypothesis> → <result>
2. <hypothesis> → <result>
3. <hypothesis> → <result>

What I've ruled out: <list>
What I still don't understand: <list>

Suggested next steps:
- <e.g., read more of the surrounding code>
- <e.g., check git history for when this last worked>
- <e.g., ask the user about intended behavior>
- <e.g., try a fundamentally different approach>
```

Wait for user direction before continuing.

## Anti-Patterns (never do these)

- **Shotgun debugging:** Changing multiple things hoping one sticks
- **Fix-forward without understanding:** "I'll just add a null check" without knowing WHY it's null
- **Reverting to "make it compile":** Commenting out code, adding `#pragma warning(disable:...)`, or stubbing functions just to get past the error
- **Ignoring context:** Not reading the full error, not checking what changed
- **Silent retry:** Running the same command repeatedly hoping for a different result
- **Scope creep:** "While I'm in here I'll also fix..." — stay focused on the one failure

## Integration with Plan Workflow

When debugging occurs mid-plan:
- The current task status becomes `In Progress (debugging)`
- If the Three-Fix Ceiling is reached, the task status becomes `Blocked` with a note summarizing attempts
- The fix (once found) is part of the same task — don't create a separate task for it
- If the fix reveals a plan flaw (wrong ordering, missing dependency), update the plan before continuing
