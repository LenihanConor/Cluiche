_STAGE = "EntitySpatialTestStage"

_CHECKPOINTS = [
    ("entityspatial.index_initialized",  2.0),
    ("entityspatial.circle_query_live",  5.0),
    ("entityspatial.sector_query_live",  5.0),
    ("entityspatial.knearest_exact",     5.0),
    ("entityspatial.layer_mask_filter",  5.0),   # fires just after frame 60
    ("entityspatial.destroy_sweep",      5.0),   # fires at frame 91
    ("entityspatial.ray_query_live",    12.0),
    ("entityspatial.all_queries_stable", 8.0),   # fires at frame 210
]


def test_entityspatial_stage(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp}: {result['message']}"

        active = dia_client.get_metric("cluichetest.entityspatial.active_entity_count")
        assert active == 30, f"Expected 30 active entities after destruction, got {active}"

        knearest = dia_client.get_metric("cluichetest.entityspatial.knearest_count")
        assert knearest == 5, f"QueryKNearest(5) returned {knearest}, expected 5"

        circle_hits = dia_client.get_metric("cluichetest.entityspatial.circle_hit_count")
        assert circle_hits >= 0, "circle_hit_count metric missing"

        sector_hits = dia_client.get_metric("cluichetest.entityspatial.sector_hit_count")
        assert sector_hits >= 0, "sector_hit_count metric missing"

    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
