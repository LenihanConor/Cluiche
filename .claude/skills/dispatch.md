---
name: dispatch
description: Subagent dispatch protocol — how to brief, deploy, and verify subagent work during plan execution.
tags: [subagent, delegation, plan-execution, parallel]
user_invocable: false
agent_invocable: true
---

# Subagent Dispatch Protocol

Rules for how the orchestrating agent (opus) briefs, deploys, and verifies subagent work when executing plan tasks.

## Core Principle

A subagent starts with ZERO context from the current conversation. Everything it needs must be in the prompt. Never say "read the plan" or "check the spec" — inline the relevant content.

## Dispatch Template

When sending a task to a subagent, the prompt MUST include all of these sections:

```
## Context
[2-3 sentences: what project this is, what module we're in, what the overall goal is]

## Spec Decisions Summary
[One paragraph: the binding decisions and constraints from the spec chain that apply to this task.
 Copy this from the plan's Session Notes if it exists — avoids re-reading the full spec chain.]

## Spec Excerpt
[The specific acceptance criteria or task description from the feature spec — copied verbatim, not summarized]

## Task
[Exactly what to implement/change — file paths, function names, expected behavior]
[For new feature tasks: specify RED-GREEN — "Write a failing test for <behavior>, confirm FAIL, then implement until PASS"]

## Existing Code
[Inline the relevant current state of files being modified — enough that the subagent doesn't need to explore]

## Constraints
- [Platform decisions that apply: PD-001 through PD-007 if relevant]
- [Module boundaries: what this code can and cannot depend on]
- [Naming conventions if non-obvious]

## Verification
[The exact command to run and what passing looks like]
Run: dia run googletest --filter="SuiteName*"
Expected: All tests pass, no new warnings
[For TDD tasks: "Run test BEFORE implementation — confirm FAIL. Run AFTER — confirm PASS. Quote both outputs."]

## Report Format
When done, report ONE of:
- DONE: <one-line summary> + verification output
- BLOCKED: <what failed> + <what was tried> + <what's needed>
- NEEDS_CONTEXT: <specific question> + <what you need to proceed>
```

## What to Inline vs. What to Let the Subagent Read

**Always inline:**
- The task description and acceptance criteria
- Code that will be modified (current state)
- Closely related code the subagent needs to understand patterns from
- Platform constraints and naming conventions that apply

**OK to let the subagent read:**
- Large files it needs to add entries to (e.g., vcxproj — just say "add to the vcxproj following the existing pattern")
- Test files it needs to follow patterns from (point at one example file)
- Architecture module docs (give the path)

**Never make the subagent read:**
- The plan file (it doesn't need to know about other tasks)
- The full spec (give it only the relevant excerpt)
- CLAUDE.md (it gets this automatically)

## Model Selection

Choose the model based on the task, per the Plan Workflow model guide:

| Task character | Model | Why |
|---------------|-------|-----|
| Mechanical file edits, vcxproj updates, registry updates | haiku | Fast, cheap, pattern-following |
| Standard implementation, cross-file changes | sonnet | Good balance of speed and reasoning |
| Architecture decisions, complex state, debugging | opus | Needs deep reasoning |
| Git commits, plan status updates | haiku | Formulaic output |

## Post-Completion Verification (Two-Stage)

When a subagent reports DONE, the orchestrator performs two checks before marking the task complete:

### Stage 1 — Spec Compliance
- Does the output match the acceptance criteria from the spec?
- Are all requirements addressed, not just the easy ones?
- Were any constraints violated?

### Stage 2 — Code Quality
- Does it follow project naming conventions?
- Are there obvious bugs, missing error paths, or untested branches?
- Does it integrate cleanly with surrounding code?

If either stage fails, do NOT mark the task done. Either fix it yourself (if trivial) or dispatch a follow-up to the same subagent with specific feedback.

## Parallel Dispatch Rules

Multiple subagents can run concurrently ONLY when:
1. They touch different files (no edit conflicts)
2. They don't depend on each other's output
3. They don't both modify the same vcxproj or header

After parallel dispatch completes, run an integration check:
- Build the full project (`dia pipeline --target <project>`)
- Run relevant tests
- Check for conflicting patterns (e.g., two subagents chose different naming for similar things)

## BLOCKED Handling

When a subagent reports BLOCKED:
1. Read its explanation of what was tried
2. Determine if this is a debugging issue (enter debug skill) or a plan issue (missing dependency, wrong ordering)
3. If plan issue: update the plan, reorder tasks, then re-dispatch
4. If debugging issue: fix the blocker (possibly with opus), then re-dispatch the original task

## Anti-Patterns

- **Vague dispatch:** "Implement task 3 from the plan" — subagent has no plan context
- **Over-delegation:** Sending architecture decisions to haiku/sonnet — use opus for judgment calls
- **No verification command:** Subagent finishes but neither it nor you run the tests
- **Blind trust:** Marking a task done because the subagent said DONE without checking the actual output
- **Context dump:** Pasting 500 lines of irrelevant code — be surgical about what's needed
- **Serial when parallel is safe:** Running 3 independent file edits one at a time when they could be concurrent
