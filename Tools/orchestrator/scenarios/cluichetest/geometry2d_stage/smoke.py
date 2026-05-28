"""Two-run determinism test: navigate to Geometry2DTestStage, verify shapes and intersections, return to Boot, repeat."""


def test_geometry2d_stage(dia_client):
    # Phase 1: first run — verify shapes rendered and intersections computed
    dia_client.navigate_to("Geometry2DTestStage")
    result = dia_client.poll_checkpoint("test.geometry2d.passed", timeout_s=5.0)
    assert result["passed"], f"Geometry2D test failed: {result['message']}"

    shape_count = dia_client.get_metric("cluichetest.geometry2d.shape_count")
    assert shape_count > 0, f"Expected at least one shape, got {shape_count}"

    intersection_pair_count = dia_client.get_metric("cluichetest.geometry2d.intersection_pair_count")
    assert intersection_pair_count == 6, f"Expected 6 intersection pairs, got {intersection_pair_count}"

    dia_client.navigate_to("Boot")

    # Phase 2: second run — verify determinism
    dia_client.navigate_to("Geometry2DTestStage")
    result = dia_client.poll_checkpoint("test.geometry2d.passed", timeout_s=5.0)
    assert result["passed"], f"Geometry2D determinism check failed: {result['message']}"

    dia_client.navigate_to("Boot")
