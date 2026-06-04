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

4. Select models per task:
   - haiku: file edits, vcxproj changes, registry updates, plan updates, git commits
   - sonnet: most implementation tasks, cross-file reasoning, spec work
   - opus: architecture decisions, complex debugging, state management, graph/visual components

5. Update the spec's Status section to reference the plan file.

### Phase 3 — Execute Loop

For each task in order (skip tasks marked Done/Skipped):

#### 3a. Dispatch

Dispatch the task to a subagent following `.claude/skills/dispatch.md`:
- Inline all context: spec excerpt, relevant code paths, binding decisions, test approach
- Set the model per the plan's Model column
- Tell the subagent: implement the task, run tests, report DONE or BLOCKED

#### 3b. Verify

After the subagent reports DONE:
1. Check spec compliance — does the implementation match the spec?
2. Check code quality — naming conventions, no singletons without approval, proper includes
3. Run verification per `.claude/skills/verify.md` — fresh `dia run` or `dia pipeline`

If verification fails → mark task BLOCKED in plan, fix or re-dispatch.

#### 3c. Commit

If verification passes:
1. Stage only files related to this task
2. Commit with a descriptive message referencing the feature
3. Update plan: mark task Done, add notes

#### 3d. Continue

Move to next task. If a task is BLOCKED:
- Try to diagnose using `.claude/skills/debug.md`
- If unblockable, mark BLOCKED and continue to next independent task
- Report blocked tasks at the end

### Phase 4 — Finalize

After all tasks complete:
1. Update plan status to `Done`
2. Update spec status to `Done`
3. Run full verification: `dia pipeline --target googletest`
4. Run Observation Opportunity Scan (per CLAUDE.md) on all files touched
5. Report summary: tasks completed vs blocked, files created/modified, test results, observation opportunities

## Resume Behavior

If a plan file already exists at the expected path:
1. Read it to determine which tasks are Done/Blocked/Pending
2. Skip Phase 2
3. Resume from the first Pending task in Phase 3

## Anti-Patterns

- Do NOT implement multiple tasks in a single subagent dispatch — one task per dispatch
- Do NOT skip verification — even for "trivial" tasks
- Do NOT commit multiple tasks in a single commit
- Do NOT proceed if the spec is not Approved
- Do NOT modify the spec during implementation — raise design questions to the user
- Do NOT dispatch tasks in parallel unless truly independent (different files, no shared headers)
