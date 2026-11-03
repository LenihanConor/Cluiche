"""dia.geometry2d.* count queries are sim-hosted (Sim-Registered Queries): the counters are written on SimPU."""
import time

from client import AutomationError


def _call(c, name, timeout_s=10.0):
    deadline = time.time() + timeout_s
    last = None
    while time.time() < deadline:
        try:
            return c.send_command(name, {})
        except AutomationError as e:
            last = e
            time.sleep(0.2)
    raise AssertionError(f"{name} never became available: {last}")


def test_geometry2d_count_queries_answer(dia_client):
    c = dia_client
    c.navigate_to("Geometry2DTestStage")
    shapes = _call(c, "dia.geometry2d.shape_count")
    assert shapes.get("value", 0) > 0, shapes
    pairs = _call(c, "dia.geometry2d.intersection_pairs")
    assert "value" in pairs, pairs
    c.navigate_to("Boot")
