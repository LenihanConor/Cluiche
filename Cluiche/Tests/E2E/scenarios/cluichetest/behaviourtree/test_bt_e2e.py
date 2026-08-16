"""BehaviourTree Test Stage — E2E: shared BT asset, 3 independent guards, patrol/alert/chase loop.

Five checkpoints must pass:
  bt.patrol_started         — first OnUpdate frame, guards begin patrolling
  bt.chase_triggered        — any guard's has_target goes true
  bt.parallel_fired         — parallel node (move_to_target + set_alert_effect) activates
  bt.multi_state_divergence — guards are in at least 2 different BT branches
  bt.target_lost            — has_target resets after wanderer leaves detection range

Metrics asserted on completion:
  cluichetest.bt.total_chases       >= 1
  cluichetest.bt.decorator_cycles   >= 1   (patrol repeater cycled at least once)
  cluichetest.bt.total_frames       >= 100
"""

_STAGE = "BehaviourTreeTestStage"

# Wanderer starts at (6, 0); Guard 1 starts at (4, 0) — distance 2.0 < detection range 5.0.
# Checkpoints ordered by expected fire time (all within the 600-frame / 20s budget).
_CHECKPOINTS = [
    ("bt.patrol_started",          5.0),
    ("bt.chase_triggered",        10.0),
    ("bt.parallel_fired",         12.0),
    ("bt.multi_state_divergence", 12.0),
    ("bt.target_lost",            20.0),
]


def test_bt_guard_patrol_chase_loop(dia_client):
    """Navigate to BehaviourTreeTestStage and confirm all five checkpoints pass."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp} failed: {result['message']}"

        total_chases    = dia_client.get_metric("cluichetest.bt.total_chases")
        decorator_cycles = dia_client.get_metric("cluichetest.bt.decorator_cycles")
        total_frames    = dia_client.get_metric("cluichetest.bt.total_frames")

        assert total_chases >= 1,    f"Expected >=1 chase, got {total_chases}"
        assert decorator_cycles >= 1, f"Expected >=1 decorator cycle, got {decorator_cycles}"
        assert total_frames >= 100,  f"Expected >=100 frames, got {total_frames}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_bt_determinism(dia_client):
    """Run the stage twice; total_frames must be identical both runs."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"First run — {cp} failed: {result['message']}"
        frames1 = dia_client.get_metric("cluichetest.bt.total_frames")
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"Second run — {cp} failed: {result['message']}"
        frames2 = dia_client.get_metric("cluichetest.bt.total_frames")
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    assert frames1 == frames2, \
        f"Non-deterministic frame count: first={frames1}, second={frames2}"
