"""MessageBus Stress Test Stage — E2E: 10,000-message stress test for CoW F-1
Test 4 (Bus Live Verification).

Confirms Primary/Reaction two-pass delivery semantics hold under load:
  - Messages posted outside Update() delivered on Primary pass
  - Messages posted from Primary handler reach Reaction subscribers same tick
  - Messages posted from Reaction handler are rejected

Single checkpoint:
  msgbus.stress_complete — 10,000 messages sent and delivered 1:1

Metrics asserted on completion:
  cluichetest.msgbus.pulses_sent + cluichetest.msgbus.pings_sent >= 10000
  cluichetest.msgbus.dropped     == 0
  cluichetest.msgbus.duplicates  == 0

Note: sent == delivered 1:1 verification is done by the stress_complete checkpoint
internally; delivered counts are not exposed as separate metrics.
"""

import pytest

_STAGE = "MessageBusStressTestStage"

# Stress mode uses 0.1s pulse interval and 0.15s ping interval (10x faster than standard).
# With 5 emitters pulsing every 0.1s and pinging every 0.15s, we hit 10k messages in ~120s.
# Add margin for startup and variability.
_STRESS_CHECKPOINT_TIMEOUT = 180.0  # 3 minutes


def _run_and_assert(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint("msgbus.stress_complete", timeout_s=_STRESS_CHECKPOINT_TIMEOUT)
        assert result["passed"], f"msgbus.stress_complete failed: {result['message']}"

        pulses_sent = dia_client.get_metric("cluichetest.msgbus.pulses_sent")
        pings_sent  = dia_client.get_metric("cluichetest.msgbus.pings_sent")
        dropped     = dia_client.get_metric("cluichetest.msgbus.dropped")
        duplicates  = dia_client.get_metric("cluichetest.msgbus.duplicates")

        total_sent = pulses_sent + pings_sent

        assert total_sent >= 10000, f"Expected >=10k messages sent, got {total_sent}"
        assert dropped == 0, f"Expected 0 dropped messages, got {dropped}"
        assert duplicates == 0, f"Expected 0 duplicate messages, got {duplicates}"

        # The stress_complete checkpoint already verified sent == delivered 1:1
        return total_sent
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


@pytest.mark.integration
def test_msgbus_stress_10k_messages(dia_client):
    """Navigate to MessageBusStressTestStage and confirm 10,000 messages are sent
    and delivered at exactly 1:1 ratio with zero drops and zero duplicates."""
    _run_and_assert(dia_client)


@pytest.mark.integration
def test_msgbus_stress_determinism(dia_client):
    """Run the stress stage twice; total sent must be identical both runs
    (fixed-timestep, pre-computed wander tables, no randomness)."""
    sent1 = _run_and_assert(dia_client)
    sent2 = _run_and_assert(dia_client)

    assert sent1 == sent2, \
        f"Non-deterministic sent: first={sent1}, second={sent2}"
