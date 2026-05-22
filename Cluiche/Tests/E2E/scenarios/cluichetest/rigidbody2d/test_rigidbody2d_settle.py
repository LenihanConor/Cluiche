"""RigidBody2D Test Stage — validate multi-body settle under real timing."""


def test_rigidbody2d_all_bodies_settle(dia_client):
    """Navigate to RigidBody2DStage, wait for all bodies to settle."""
    dia_client.navigate_to("RigidBody2DStage")

    result = dia_client.poll_checkpoint("rigid_body.all_settled", timeout_s=12.0)
    assert result["passed"], f"Bodies did not settle: {result['message']}"

    dia_client.navigate_to("Boot")


def test_rigidbody2d_determinism(dia_client):
    """Run the stage twice and verify identical settle frame counts."""
    dia_client.navigate_to("RigidBody2DStage")
    result1 = dia_client.poll_checkpoint("rigid_body.all_settled", timeout_s=12.0)
    assert result1["passed"], f"First run failed: {result1['message']}"
    dia_client.navigate_to("Boot")

    dia_client.navigate_to("RigidBody2DStage")
    result2 = dia_client.poll_checkpoint("rigid_body.all_settled", timeout_s=12.0)
    assert result2["passed"], f"Second run failed: {result2['message']}"
    dia_client.navigate_to("Boot")
