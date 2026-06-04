Orchestrate full implementation from an approved spec — plan, dispatch, verify, commit per task.

Usage: /implement <spec-path>

Follow the full protocol in `.claude/skills/implement.md`. Summary:

1. **Validate** — Read the spec, confirm status is `Approved`. Read parent system spec for binding decisions. Stop if not approved.
2. **Create Plan** — Derive plan path (same dir, `.plan.md`). Build task table from spec's task list with Model column (haiku/sonnet/opus per task complexity). Skip if plan already exists (resume).
3. **Execute Loop** — For each pending task:
   - Dispatch to subagent per `.claude/skills/dispatch.md` (inline all context, set model)
   - Verify: spec compliance + code quality + fresh `dia run`/`dia pipeline`
   - Commit: stage task files, commit with feature reference, update plan
   - If blocked: diagnose via `.claude/skills/debug.md`, mark BLOCKED, continue
4. **Finalize** — Update plan+spec to Done, run `dia pipeline --target googletest`, Observation Opportunity Scan, report summary.

Rules:
- One task per subagent dispatch
- Never skip verification
- One commit per task
- Never modify the spec during implementation
- Resume from first Pending task if plan exists
