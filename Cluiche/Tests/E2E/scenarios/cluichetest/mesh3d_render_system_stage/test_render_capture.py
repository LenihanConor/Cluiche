"""Navigate to Mesh3DRenderSystemTestStage, verify capture + metrics files are written."""
import time
from pathlib import Path


def test_render_capture_files_written(dia_client, request):
    """Run Mesh3DRenderSystemTestStage and verify DiaRenderTest output files exist."""
    repo_root = request.config._dia_repo_root
    run_dir = Path(repo_root) / "Cluiche" / "out" / "CluicheTest" / "captures" / "run"
    metrics_dir = Path(repo_root) / "Cluiche" / "out" / "CluicheTest" / "captures" / "metrics"

    tag = "Mesh3DRenderSystemTestStage"
    checkpoint = "test.mesh3drendersystem.passed"

    # Send navigate command without waiting on report() polling (the stage loads 3D assets
    # which can cause MainPU to skip DebugServer ticks during transition, making report() slow).
    dia_client.send_command("dia.automation.navigate_to", {"target": tag})
    try:
        # Wait for the stage to complete via checkpoint polling (does not use report())
        result = dia_client.poll_checkpoint(checkpoint, timeout_s=30.0)
        assert result.get("passed"), f"Stage did not pass: {result}"

        # Allow a brief moment for async file writes to complete
        time.sleep(1.5)

        run_png = run_dir / f"{tag}.png"
        assert run_png.exists(), (
            f"Run capture not found at {run_png}. "
            "Check CaptureManager::CompleteCapture wiring."
        )
        assert run_png.stat().st_size > 1000, f"Run PNG too small ({run_png.stat().st_size} bytes)"

        metrics_json = metrics_dir / f"{tag}_metrics.json"
        assert metrics_json.exists(), (
            f"Metrics JSON not found at {metrics_json}. "
            "Check TestStageModuleBase::WriteMetrics wiring."
        )
        assert metrics_json.stat().st_size > 10, "Metrics JSON too small"
    finally:
        dia_client.abort_stage()
        dia_client.send_command("dia.automation.navigate_to", {"target": "Boot"})
