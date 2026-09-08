"""AICallout Test Stage — E2E: two-faction wandering-emitter emit/claim/release/TTL lifecycle.

Five checkpoints must pass:
  aicallout.first_emit        — first callout emitted within 6s
  aicallout.first_claim       — first claim succeeds
  aicallout.first_release     — first relay completes; callout returned to unclaimed pool
  aicallout.first_ttl_expiry  — at least one callout expires via TTL
  aicallout.ten_claims        — 10 cumulative claims (sustained coordination cycle)

Metrics asserted on completion:
  cluichetest.aicallout.total_emitted    >= 10
  cluichetest.aicallout.total_claimed    >= 10
  cluichetest.aicallout.total_expired    >= 1
  cluichetest.aicallout.total_released   >= 1
  cluichetest.aicallout.live_count_peak  >= 1
  cluichetest.aicallout.cross_faction_violations == 0
"""

_STAGE = "AICalloutTestStage"

_CHECKPOINTS = [
    ("aicallout.first_emit",       8.0),
    ("aicallout.first_claim",     10.0),
    ("aicallout.first_release",   15.0),
    ("aicallout.first_ttl_expiry", 25.0),
    ("aicallout.ten_claims",       35.0),
]


def test_aicallout_lifecycle(dia_client):
    """Navigate to AICalloutTestStage and confirm all five checkpoints pass."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp} failed: {result['message']}"

        total_emitted    = dia_client.get_metric("cluichetest.aicallout.total_emitted")
        total_claimed    = dia_client.get_metric("cluichetest.aicallout.total_claimed")
        total_expired    = dia_client.get_metric("cluichetest.aicallout.total_expired")
        total_released   = dia_client.get_metric("cluichetest.aicallout.total_released")
        live_count_peak  = dia_client.get_metric("cluichetest.aicallout.live_count_peak")
        cross_faction    = dia_client.get_metric("cluichetest.aicallout.cross_faction_violations")

        assert total_emitted  >= 10, f"Expected >=10 emitted, got {total_emitted}"
        assert total_claimed  >= 10, f"Expected >=10 claimed, got {total_claimed}"
        assert total_expired  >= 1,  f"Expected >=1 expired,  got {total_expired}"
        assert total_released >= 1,  f"Expected >=1 released, got {total_released}"
        assert live_count_peak >= 1, f"Expected >=1 peak live count, got {live_count_peak}"
        assert cross_faction == 0,   f"Expected 0 cross-faction violations, got {cross_faction}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_aicallout_determinism(dia_client):
    """Run the stage twice; total_emitted must be identical both runs (fixed-timestep)."""
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"First run — {cp} failed: {result['message']}"
        emitted1 = dia_client.get_metric("cluichetest.aicallout.total_emitted")
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"Second run — {cp} failed: {result['message']}"
        emitted2 = dia_client.get_metric("cluichetest.aicallout.total_emitted")
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    assert emitted1 == emitted2, \
        f"Non-deterministic: first run emitted {emitted1}, second run emitted {emitted2}"
