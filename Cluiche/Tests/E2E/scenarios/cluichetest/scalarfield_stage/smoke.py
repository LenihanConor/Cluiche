_STAGE = "ScalarFieldTestStage"

_CHECKPOINTS = [
    ("scalarfield.fields_initialized",          2.0),
    ("scalarfield.blue_steady_state",           8.0),
    ("scalarfield.red_steady_state",            8.0),
    ("scalarfield.walls_respected",             8.0),
    ("scalarfield.box_write_burst",             5.0),   # fires at frame 60
    ("scalarfield.local_maxima_found",         12.0),
    ("scalarfield.contested_zone_stable",      15.0),
    ("scalarfield.gradient_non_zero_at_probe", 15.0),
]


def test_scalarfield_stage(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp}: {result['message']}"

        blue_peak = dia_client.get_metric("cluichetest.scalarfield.blue_peak")
        assert blue_peak > 0.7, f"Blue peak {blue_peak:.2f} below threshold"

        red_peak = dia_client.get_metric("cluichetest.scalarfield.red_peak")
        assert red_peak > 0.7, f"Red peak {red_peak:.2f} below threshold"

        contested = dia_client.get_metric("cluichetest.scalarfield.contested_cell_count")
        assert contested > 0, "No contested cells found — front line not forming"

    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
