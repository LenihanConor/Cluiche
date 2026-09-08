"""MessageBus Test Stage — E2E: BroadcastRouter fan-out, EntityRouter targeted
delivery, Reaction-pass chaining, and IFlushAdapter injection, all exercised
together with real entities at real frame rate.

Five checkpoints must pass:
  msgbus.first_pulse     — first NetworkPulseEvent delivered to >=1 Receiver
  msgbus.first_ping      — first DirectPingEvent delivered to the correct Receiver
  msgbus.first_pong      — first PongEvent delivered via the Reaction pass
  msgbus.twenty_pulses   — >=20 cumulative NetworkPulseEvent deliveries (sustained broadcast)
  msgbus.ten_pings       — >=10 cumulative DirectPingEvent deliveries (sustained entity routing)

Metrics asserted on completion:
  cluichetest.msgbus.pulses_sent     >= 20
  cluichetest.msgbus.pings_sent      >= 10
  cluichetest.msgbus.pongs_received  >= 1
  cluichetest.msgbus.bursts          == 1   (BurstAdapter fires exactly once, at frame 150)
  cluichetest.msgbus.dropped         == 0   (throughout the run)
"""

_STAGE = "MessageBusTestStage"

# 5 Emitters broadcast NetworkPulseEvent every 2.0s and post DirectPingEvent
# every 3.0s; first_pulse/first_ping/first_pong land within a few seconds,
# twenty_pulses/ten_pings follow once the 5-emitter fan-out accumulates.
_CHECKPOINTS = [
    ("msgbus.first_pulse",    8.0),
    ("msgbus.first_ping",    10.0),
    ("msgbus.first_pong",    12.0),
    ("msgbus.twenty_pulses", 20.0),
    ("msgbus.ten_pings",     20.0),
]


def _run_and_assert(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp} failed: {result['message']}"

        pulses_sent    = dia_client.get_metric("cluichetest.msgbus.pulses_sent")
        pings_sent     = dia_client.get_metric("cluichetest.msgbus.pings_sent")
        pongs_received = dia_client.get_metric("cluichetest.msgbus.pongs_received")
        bursts         = dia_client.get_metric("cluichetest.msgbus.bursts")
        dropped        = dia_client.get_metric("cluichetest.msgbus.dropped")

        assert pulses_sent    >= 20, f"Expected >=20 pulses sent, got {pulses_sent}"
        assert pings_sent     >= 10, f"Expected >=10 pings sent, got {pings_sent}"
        assert pongs_received >= 1,  f"Expected >=1 pong received, got {pongs_received}"
        assert bursts         == 1,  f"Expected exactly 1 burst fired, got {bursts}"
        assert dropped        == 0,  f"Expected 0 dropped messages, got {dropped}"

        return pulses_sent, pings_sent
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_msgbus_e2e_all_paths(dia_client):
    """Navigate to MessageBusTestStage and confirm all five checkpoints pass,
    plus the burst-injection and zero-drop metric assertions."""
    _run_and_assert(dia_client)


def test_msgbus_determinism(dia_client):
    """Run the stage twice; pulses_sent/pings_sent must be identical both runs
    (fixed-timestep, pre-computed wander tables, no randomness — AC-C10)."""
    pulses1, pings1 = _run_and_assert(dia_client)
    pulses2, pings2 = _run_and_assert(dia_client)

    assert pulses1 == pulses2, \
        f"Non-deterministic pulses_sent: first={pulses1}, second={pulses2}"
    assert pings1 == pings2, \
        f"Non-deterministic pings_sent: first={pings1}, second={pings2}"
