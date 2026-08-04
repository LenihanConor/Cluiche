"""EntityTestStage smoke: navigate to stage, wait for all 6 checkpoints, verify metrics."""


def test_entity_test_stage(dia_client):
    dia_client.navigate_to("EntityTestStage")

    result = dia_client.poll_checkpoint("entity.spawn_complete", timeout_s=10.0)
    assert result["passed"], f"spawn_complete failed: {result['message']}"

    result = dia_client.poll_checkpoint("entity.hierarchy_valid", timeout_s=10.0)
    assert result["passed"], f"hierarchy_valid failed: {result['message']}"

    result = dia_client.poll_checkpoint("entity.destroy_cascade", timeout_s=10.0)
    assert result["passed"], f"destroy_cascade failed: {result['message']}"

    result = dia_client.poll_checkpoint("entity.query_correct", timeout_s=10.0)
    assert result["passed"], f"query_correct failed: {result['message']}"

    result = dia_client.poll_checkpoint("entity.lifecycle_complete", timeout_s=10.0)
    assert result["passed"], f"lifecycle_complete failed: {result['message']}"

    result = dia_client.poll_checkpoint("entity.mailbox_received", timeout_s=15.0)
    assert result["passed"], f"mailbox_received failed: {result['message']}"

    alive_count = dia_client.get_metric("cluichetest.entity.alive_count")
    assert alive_count == 8, f"Expected 8 alive entities after destroy, got {alive_count}"

    mailbox_count = dia_client.get_metric("cluichetest.entity.mailbox_count")
    assert mailbox_count > 0, f"Expected mailbox messages received, got {mailbox_count}"

    dia_client.navigate_to("Boot")
