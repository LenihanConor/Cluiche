_STAGE = "DeferredStepTestStage"
_CHECKPOINT = "test.deferred_step.passed"


def test_deferredstep_stage(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint(_CHECKPOINT, timeout_s=60.0, expected_stage=_STAGE)
        assert result["passed"], f"{_CHECKPOINT}: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
