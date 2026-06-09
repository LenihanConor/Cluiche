"""Core agent loop — calls Claude API, executes tools, tracks state."""
import json
import time
from dataclasses import dataclass, field
from pathlib import Path

from anthropic import Anthropic

from .config import AgentConfig
from .tools import TOOL_DEFINITIONS, ToolExecutor, BlockedError


@dataclass
class AgentState:
    plan_path: str = ""
    last_task_done: int = 0
    consecutive_failures: int = 0
    total_input_tokens: int = 0
    total_output_tokens: int = 0
    tokens_since_progress: int = 0
    start_time: float = 0.0
    files_modified: list = field(default_factory=list)
    last_error: str = ""
    tasks_completed: list = field(default_factory=list)

    @property
    def total_tokens(self) -> int:
        return self.total_input_tokens + self.total_output_tokens

    @property
    def estimated_cost(self) -> float:
        # Sonnet 4.6 pricing: $3/M input, $15/M output
        return (self.total_input_tokens * 3 + self.total_output_tokens * 15) / 1_000_000

    @property
    def elapsed(self) -> str:
        seconds = int(time.time() - self.start_time)
        m, s = divmod(seconds, 60)
        h, m = divmod(m, 60)
        if h:
            return f"{h}h {m}m"
        return f"{m}m {s}s"


class EscalationReason:
    def __init__(self, trigger: str, message: str):
        self.trigger = trigger
        self.message = message


def check_escalation(state: AgentState, config: AgentConfig) -> EscalationReason | None:
    if state.consecutive_failures >= config.max_retries_per_task:
        return EscalationReason(
            "max_retries",
            f"{config.max_retries_per_task} consecutive failures on same task",
        )
    if state.estimated_cost >= config.cost_ceiling_dollars:
        return EscalationReason(
            "cost_ceiling",
            f"Hit ${config.cost_ceiling_dollars:.2f} spend ceiling",
        )
    if state.tokens_since_progress > config.tokens_without_progress_limit:
        return EscalationReason(
            "spinning",
            f"{config.tokens_without_progress_limit:,} tokens with no task completed",
        )
    elapsed = time.time() - state.start_time
    if elapsed > config.timeout_seconds:
        return EscalationReason(
            "timeout",
            f"Runtime exceeded {config.timeout_seconds // 3600}h limit",
        )
    if state.total_tokens > config.max_total_tokens:
        return EscalationReason(
            "token_limit",
            f"Exceeded {config.max_total_tokens:,} total token budget",
        )
    return None


SYSTEM_PROMPT = """\
You are a plan executor for the Dia game engine. You implement tasks from a plan file \
one at a time, in order.

WORKFLOW PER TASK:
1. Read the relevant spec constraints and existing code
2. Implement the changes (write/edit files)
3. Run `dia run googletest` to verify (and optionally `dia check deps`)
4. If tests pass, call `task_done` with a summary
5. If tests fail, fix and retry (up to 3 attempts)
6. If stuck after 3 attempts, call `blocked` with the error

RULES:
- Work through tasks sequentially — do not skip ahead
- You may ONLY modify files under Dia/ or Cluiche/
- You may NOT use git, commit, push, or modify .git/
- You may NOT modify .env files, credentials, or secrets
- You may NOT run arbitrary shell commands — only `dia` commands from the allowlist
- Read existing code patterns before writing — match the style
- Use DiaCore containers and patterns (StringCRC, DynamicArrayC, etc.)
- Follow the naming conventions: PascalCase classes, camelCase members with m prefix, k constants

When you complete ALL tasks in the plan, end your response with a final summary \
of what was accomplished.
"""


_FAILURE_MARKERS = [
    "FAILED",          # googletest
    "FAIL:",           # googletest alternate
    "error:",          # MSBuild/compiler
    "Error:",          # MSBuild capitalized
    "fatal error",     # linker
    "LNK2019",         # unresolved external
    "LNK2001",         # unresolved external
    "LNK1120",         # unresolved externals count
    "DENIED:",         # our own tool denial
]


def _looks_like_failure(output: str) -> bool:
    return any(marker in output for marker in _FAILURE_MARKERS)


def build_user_prompt(plan_content: str, spec_content: str) -> str:
    return f"""\
Execute the following plan. Start from task 1 (or the first non-Done task).

## Spec (frozen design contract — constraints you must follow)

{spec_content}

## Plan (your task list)

{plan_content}

Begin with task 1. Read the relevant code first, then implement.
"""


def run_agent_loop(
    plan_path: Path,
    spec_path: Path,
    repo_root: Path,
    config: AgentConfig,
    verbose: bool = False,
) -> AgentState:
    client = Anthropic()
    executor = ToolExecutor(repo_root, config)
    state = AgentState(plan_path=str(plan_path), start_time=time.time())

    plan_content = plan_path.read_text(encoding="utf-8")
    spec_content = spec_path.read_text(encoding="utf-8")

    messages = [
        {"role": "user", "content": build_user_prompt(plan_content, spec_content)}
    ]

    tools_for_api = [
        {"name": t["name"], "description": t["description"], "input_schema": t["input_schema"]}
        for t in TOOL_DEFINITIONS
    ]

    print(f"  Plan:  {plan_path}")
    print(f"  Spec:  {spec_path}")
    print(f"  Model: {config.model}")
    print(f"  Limits: ${config.cost_ceiling_dollars} cost, {config.max_total_tokens:,} tokens, {config.timeout_seconds // 3600}h")
    print()

    try:
        state = _run_loop(client, state, executor, config, messages, tools_for_api, verbose)
    except KeyboardInterrupt:
        state.last_error = "Interrupted by user (Ctrl+C)"
        print("\n\n  Interrupted — preserving progress...")

    state.files_modified = executor.files_modified
    return state


def _run_loop(client, state, executor, config, messages, tools_for_api, verbose):
    while True:
        # Check escalation before each API call
        escalation = check_escalation(state, config)
        if escalation:
            state.last_error = escalation.message
            break

        response = client.messages.create(
            model=config.model,
            max_tokens=config.max_tokens_per_turn,
            system=SYSTEM_PROMPT,
            tools=tools_for_api,
            messages=messages,
        )

        # Track tokens
        state.total_input_tokens += response.usage.input_tokens
        state.total_output_tokens += response.usage.output_tokens
        state.tokens_since_progress += response.usage.output_tokens

        # End of conversation
        if response.stop_reason == "end_turn":
            for block in response.content:
                if hasattr(block, "text") and verbose:
                    print(f"  [agent] {block.text[:200]}")
            break

        # Process tool calls
        tool_results = []
        for block in response.content:
            if block.type == "text" and verbose:
                print(f"  [agent] {block.text[:200]}")
            elif block.type == "tool_use":
                if verbose:
                    print(f"  [tool]  {block.name}({json.dumps(block.input)[:100]})")

                try:
                    result = executor.execute(block.name, block.input)
                except BlockedError as e:
                    state.last_error = e.last_error or e.reason
                    return state

                # Track task_done calls
                if block.name == "task_done":
                    task_num = block.input["task_number"]
                    state.last_task_done = task_num
                    state.consecutive_failures = 0
                    state.tokens_since_progress = 0
                    state.tasks_completed.append({
                        "task": task_num,
                        "summary": block.input["summary"],
                    })
                    print(f"  [done]  Task {task_num}: {block.input['summary']}")

                # Track failures (test runs that fail)
                if block.name == "run_dia" and _looks_like_failure(result):
                    state.consecutive_failures += 1
                    state.last_error = result[-500:]

                tool_results.append({
                    "type": "tool_result",
                    "tool_use_id": block.id,
                    "content": result[:10_000],
                })

        messages.append({"role": "assistant", "content": response.content})
        messages.append({"role": "user", "content": tool_results})

    return state
