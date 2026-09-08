"""PathfindingTestStage — E2E validation: A* + FlowField + DiaSteering full navigation stack integration."""

_STAGE = "PathfindingTestStage"

_CHECKPOINTS = [
    ("pathfinding.path_computed",          0.4),    # fires in OnStart — within ~10 frames
    ("pathfinding.flow_field_ready",       0.4),    # fires in OnStart — within ~10 frames
    ("pathfinding.recomputed_after_block", 2.5),    # fires at frame 60 + margin (~75 frames)
    ("pathfinding.first_agent_arrived",    23.5),   # up to ~700 frames at 30 Hz
    ("pathfinding.all_agents_arrived",     33.5),   # up to ~1000 frames at 30 Hz
]


def test_pathfinding_stage(dia_client):
    """All 5 pathfinding checkpoints pass: path computed, flow field ready, reroute after dynamic obstacle, all 3 agents arrive."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp}: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
