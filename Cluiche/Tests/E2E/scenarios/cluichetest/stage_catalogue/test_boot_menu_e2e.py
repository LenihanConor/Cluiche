"""StageCatalogue F4 E2E: the boot menu builds from the catalogue snapshot (AC 15).

The ImGui menu is not queryable, so this asserts what is observable at Boot:
  - the `boot_menu.stage_count` gauge equals the number of unique stages reported by `stages.list`
    (the menu was built from the snapshot, with StageStatusModule gone);
  - the `boot_menu.row_count` gauge equals the grouped row count derived from `stages.list`
    (the session log is read-denied while the game runs, so rows are exposed as a gauge, not read from the log);
  - the implicit log-error scan (plugin teardown) proves no missing module/channel errors at boot.
Launching from Boot via stages.launch is covered by the CoW stagecatalogue preset scenarios.

Spec: docs/specs/applications/dia/systems/stagecatalogue/boot-menu-view.md
"""
import time

_GROUP_KINDS = ("showcase", "tool", "test")


def _stage_entries(c):
    """Catalogue entries from the live `stages.list` query (reply shape {"stages": [...]})."""
    return c.send_command("stages.list").get("stages", [])


def _expected_rows(entries):
    rows = 0
    for e in entries:
        groups = max(1, sum(1 for k in e.get("kinds", []) if k in _GROUP_KINDS))
        rows += groups * (1 + len(e.get("presets", [])))
    return rows


def _wait_gauge(c, expected, timeout_s=30.0, metric="boot_menu.stage_count"):
    deadline = time.time() + timeout_s
    value = None
    while time.time() < deadline:
        value = c.get_metric(metric)
        if value == expected:
            return value
        time.sleep(0.5)
    raise AssertionError(f"{metric}={value}, expected {expected}")


def test_boot_menu_stage_count_matches_catalogue(dia_client):
    c = dia_client
    assert c.report().get("stage") == "Boot"
    entries = _stage_entries(c)
    names = {e["name"] for e in entries}
    assert names, "no catalogue stages found"
    assert len(names) == len(entries), "duplicate names"
    _wait_gauge(c, len(names))


def test_boot_menu_row_count_matches_catalogue(dia_client):
    c = dia_client
    entries = _stage_entries(c)
    _wait_gauge(c, len(entries))
    _wait_gauge(c, _expected_rows(entries), metric="boot_menu.row_count")
