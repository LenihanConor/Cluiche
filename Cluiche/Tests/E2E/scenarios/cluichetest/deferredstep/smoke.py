import time

_STAGE = "DeferredStepTestStage"
_CHECKPOINT = "test.deferred_step.passed"
_CHANNEL = "DeferredStepSynthetic"


def _step_channels(dia_client):
    window = dia_client.send_command("perf.window", {"seconds": 5})
    return {s["channel"] for s in window.get("jobs", {}).get("steps", [])}


def test_deferredstep_stage(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint(_CHECKPOINT, timeout_s=60.0, expected_stage=_STAGE)
        assert result["passed"], f"{_CHECKPOINT}: {result['message']}"

        # Step summaries reach Main via $perfjobs and the channel name via $perfnames, so the channel is named, not hex.
        channels = set()
        deadline = time.time() + 15.0
        while time.time() < deadline:
            channels = _step_channels(dia_client)
            if _CHANNEL in channels:
                break
            time.sleep(0.25)
        assert _CHANNEL in channels, f"perf.window jobs.steps channels: {sorted(channels)}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
