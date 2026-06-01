"""RigidBody2D Test Stage — validate multi-body settle under real timing."""


_CHECKPOINT = "test.rigid_body.all_settled"


def test_rigidbody2d_all_bodies_settle(dia_client):
    """Navigate to RigidBody2DTestStage, wait for all bodies to settle."""
    dia_client.navigate_to("RigidBody2DTestStage")

    result = dia_client.poll_checkpoint(_CHECKPOINT, timeout_s=12.0)
    assert result["passed"], f"Bodies did not settle: {result['message']}"

    dia_client.navigate_to("Boot")


def test_rigidbody2d_determinism(dia_client):
    """Run the stage twice and verify the settle message is identical (both reached rest)."""
    dia_client.navigate_to("RigidBody2DTestStage")
    result1 = dia_client.poll_checkpoint(_CHECKPOINT, timeout_s=12.0)
    assert result1["passed"], f"First run failed: {result1['message']}"
    msg1 = result1["message"]
    dia_client.navigate_to("Boot")

    dia_client.navigate_to("RigidBody2DTestStage")
    result2 = dia_client.poll_checkpoint(_CHECKPOINT, timeout_s=12.0)
    assert result2["passed"], f"Second run failed: {result2['message']}"
    msg2 = result2["message"]
    dia_client.navigate_to("Boot")

    assert msg1 == msg2, f"Settle results differ: '{msg1}' vs '{msg2}'"
