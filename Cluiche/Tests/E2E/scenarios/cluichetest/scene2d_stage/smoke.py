"""Two-run determinism test: navigate to Scene2DTestStage, verify scene load pipeline, return to Boot, repeat."""


def test_scene2d_stage(dia_client):
    # Phase 1: first run — verify scene loaded and all subsystems hydrated
    dia_client.navigate_to("Scene2DTestStage")

    result = dia_client.poll_checkpoint("scene.loaded", timeout_s=5.0)
    assert result["passed"], f"Scene load failed: {result['message']}"

    result = dia_client.poll_checkpoint("scene.cameras_hydrated", timeout_s=3.0)
    assert result["passed"], f"Camera hydration failed: {result['message']}"

    result = dia_client.poll_checkpoint("scene.lights_hydrated", timeout_s=3.0)
    assert result["passed"], f"Light hydration failed: {result['message']}"

    result = dia_client.poll_checkpoint("scene.entities_spawned", timeout_s=3.0)
    assert result["passed"], f"Entity spawn failed: {result['message']}"

    result = dia_client.poll_checkpoint("scene.layers_resolved", timeout_s=3.0)
    assert result["passed"], f"Layer resolution failed: {result['message']}"

    entity_count = dia_client.get_metric("cluichetest.scene2d.entity_count")
    assert entity_count == 3, f"Expected 3 entities (1 disabled), got {entity_count}"

    layer_count = dia_client.get_metric("cluichetest.scene2d.layer_count")
    assert layer_count == 3, f"Expected 3 layers, got {layer_count}"

    dia_client.navigate_to("Boot")

    # Phase 2: second run — verify determinism
    dia_client.navigate_to("Scene2DTestStage")
    result = dia_client.poll_checkpoint("scene.loaded", timeout_s=5.0)
    assert result["passed"], f"Scene2D determinism check failed: {result['message']}"

    dia_client.navigate_to("Boot")
