---
name: dispatch
description: Subagent dispatch protocol — how to brief, deploy, and verify subagent work during plan execution.
tags: [subagent, delegation, plan-execution, parallel]
user_invocable: false
agent_invocable: true
---

# Subagent Dispatch Protocol

Rules for how the orchestrating agent briefs, deploys, and verifies subagent work when executing plan tasks. Default the orchestrator itself to sonnet — see § Orchestrator Model for when to escalate to opus.

## Orchestrator Model (Cost Control)

Default the orchestrating agent itself to **sonnet**, not opus. Most of the loop — reading compact summaries, forming batches, updating plan rows, deciding commit order — is mechanical, not judgment-heavy, and running the whole multi-hour loop at opus pricing (2.5x sonnet on both input and output) taxes every turn for the sake of the few that need it.

Escalate to opus only for the specific moment that needs it, not the surrounding session:
- Stage 1/2 review calls that are genuinely ambiguous — not routine pattern-matching
- Three-Fix Ceiling debugging handoffs (`.claude/skills/debug.md`)
- Architecture/design decisions surfaced mid-plan
- Batch-boundary calls where the risk tiering itself is unclear

Escalation means dispatching a **fresh Agent call with `model: opus`** scoped to that one judgment call, inlining only the context it needs (per § Core Principle below) — not switching the orchestrator's own session model, and not trying to reason it out carefully at sonnet tier instead. Isolate the ambiguous decision; don't drag the surrounding mechanical work up to opus pricing along with it.

## Core Principle

A subagent starts with ZERO context from the current conversation. Everything it needs must be in the prompt. Never say "read the plan" or "check the spec" — inline the relevant content.

## Dispatch Template

When sending a task to a subagent, the prompt MUST include all of these sections. For batched haiku-tier dispatch (§ Batched Dispatch), repeat the full section set once per task, labeled "Task 1", "Task 2", etc. — never collapse multiple tasks into one shared section.

```
## Context
[2-3 sentences: what project this is, what module we're in, what the overall goal is]

## Spec Excerpt
[The specific acceptance criteria or task description from the feature spec — copied verbatim, not summarized]
[Include any binding decisions from the spec that constrain this task]

## Task
[Exactly what to implement/change — file paths, function names, expected behavior]
[For new feature tasks: specify RED-GREEN — "Write a failing test for <behavior>, confirm FAIL, then implement until PASS"]

## Existing Code
[Inline the relevant current state of files being modified — enough that the subagent doesn't need to explore]

## Constraints
- [Module boundaries: what this code can and cannot depend on]
- [Naming conventions if non-obvious]

## Verification
[The exact command to run and what passing looks like]
Run: dia run googletest --filter="SuiteName*"
Expected: All tests pass, no new warnings
[For TDD tasks: "Run test BEFORE implementation — confirm FAIL. Run AFTER — confirm PASS. Quote both outputs."]

## Report Format
When done, report ONE of the following, per task (for batched dispatch, one such block per task — a failure on one does not block reporting the others):
- DONE: <one-line summary> + a compact change list (file:line + one-line description per file — NOT a full diff) + verification output if this task verifies solo (see § Verification Batching — batch-eligible tasks skip the build and omit this)
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

## Compact Reporting (Cost Control)

Subagent DONE reports feed straight into the orchestrator's own conversation and stay there for the rest of the session. Full pasted diffs compound: every task adds its complete patch to a prefix that only grows, and that prefix gets rewritten at full price at every guaranteed cache-miss point (see § Verification Batching, and `.claude/skills/verify.md` § Long-Running Verification).

- Subagents report a compact change list — file:line + one-line description per changed file — not the full diff.
- The orchestrator performs Stage 1/2 review (below) against that summary. `Read` the actual file only when the summary looks incomplete, inconsistent with the spec, or otherwise suspicious — don't default to pulling the full patch into context.

## Verification Batching (Cost Control)

`dia run`/`dia pipeline` recompiles the full project on every invocation and routinely exceeds 5 minutes regardless of what changed — running it per task multiplies that fixed cost by the task count for no added safety on low-risk work. See `.claude/skills/verify.md` § Verification Batching for the full policy; the eligibility rule is encoded in the Model Selection table below (Batchable column). Batching applies to the build/test step only:

- **Stage 1/2 review still happens per task, immediately** — batching never delays code review, only the build.
- **Commits stay atomic per task** even inside a batch — this is what makes a batch bisectable if its build fails.
- **The orchestrator, not the subagent, owns the batch boundary** — a subagent never decides whether its task starts or closes a batch.

## Batched Dispatch (Cost Control)

Every dispatch pays a fixed overhead — a new subagent context gets assembled even though the model/tool/CLAUDE.md portion of it is cache-cheap. For haiku-tier mechanical tasks that are independent (different files, no shared headers — same independence test as § Parallel Dispatch Rules), the orchestrator MAY combine several into a single subagent dispatch instead of one dispatch per task, cutting that per-dispatch overhead N-fold.

Rules for combining tasks into one dispatch:
- **Haiku-tier only.** Never combine sonnet or opus tasks this way — they need the surgical, single-purpose framing a dedicated dispatch gives them.
- **Each task still gets its own full section** in the prompt (Context/Spec Excerpt/Task/Existing Code/Constraints — labeled "Task 1", "Task 2", ...). Combining tasks is not an excuse to write one vague shared prompt; that's still the Vague Dispatch anti-pattern.
- **The subagent reports DONE/BLOCKED per task**, each with its own compact change list (§ Compact Reporting) — one task failing doesn't block reporting the others.
- **Stage 1/2 review and commits still happen per task individually**, exactly as if each had its own dispatch — batched dispatch only removes the *dispatch* overhead, not the review/commit discipline.
- This composes with § Verification Batching: a batched-dispatch group's tasks still land in the same verification batch and share one build.

## Model Selection

Choose the model based on the task, per the Plan Workflow model guide:

| Task character | Model | Batchable | Why |
|---------------|-------|-----------|-----|
| Mechanical file edits, vcxproj updates, registry updates | haiku | Yes — also eligible for batched dispatch (§ Batched Dispatch) | Fast, cheap, pattern-following |
| Standard implementation, cross-file changes | sonnet | Yes for verification; never for dispatch | Good balance of speed and reasoning |
| Architecture decisions, complex state, debugging | opus | No — always verify solo, immediately, one dispatch each | Needs deep reasoning |
| Git commits, plan status updates | haiku | N/A — no build involved | Formulaic output |

## Post-Completion Verification (Two-Stage)

When a subagent reports DONE, the orchestrator performs two checks — from the compact change list, not a full diff (see § Compact Reporting) — before marking the task complete. These two stages run per task, immediately, regardless of whether the build/test step is batched:

### Stage 1 — Spec Compliance
- Does the output match the acceptance criteria from the spec?
- Are all requirements addressed, not just the easy ones?
- Were any constraints violated?

### Stage 2 — Code Quality
- Does it follow project naming conventions?
- Are there obvious bugs, missing error paths, or untested branches?
- Does it integrate cleanly with surrounding code?

If either stage fails, do NOT mark the task done. Either fix it yourself (if trivial) or dispatch a follow-up to the same subagent with specific feedback.

Passing Stage 1/2 makes a task **Implemented**, not **Done** — for batch-eligible tasks, Done is only reached once the batch's build/test passes (see `.claude/skills/implement.md` Phase 3). Commit the task now regardless; don't wait for the batch build to commit.

## Parallel Dispatch Rules

Multiple subagents can run concurrently ONLY when:
1. They touch different files (no edit conflicts)
2. They don't depend on each other's output
3. They don't both modify the same vcxproj or header

After parallel dispatch completes, run ONE consolidated integration check covering every task in the parallel batch — not one per task:
- Build the project once (`dia pipeline --target <project>`), with a test filter that unions the affected suites (never the full unfiltered suite — see `.claude/skills/verify.md` § Test Selection)
- Check for conflicting patterns (e.g., two subagents chose different naming for similar things)
- If it fails, bisect via each task's already-atomic commit (see § Verification Batching)

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
- **Full-diff DONE reports:** Subagent pastes its entire patch instead of a compact file:line summary — this compounds every task into the orchestrator's permanent context
- **Batching an opus/TDD task:** These always verify solo and immediately — never deferred into a batch build
- **Marking Done off Stage 1/2 alone:** Passing spec/quality review makes a batch-eligible task Implemented, not Done — Done requires the batch build to actually pass
- **Batched dispatch without per-task sections:** Combining tasks into one dispatch but writing one loose shared prompt instead of a full section per task — that's Vague Dispatch wearing a cost-saving label
- **Batched dispatch for sonnet/opus tasks:** Reserve combined dispatch for independent haiku-tier mechanical tasks only
- **Always defaulting to opus for the orchestrator:** Default sonnet; escalate a single fresh opus call for the specific ambiguous moment (§ Orchestrator Model), don't run the whole loop at opus tier out of caution
