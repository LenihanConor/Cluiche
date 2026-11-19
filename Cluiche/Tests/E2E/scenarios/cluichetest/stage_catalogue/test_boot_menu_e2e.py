"""StageCatalogue F4 E2E: the boot menu builds from the catalogue snapshot (AC 15).

The ImGui menu is not queryable, so this asserts what is observable at Boot:
  - the `boot_menu.stage_count` gauge equals the number of unique stages reported by `stages.get` over the manifest stages (stages.list array roots are dropped by the protocol)
    (the menu was built from the snapshot, with StageStatusModule gone);
  - the menu-built log line (when the session log is readable) reports rows consistent with stages.list;
  - the implicit log-error scan (plugin teardown) proves no missing module/channel errors at boot.
Launching from Boot via stages.launch is covered by the CoW stagecatalogue preset scenarios.

Spec: docs/specs/applications/dia/systems/stagecatalogue/boot-menu-view.md
"""
import json
import re
import time
from pathlib import Path

_SESSIONS = Path(__file__).resolve().parents[5] / "Cluiche" / "out" / "CluicheTest" / "sessions"
_GROUP_KINDS = ("showcase", "tool", "test")


def _stage_entries(c):
    """Catalogue entries via stages.get per manifest stage.

    `stages.list` returns a JSON array and the command protocol payload is a proto Struct, so an array root
    arrives as {} over the wire; stages.get (object result) is used instead. Unknown stages (Boot etc.) error.
    """
    from client import AutomationError
    entries = []
    for name in c.list_stages():
        name = name.get("name") if isinstance(name, dict) else name
        try:
            e = c.send_command("stages.get", {"stage": name})
        except AutomationError:
            continue
        if isinstance(e, dict) and e.get("name") == name:
            entries.append(e)
    return entries


def _expected_rows(entries):
    rows = 0
    for e in entries:
        groups = max(1, sum(1 for k in e.get("kinds", []) if k in _GROUP_KINDS))
        rows += groups * (1 + len(e.get("presets", [])))
    return rows


def _wait_gauge(c, expected, timeout_s=30.0):
    deadline = time.time() + timeout_s
    value = None
    while time.time() < deadline:
        value = c.get_metric("boot_menu.stage_count")
        if value == expected:
            return value
        time.sleep(0.5)
    raise AssertionError(f"boot_menu.stage_count={value}, expected {expected}")


def test_boot_menu_stage_count_matches_catalogue(dia_client):
    c = dia_client
    assert c.report().get("stage") == "Boot"
    entries = _stage_entries(c)
    names = {e["name"] for e in entries}
    assert names, "no catalogue stages found"
    assert len(names) == len(entries), "duplicate names"
    _wait_gauge(c, len(names))


def test_boot_menu_built_log_rows_match_catalogue(dia_client):
    """Best effort: the session log is normally locked while the game runs, so skip when unreadable."""
    import pytest
    c = dia_client
    entries = _stage_entries(c)
    _wait_gauge(c, len(entries))
    sessions = sorted(_SESSIONS.iterdir(), key=lambda p: p.name, reverse=True) if _SESSIONS.exists() else []
    if not sessions or not (sessions[0] / "log.jsonl").exists():
        pytest.skip("no session log")
    try:
        lines = (sessions[0] / "log.jsonl").read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        pytest.skip("session log locked while the game runs")
    pat = re.compile(r"menu built from snapshot \((\d+) stages, (\d+) rows")
    seen = []
    for line in lines:
        try:
            m = pat.search(json.loads(line).get("msg", ""))
        except json.JSONDecodeError:
            continue
        if m:
            seen.append((int(m.group(1)), int(m.group(2))))
    assert seen, "no 'menu built from snapshot' log line"
    assert (len(entries), _expected_rows(entries)) in seen, (seen, len(entries), _expected_rows(entries))
