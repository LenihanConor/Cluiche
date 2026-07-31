"""AIHTNTestStage — E2E validation: HTN planner (sync + diverge/replan + async) + AIBudget + RuleActionBridge."""

_STAGE = "AIHTNTestStage"

_CHECKPOINTS = [
    ("ai.htn.plan_built",                   5),
    ("ai.htn.plan_complete",               10),
    ("ai.htn.diverged_and_replanned",      10),
    ("ai.htn.rule_bridge_operator_fired",  10),
    ("ai.htn.async_plan_completed",        10),
    ("ai.htn.async_plan_correct",          10),
]


def test_ai_htn(dia_client):
    """All six HTN checkpoints pass: plan built, executed, diverged+replanned, bridge fired, async plan correct."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp}: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
