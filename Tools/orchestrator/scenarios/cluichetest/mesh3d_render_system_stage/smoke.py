"""Two-run determinism test: navigate to Mesh3DRenderSystemTestStage, verify MeshRenderer handles
multiple draw commands per frame (including an unknown mesh ID silently skipped), return to Boot, repeat."""


def test_mesh3d_render_system_stage(dia_client):
    # Phase 1: first run — verify 60 frames rendered with all valid draw commands
    dia_client.navigate_to("Mesh3DRenderSystemTestStage")

    result = dia_client.poll_checkpoint("test.mesh3drendersystem.passed", timeout_s=10.0)
    assert result["passed"], f"Mesh3DRenderSystem stage failed: {result['message']}"

    # Verify draw calls metric: Canvas3D only counts GPU submissions, so ≥ 3 (cubes) is the
    # safe floor — avocado adds 1 more once the glTF asset loads, ghost never contributes.
    draw_calls = dia_client.get_metric("dia.render3d.mesh_draw_calls")
    assert draw_calls >= 3, f"Expected ≥ 3 mesh draw calls, got {draw_calls}"

    dia_client.navigate_to("Boot")

    # Phase 2: second run — verify determinism (stage resets and reaches 60 frames again)
    dia_client.navigate_to("Mesh3DRenderSystemTestStage")

    result = dia_client.poll_checkpoint("test.mesh3drendersystem.passed", timeout_s=10.0)
    assert result["passed"], f"Mesh3DRenderSystem determinism check failed: {result['message']}"

    dia_client.navigate_to("Boot")
