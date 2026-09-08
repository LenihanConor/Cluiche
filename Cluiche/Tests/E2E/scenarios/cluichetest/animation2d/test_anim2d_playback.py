"""Animation2DTestStage smoke: navigate to stage, verify dragon wing flap sequence and golden pose."""


def test_anim2d_dragon_wing_flap(dia_client):
    dia_client.navigate_to("Animation2DTestStage")

    result = dia_client.poll_checkpoint("animation2d.clips_completed", timeout_s=10.0)
    assert result["passed"], f"clips_completed failed: {result['message']}"

    result = dia_client.poll_checkpoint("animation2d.pose_correct", timeout_s=2.0)
    assert result["passed"], f"pose_correct failed: {result['message']}"

    clips_played = dia_client.get_metric("cluichetest.animation2d.clips_played")
    assert clips_played == 3, f"Expected 3 clips played, got {clips_played}"

    total_frames = dia_client.get_metric("cluichetest.animation2d.total_playback_frames")
    assert total_frames == 180, f"Expected 180 total playback frames, got {total_frames}"

    dia_client.navigate_to("Boot")
