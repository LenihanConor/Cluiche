"""UIUltralightTestStage — E2E validation of DiaUIUltralight: page load, JS bridge, round-trip, pixel buffer, mouse injection."""

_STAGE = "UIUltralightTestStage"
_TIMEOUT = 5.0


def test_ui_page_loads(dia_client):
    """Page loads and IsPageLoaded() returns true within budget."""
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint("ui.page_loaded", timeout_s=_TIMEOUT)
        assert result["passed"], f"Page did not load: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_ui_js_callbacks(dia_client):
    """JS window.onload calls OnPageReady(); button click calls OnButtonClicked()."""
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint("ui.js_to_cpp_callback_fired", timeout_s=_TIMEOUT)
        assert result["passed"], f"JS callbacks did not fire: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_ui_round_trip(dia_client):
    """C++ GetTestValue() returns 'dia_test_value_42'; JS echoes it back via ReportReceivedValue."""
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint("ui.round_trip_value_correct", timeout_s=_TIMEOUT)
        assert result["passed"], f"Round-trip value mismatch: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_ui_pixel_buffer(dia_client):
    """FetchUIDataBuffer returns a non-empty buffer with at least one non-zero byte."""
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint("ui.pixel_buffer_non_empty", timeout_s=_TIMEOUT)
        assert result["passed"], f"Pixel buffer empty: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_ui_mouse_injection(dia_client):
    """InjectMouseClick at button coordinates triggers OnButtonClicked in C++."""
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint("ui.mouse_click_handled", timeout_s=_TIMEOUT)
        assert result["passed"], f"Mouse click not handled: {result['message']}"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")


def test_ui_deterministic_reload(dia_client, assert_metric):
    """Two navigation cycles produce identical frames_until_loaded (±1 tolerance)."""
    # Run 1
    dia_client.navigate_to(_STAGE)
    try:
        dia_client.poll_checkpoint("ui.page_loaded", timeout_s=_TIMEOUT)
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")

    # Run 2 — determinism checkpoint passes once module has seen both runs
    dia_client.navigate_to(_STAGE)
    try:
        result = dia_client.poll_checkpoint("ui.deterministic_reload", timeout_s=10.0)
        assert result["passed"], f"Determinism check failed: {result['message']}"
        assert_metric("cluichetest.ui.frames_until_loaded", "<=", 10)
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
