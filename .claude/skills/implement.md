---
name: implement
description: Orchestrate full implementation from an approved spec — plan, dispatch, verify, commit per task.
tags: [implementation, spec, plan, dispatch, orchestration]
user_invocable: true
agent_invocable: false
---

# Implement

Orchestrates the full implementation loop for an approved feature spec: create a plan, dispatch each task to a subagent, verify, commit, repeat until done.

## Usage

```
/implement <spec-path>
```

Examples:
- `/implement docs/specs/features/dia/diaanimation2d/timeline.md`
- `/implement docs/specs/features/dia/diaentityeditor/hierarchy-panel.md`

## Prerequisites

Before starting:
1. The spec at `<spec-path>` must exist and have status `Approved`
2. The branch should be clean (no uncommitted changes unrelated to this feature)

## Instructions for Claude

### Phase 1 — Validate

1. Read the spec file. Confirm status is `Approved`.
2. Read the parent system spec (from the spec's parent link) to get binding decisions.
3. If spec is not Approved, STOP and tell the user.

### Phase 2 — Create Plan

If a plan file already exists at the expected path, skip to Phase 3 (resume).

1. Derive the plan file path: same directory as the spec, same filename but `.plan.md` extension.
2. Read the spec's task list (under `## Tasks` or `## Implementation Tasks`).
3. Create the plan file with this format:

```markdown
**Spec:** @<spec-path>
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | <task from spec> | <test approach> | Pending | <model> | |
| 2 | ... | ... | Pending | ... | |
```

Status values: `Pending` → `Implemented — pending batch verification` (batch-eligible tasks only, once Stage 1/2 review passes and the task is committed) → `Done` (batch/solo build passed) → `Blocked` → `Skipped`.

4. Select models per task:
   - haiku: file edits, vcxproj changes, registry updates, plan updates, git commits
   - sonnet: most implementation tasks, cross-file reasoning, spec work
   - opus: architecture decisions, complex debugging, state management, graph/visual components

5. Mark tasks that require RED-GREEN proof in the Test column (e.g. "RED-GREEN: ..."). Those, plus every opus-tier task, are excluded from verification batching in Phase 3 — see `.claude/skills/verify.md` § Verification Batching.

6. Update the spec's Status section to reference the plan file.

### Phase 3 — Execute Loop

Before dispatching, group the remaining Pending tasks into **verification batches** per `.claude/skills/verify.md` § Verification Batching: consecutive batch-eligible tasks (Model = haiku, or Model = sonnet without RED-GREEN required) group up to 5 tasks or ~150 changed lines / 5 files, whichever comes first. Every opus-tier task and every RED-GREEN task forms a batch of exactly one — these always verify solo and immediately, unchanged from the original per-task flow.

For each batch, in order:

#### 3a. Dispatch

Dispatch tasks in the batch following `.claude/skills/dispatch.md`:
- Inline all context: spec excerpt, relevant code paths, binding decisions, test approach
- Set the model per the plan's Model column
- Batch-eligible tasks: implement only — no build/test, that happens once for the whole batch in 3d. Solo (opus/RED-GREEN) tasks: implement AND run their own verification immediately, per `.claude/skills/verify.md`.
- Tell the subagent to report DONE (with a compact change list, not a full diff — see `.claude/skills/dispatch.md` § Compact Reporting) or BLOCKED, per task
- **One subagent per task by default.** Exception: if several haiku-tier tasks in the batch are independent (different files, no shared headers), combine them into a single subagent dispatch per `.claude/skills/dispatch.md` § Batched Dispatch — one dispatch, one section per task, one DONE/BLOCKED report per task. Never combine sonnet or opus tasks this way.

#### 3b. Review (per task, immediately — not deferred by batching)

As each subagent in the batch reports DONE:
1. Check spec compliance — does the implementation match the spec?
2. Check code quality — naming conventions, no singletons without approval, proper includes
3. Work from the compact change list; `Read` the actual file only if something looks off

If either check fails → do not commit; fix or re-dispatch that task before moving on. This does not wait for the rest of the batch.

#### 3c. Commit (per task, atomic — always, regardless of batching)

Once a task passes review:
1. Stage only files related to this task
2. Commit with a descriptive message referencing the feature
3. Batch-eligible task → mark plan status `Implemented — pending batch verification`. Solo task → run its verification now per `.claude/skills/verify.md`, then mark `Done` if it passes.

#### 3d. Batch Verify (once per batch, batch-eligible tasks only)

Once every task in the batch has been implemented, reviewed, and committed:
1. Run one build/test pass covering the whole batch — narrowest filter that unions the affected suites, never the full unfiltered suite (see `.claude/skills/verify.md` § Test Selection)
2. Run it in the background; use the wait to prepare the next batch's dispatch content rather than idling (see `.claude/skills/verify.md` § Long-Running Verification)
3. Passes → mark every task in the batch `Done`
4. Fails → do NOT mark any task in the batch `Done`. Bisect via the batch's atomic per-task commits (most-recently-added task first) to isolate the break, then enter `.claude/skills/debug.md`. Re-run the batch verification once fixed.

#### 3e. Continue / Checkpoint

After a batch reaches `Done` (or a task is `Blocked`), stop and report a batch summary. Explicitly tell the user the conversation has grown and recommend running `/clear` (or starting a new session) before resuming — pausing without clearing keeps the same accumulated context and defeats the point of checkpointing. Resuming `/implement <spec-path>` in the fresh session picks up cleanly via Phase 2's plan-file Resume Behavior. Continuous unattended execution across the whole plan in one conversation is only appropriate when the user has explicitly asked for hands-off execution.

If a task is BLOCKED:
- Try to diagnose using `.claude/skills/debug.md`
- If unblockable, mark BLOCKED and continue to the next independent task (or batch)
- Report blocked tasks at the end

### Phase 4 — Finalize

After all tasks complete. This is the ONLY point in the workflow that runs the full unfiltered suite — every Phase 3 verification, batched or solo, uses a scoped filter.

1. Update plan status to `Done`
2. Update spec status to `Done`
3. Run full verification: `dia pipeline --target googletest` (unfiltered)
4. Run Observation Opportunity Scan (per CLAUDE.md) on all files touched
5. Report summary: tasks completed vs blocked, files created/modified, test results, observation opportunities

## Resume Behavior

If a plan file already exists at the expected path:
1. Read it to determine which tasks are Done/Implemented/Blocked/Pending
2. Skip Phase 2
3. Any task marked `Implemented — pending batch verification` means its batch's build never completed last session — re-form that batch (plus any remaining Pending tasks up to the batch cap) and resume at Phase 3d
4. Otherwise, resume from the first Pending task/batch in Phase 3

## Anti-Patterns

- Do NOT implement multiple tasks in a single subagent dispatch, except independent haiku-tier tasks explicitly combined per `.claude/skills/dispatch.md` § Batched Dispatch — each task still gets its own full section, its own report, its own review, its own commit
- Do NOT skip verification — even for batches of "trivial" tasks, the batch still needs one confirmed build/test pass before any task in it is Done
- Do NOT run the orchestrator itself at opus by default — default sonnet, escalate a single scoped opus call for the specific judgment moment (see `.claude/skills/dispatch.md` § Orchestrator Model)
- Do NOT commit multiple tasks in a single commit — commits stay atomic per task even inside a verification batch
- Do NOT proceed if the spec is not Approved
- Do NOT modify the spec during implementation — raise design questions to the user
- Do NOT dispatch tasks in parallel unless truly independent (different files, no shared headers)
- Do NOT batch an opus-tier or RED-GREEN task — these always verify solo, immediately
- Do NOT mark a batch-eligible task `Done` off Stage 1/2 review alone — that makes it `Implemented`; `Done` requires the batch build to actually pass
- Do NOT run the full unfiltered suite outside Phase 4
- Do NOT auto-continue into the next batch in the same conversation by default — checkpoint and let Resume Behavior pick it up, unless the user asked for continuous hands-off execution
