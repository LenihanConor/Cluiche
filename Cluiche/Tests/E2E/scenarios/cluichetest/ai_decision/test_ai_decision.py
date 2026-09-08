"""AIDecisionTestStage — E2E validation: Condition + Rules + UtilityAI (sync/async) + AIBudget."""

_STAGE = "AIDecisionTestStage"

_CHECKPOINTS = [
    ("ai.condition.health_low_passes",    5),
    ("ai.condition.enemy_visible_passes", 5),
    ("ai.rules.call_for_help_fired",      5),
    ("ai.utility.flee_wins",              5),
    ("ai.budget.utility_async_completed", 10),
]


def test_ai_decision(dia_client):
    """All five AI decision checkpoints pass: conditions evaluate, rules fire, utility selects, async completes."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp}: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
