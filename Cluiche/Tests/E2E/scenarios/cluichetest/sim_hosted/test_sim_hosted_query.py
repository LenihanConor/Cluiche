"""Sim-Hosted Queries round trip: Main DebugServer -> SimQueryRequest stream -> SimPU handler -> SimQueryReply -> client.

SimEchoTestModule (CluicheTest, SimPU, all stages) registers the sim-hosted query "sim.echo" which returns
{"echo": <args>, "thread": "sim"}. The registration itself travels over the SimQueryReply stream (reserved requestId 0),
so the first call may need a few frames after boot before the name is known to the Main-side DebugServer.
"""
import time

from client import AutomationError


def _echo(c, params, timeout_s=10.0):
    """Call sim.echo, retrying while the name is not yet registered on Main (falls through to 'unknown command')."""
    deadline = time.time() + timeout_s
    last = None
    while time.time() < deadline:
        try:
            return c.send_command("sim.echo", params)
        except AutomationError as e:
            last = e
            time.sleep(0.2)
    raise AssertionError(f"sim.echo never became available: {last}")


def test_sim_echo_round_trip(dia_client):
    c = dia_client
    res = _echo(c, {"msg": "hello", "n": 7, "nested": {"a": [1, 2, 3]}})
    assert res["thread"] == "sim", res
    assert res["echo"] == {"msg": "hello", "n": 7, "nested": {"a": [1, 2, 3]}}, res


def test_sim_echo_repeated_and_empty_args(dia_client):
    c = dia_client
    _echo(c, {"warm": True})
    for i in range(20):
        res = c.send_command("sim.echo", {"i": i})
        assert res == {"echo": {"i": i}, "thread": "sim"}, (i, res)
    res = c.send_command("sim.echo")
    assert res["thread"] == "sim" and res["echo"] == {}, res


def test_main_hosted_queries_still_work(dia_client):
    """A sim-hosted name must not disturb Main-hosted dispatch."""
    c = dia_client
    _echo(c, {})
    assert c.list_stages(), "dia.manifest.stages returned nothing"
