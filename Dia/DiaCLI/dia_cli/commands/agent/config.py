"""Agent configuration — guardrails, limits, and tool allowlists."""
from dataclasses import dataclass, field


@dataclass
class AgentConfig:
    model: str = "claude-sonnet-4-6-20250514"
    max_tokens_per_turn: int = 8096
    max_retries_per_task: int = 3
    max_total_tokens: int = 500_000
    cost_ceiling_dollars: float = 5.00
    tokens_without_progress_limit: int = 80_000
    timeout_seconds: int = 14400  # 4 hours

    allowed_dia_commands: list = field(default_factory=lambda: [
        "run googletest",
        "check deps",
        "validate manifest",
        "docs precommit",
    ])

    allowed_path_prefixes: list = field(default_factory=lambda: [
        "Dia/",
        "Cluiche/",
    ])

    forbidden_patterns: list = field(default_factory=lambda: [
        ".git/",
        ".env",
        "credentials",
        "secret",
    ])
