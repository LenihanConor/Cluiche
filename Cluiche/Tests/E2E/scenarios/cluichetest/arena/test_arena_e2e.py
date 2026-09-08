"""Arena Test Stage — multi-system E2E: TriggerScript + Objective + Blackboard +
StateMachine + UtilityAI + Rules across a three-wave arena scenario.

Six checkpoints must pass in sequence:
  arena.wave1_complete      — Count trigger fires after 5 kills
  arena.wave2_complete      — Count trigger fires after 8 kills
  arena.wave3_complete      — Count trigger fires after 10 kills
  arena.victory             — AllPrimaryComplete() true
  arena.powerup_collected   — Spatial trigger fires when player enters zone
  arena.desperate_charge_triggered — DiaRules fires for at least one enemy

Metrics asserted on completion:
  cluichetest.arena.total_kills          == 23
  cluichetest.arena.waves_completed      == 3
  cluichetest.arena.powerup_collected    == 1
  cluichetest.arena.desperate_charges_fired >= 1

Determinism test: second run must produce identical total_kills/waves_completed.
total_frames is diagnostic only — TriggerScript/EnemyAI/Objectives now run
through DiaSimTimeModule's wall-clock budget gate (ST-010: non-deterministic
tick-to-tick under load), so exact frame-count equality is not guaranteed.
"""

_STAGE = "ArenaTestStage"

# Ordered by when they fire in a typical run:
#   powerup_collected       — player walks into zone ~t=5s
#   wave1_complete          — 5 kills, ~t=10s
#   desperate_charge        — enemy health < 0.2 during wave combat
#   wave2_complete          — 8 kills, ~t=17s
#   wave3_complete + victory — 10 kills + AllPrimaryComplete, ~t=20s
# All timeouts are cumulative from stage start; latched checkpoints return instantly.
_CHECKPOINTS = [
    ("arena.powerup_collected",          15.0),
    ("arena.wave1_complete",             20.0),
    ("arena.desperate_charge_triggered", 25.0),
    ("arena.wave2_complete",             25.0),
    ("arena.wave3_complete",             30.0),
    ("arena.victory",                    30.0),
]


def test_arena_three_wave_victory(dia_client):
    """Navigate to ArenaTestStage and confirm all six checkpoints pass."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp} failed: {result['message']}"

        total_kills  = dia_client.get_metric("cluichetest.arena.total_kills")
        waves        = dia_client.get_metric("cluichetest.arena.waves_completed")
        powerup      = dia_client.get_metric("cluichetest.arena.powerup_collected")
        desp_charges = dia_client.get_metric("cluichetest.arena.desperate_charges_fired")

        assert total_kills == 23, f"Expected 23 total kills, got {total_kills}"
        assert waves == 3,        f"Expected 3 waves completed, got {waves}"
        assert powerup == 1,      f"Expected powerup_collected=1, got {powerup}"
        assert desp_charges >= 1, f"Expected >=1 desperate charges, got {desp_charges}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_arena_determinism(dia_client):
    """Run the stage twice; total_kills/waves_completed must be identical both runs."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"First run — {cp} failed: {result['message']}"
        kills1 = dia_client.get_metric("cluichetest.arena.total_kills")
        waves1 = dia_client.get_metric("cluichetest.arena.waves_completed")
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"Second run — {cp} failed: {result['message']}"
        kills2 = dia_client.get_metric("cluichetest.arena.total_kills")
        waves2 = dia_client.get_metric("cluichetest.arena.waves_completed")
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    assert kills1 == kills2, \
        f"Non-deterministic kill count: first={kills1}, second={kills2}"
    assert waves1 == waves2, \
        f"Non-deterministic waves_completed: first={waves1}, second={waves2}"
