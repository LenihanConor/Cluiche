"""Two-entry reload test: load 4 assets, exit, re-enter, verify clean state."""


def test_asset_concurrent_load_and_clean_reload(dia_client):
    # Phase 1: first entry — load 4 assets, verify all_loaded
    dia_client.navigate_to("AssetRuntimeTestStage")
    result = dia_client.poll_checkpoint("test.asset_runtime.all_loaded", timeout_s=5.0)
    assert result["passed"], f"First load failed: {result['message']}"

    load_count = dia_client.get_metric("cluichetest.asset_runtime.load_count")
    assert load_count == 4, f"Expected 4 loaded assets, got {load_count}"

    load_time = dia_client.get_metric("cluichetest.asset_runtime.load_time_ms")
    assert load_time > 0, "load_time_ms should be positive"

    # Return to Boot — triggers DoStop + handle release
    dia_client.navigate_to("Boot")

    # Phase 2: second entry — reload, verify clean_reload
    dia_client.navigate_to("AssetRuntimeTestStage")
    result = dia_client.poll_checkpoint("test.asset_runtime.all_loaded", timeout_s=5.0)
    assert result["passed"], f"Reload failed: {result['message']}"

    result = dia_client.poll_checkpoint("test.asset_runtime.clean_reload", timeout_s=2.0)
    assert result["passed"], f"Clean reload failed: {result['message']}"

    active_handles = dia_client.get_metric("cluichetest.asset_runtime.active_handles")
    assert active_handles == 4, f"Expected 4 active handles, got {active_handles}"

    dia_client.navigate_to("Boot")
