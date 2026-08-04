"""SoftBody2D Test Stage — validate rope and cloth settle under real SimPU timing."""

_ROPE_CHECKPOINT  = "test.soft_body.rope_settled"
_CLOTH_CHECKPOINT = "test.soft_body.cloth_settled"


def test_softbody2d_rope_settles(dia_client):
    """Rope (12 particles, RB anchor) must reach rest within budget."""
    dia_client.navigate_to("SoftBody2DTestStage")
    try:
        result = dia_client.poll_checkpoint(_ROPE_CHECKPOINT, timeout_s=15.0)
        assert result["passed"], f"Rope did not settle: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_softbody2d_cloth_settles(dia_client):
    """Cloth (4x4 grid, corner-pinned) must reach rest within budget."""
    dia_client.navigate_to("SoftBody2DTestStage")
    try:
        result = dia_client.poll_checkpoint(_CLOTH_CHECKPOINT, timeout_s=15.0)
        assert result["passed"], f"Cloth did not settle: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_softbody2d_rope_settles_before_cloth(dia_client, assert_metric):
    """Rope (fewer constraints) must settle before cloth."""
    dia_client.navigate_to("SoftBody2DTestStage")
    try:
        rope_result  = dia_client.poll_checkpoint(_ROPE_CHECKPOINT,  timeout_s=15.0)
        cloth_result = dia_client.poll_checkpoint(_CLOTH_CHECKPOINT, timeout_s=15.0)

        assert rope_result["passed"],  f"Rope did not settle: {rope_result['message']}"
        assert cloth_result["passed"], f"Cloth did not settle: {cloth_result['message']}"

        rope_frame  = assert_metric("cluichetest.softbody.rope_settle_frame_count",  ">", 0)
        cloth_frame = assert_metric("cluichetest.softbody.cloth_settle_frame_count", ">", 0)
        assert rope_frame < cloth_frame, (
            f"Expected rope to settle first, but rope={rope_frame} cloth={cloth_frame}"
        )
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_softbody2d_determinism(dia_client):
    """Two consecutive runs must both pass (deterministic settle)."""
    dia_client.navigate_to("SoftBody2DTestStage")
    try:
        r1_rope  = dia_client.poll_checkpoint(_ROPE_CHECKPOINT,  timeout_s=15.0)
        r1_cloth = dia_client.poll_checkpoint(_CLOTH_CHECKPOINT, timeout_s=15.0)
        assert r1_rope["passed"],  f"Run 1 rope failed: {r1_rope['message']}"
        assert r1_cloth["passed"], f"Run 1 cloth failed: {r1_cloth['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    dia_client.navigate_to("SoftBody2DTestStage")
    try:
        r2_rope  = dia_client.poll_checkpoint(_ROPE_CHECKPOINT,  timeout_s=15.0)
        r2_cloth = dia_client.poll_checkpoint(_CLOTH_CHECKPOINT, timeout_s=15.0)
        assert r2_rope["passed"],  f"Run 2 rope failed: {r2_rope['message']}"
        assert r2_cloth["passed"], f"Run 2 cloth failed: {r2_cloth['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
