"""RigidBody2D Test Stage — validate the full physics integration scene.

The stage drops circles + a thrown body onto a polygon ground, swings a
distance-joint pendulum, fires collision/trigger events and runs a raycast.
It reports four checkpoints; all must pass for the stage to be considered good.
"""

_CHECKPOINTS = [
    "test.rigid_body.all_settled",
    "test.rigid_body.collision_events",
    "test.rigid_body.trigger_overlap",
    "test.rigid_body.raycast_hit",
]


def test_rigidbody2d_full_scene(dia_client):
    """Navigate to the stage and wait for every checkpoint to pass."""
    dia_client.navigate_to("RigidBody2DTestStage")
    try:
        for cp in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=15.0)
            assert result["passed"], f"{cp} failed: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_rigidbody2d_determinism(dia_client):
    """Run the stage twice; the settle message must be identical both runs."""
    cp = "test.rigid_body.all_settled"

    dia_client.navigate_to("RigidBody2DTestStage")
    try:
        result1 = dia_client.poll_checkpoint(cp, timeout_s=15.0)
        assert result1["passed"], f"First run failed: {result1['message']}"
        msg1 = result1["message"]
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    dia_client.navigate_to("RigidBody2DTestStage")
    try:
        result2 = dia_client.poll_checkpoint(cp, timeout_s=15.0)
        assert result2["passed"], f"Second run failed: {result2['message']}"
        msg2 = result2["message"]
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    assert msg1 == msg2, f"Settle results differ: '{msg1}' vs '{msg2}'"
