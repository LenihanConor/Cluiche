"""Human handoff — print clear status when agent stops."""
import json
from pathlib import Path

from dia_cli.utils.repo_root import find_repo_root
from .loop import AgentState, EscalationReason, check_escalation
from .config import AgentConfig


def print_handoff(state: AgentState, config: AgentConfig):
    escalation = check_escalation(state, config)
    if escalation:
        reason = escalation.message
    elif state.last_error:
        reason = state.last_error
    else:
        reason = "All tasks completed"

    is_success = not state.last_error and not escalation

    print()
    print("=" * 60)
    if is_success:
        print("AGENT FINISHED — ALL TASKS COMPLETE")
    else:
        print("AGENT STOPPED — HANDOFF TO HUMAN")
    print("=" * 60)
    print(f"  Reason:     {reason}")
    print(f"  Plan:       {state.plan_path}")
    print(f"  Completed:  {len(state.tasks_completed)} task(s)")

    if state.tasks_completed:
        for tc in state.tasks_completed:
            print(f"              - Task {tc['task']}: {tc['summary']}")

    if not is_success:
        print(f"  Stuck on:   task {state.last_task_done + 1}")

    print(f"  Tokens:     {state.total_tokens:,} ({state.total_input_tokens:,} in, {state.total_output_tokens:,} out)")
    print(f"  Cost:       ${state.estimated_cost:.2f}")
    print(f"  Runtime:    {state.elapsed}")
    print("=" * 60)

    if state.last_error and not is_success:
        error_display = state.last_error[:500]
        print(f"\n  Last error:\n    {error_display}")

    if state.files_modified:
        print(f"\n  Files modified ({len(state.files_modified)}):")
        for f in state.files_modified[:30]:
            print(f"    {f}")
        if len(state.files_modified) > 30:
            print(f"    ... and {len(state.files_modified) - 30} more")

    print(f"\n  To continue from here:")
    print(f"    dia run googletest          # verify current state")
    print(f"    git diff                    # review what was changed")
    print(f"    git checkout .              # discard if bad")
    print(f"    git add -p                  # stage what's good")
    print()

    # Write machine-readable status to build output dir
    status = {
        "success": is_success,
        "reason": reason,
        "plan_path": state.plan_path,
        "tasks_completed": state.tasks_completed,
        "stuck_on": None if is_success else state.last_task_done + 1,
        "files_modified": state.files_modified,
        "tokens": state.total_tokens,
        "cost_dollars": round(state.estimated_cost, 2),
        "elapsed": state.elapsed,
        "last_error": state.last_error if not is_success else None,
    }
    repo_root = find_repo_root(__file__)
    out_dir = repo_root / "Cluiche" / "out" / "DiaCLI" / "agent"
    out_dir.mkdir(parents=True, exist_ok=True)
    status_path = out_dir / "last_run.json"
    status_path.write_text(json.dumps(status, indent=2), encoding="utf-8")
    print(f"  Status written to: {status_path}")
    print()
