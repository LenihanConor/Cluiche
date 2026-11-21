"""Tests for dia_cli.commands.check.stage_proves (pure core)."""
import json

from dia_cli.commands.check import stage_proves as sp

SPECS = "docs/specs/applications/cow/systems"


def _w(root, rel, text):
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def _spec(root, name, status="Approved", marker="**Testbed:** required", status_section=True, marker_late=False):
    head = f"# {name}\n\n{marker}\n" if marker and not marker_late else f"# {name}\n\n"
    if not status_section:
        head += f"**Status:** {status} (2026-11-18)\n"
    body = "\n## Summary\n\nx\n"
    if marker_late:
        body += f"\n{marker}\n"
    if status_section:
        body += f"\n## Status\n\n`{status}` - note\n"
    rel = f"{SPECS}/{name}/{name}.md"
    _w(root, rel, head + body)
    return rel


def _game(root, app, stages):
    """stages: {stage_name: [proves...]}; all listed in Boot transitions."""
    base = f"Cluiche/Assets/{app}"
    _w(root, f"{base}/{app}.diagame", json.dumps({"imports": [
        {"type": "manifest", "path": "m.diaapp"},
        *[{"type": "stage", "path": f"{n}.diastage"} for n in stages]]}))
    _w(root, f"{base}/m.diaapp", json.dumps({"stages": [{"name": "Boot", "transitions": list(stages)}]}))
    for n, proves in stages.items():
        d = {"name": n}
        if proves is not None:
            d["proves"] = proves
        _w(root, f"{base}/{n}.diastage", json.dumps(d))


def test_broken_link(tmp_path):
    _game(tmp_path, "A", {"S1": ["docs/specs/nope.md"]})
    r = sp.check(tmp_path)
    assert [(b.stage, b.game, b.path) for b in r.broken_links] == [("S1", "A", "docs/specs/nope.md")]


def test_unproven_statuses_listed(tmp_path):
    rels = [_spec(tmp_path, n, s) for n, s in [("a", "Approved"), ("b", "In Progress"), ("c", "Done")]]
    _game(tmp_path, "A", {"S1": []})
    assert sorted(sp.check(tmp_path).unproven) == sorted(rels)


def test_draft_not_flagged(tmp_path):
    _spec(tmp_path, "a", "Draft")
    _game(tmp_path, "A", {"S1": []})
    assert sp.check(tmp_path).unproven == []


def test_non_opt_in_never_flagged(tmp_path):
    _spec(tmp_path, "a", marker=None)
    _game(tmp_path, "A", {"S1": []})
    assert sp.check(tmp_path).unproven == []


def test_marker_parsing(tmp_path):
    assert sp.parse_testbed_required("# T\n\n**Testbed:** Required\n\n## S\n")
    assert sp.parse_testbed_required("# T\n**Testbed:** REQUIRED  \n")
    assert not sp.parse_testbed_required("# T\n\n**Testbed:** optional\n")
    assert not sp.parse_testbed_required("# T\n\nnothing\n")


def test_marker_after_heading_ignored(tmp_path):
    _spec(tmp_path, "a", marker="**Testbed:** required", marker_late=True)
    _game(tmp_path, "A", {"S1": []})
    assert sp.check(tmp_path).unproven == []


def test_status_section_backticks():
    assert sp.parse_status("# T\n**Status:** Draft\n\n## Status\n\n`In Progress` now\n") == "In Progress"
    assert sp.parse_status("# T\n\n## Status\n\n`Done` x\n") == "Done"


def test_status_header_fallback():
    assert sp.parse_status("# T\n**Status:** Approved (2026-11-18)\n\n## Summary\n") == "Approved"
    assert sp.parse_status("# T\n**Status:** `Done`\n") == "Done"
    assert sp.parse_status("# T\n") == "Unknown"


def test_status_header_fallback_used_for_flagging(tmp_path):
    rel = _spec(tmp_path, "a", "Approved", status_section=False)
    _game(tmp_path, "A", {"S1": []})
    assert sp.check(tmp_path).unproven == [rel]


def test_feature_spec_counts_as_coverage(tmp_path):
    rel = _spec(tmp_path, "a", "Approved")
    _game(tmp_path, "A", {"S1": [rel]})
    r = sp.check(tmp_path)
    assert r.unproven == [] and r.coverage == {rel: ["S1"]}


def test_proved_in_other_game(tmp_path):
    rel = _spec(tmp_path, "a")
    _game(tmp_path, "A", {"S1": []})
    _game(tmp_path, "B", {"S2": [rel]})
    r = sp.check(tmp_path)
    assert r.unproven == [] and r.coverage == {rel: ["S2"]}
    assert sp.check(tmp_path, game="A").unproven == []  # coverage spans all games
