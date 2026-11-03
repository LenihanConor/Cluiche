"""entity.inspect_request is sim-hosted (Sim-Registered Queries): EntityInspectorModule (SimPU) registers it through
SimQueryHostModule, so the handler runs on SimPU and the reply keeps its pre-migration shape."""
import time

from client import AutomationError


def _inspect(c, index, timeout_s=10.0):
    deadline = time.time() + timeout_s
    last = None
    while time.time() < deadline:
        try:
            return c.send_command("entity.inspect_request", {"index": index})
        except AutomationError as e:
            last = e
            time.sleep(0.2)
    raise AssertionError(f"entity.inspect_request never became available: {last}")


def test_entity_inspect_request_is_answered_on_sim(dia_client):
    c = dia_client
    c.navigate_to("EntityTestStage")
    res = _inspect(c, 0)
    assert isinstance(res, dict), res
    assert res.get("error") is None, res
    assert res, "empty payload for a live entity index"
    c.navigate_to("Boot")


def test_entity_inspect_request_missing_index_returns_empty(dia_client):
    c = dia_client
    c.navigate_to("EntityTestStage")
    _inspect(c, 0)
    res = c.send_command("entity.inspect_request", {})
    assert not res, res
    c.navigate_to("Boot")
