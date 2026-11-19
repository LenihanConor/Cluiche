"""Tests for .diastage `proves` validation (Stage Catalogue proves-traceability)."""
from __future__ import annotations

from dia_cli.cli.cli_validate import _validate_diastage, _validate_diastage_full

_GOOD = "docs/specs/applications/dia/systems/x/y.md"


def _stage(**extra) -> dict:
    d = {"name": "S", "manifest": "m.diaapp", "config": {"path_aliases": {}}}
    d.update(extra)
    return d


def _run(tmp_path, proves):
    return _validate_diastage_full(_stage(proves=proves), repo_root=tmp_path)


def _make(tmp_path, rel=_GOOD):
    p = tmp_path / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text("x", encoding="utf-8")


def test_absent_proves_ok():
    assert _validate_diastage(_stage()) == []


def test_valid_existing_no_diagnostics(tmp_path):
    _make(tmp_path)
    assert _run(tmp_path, [_GOOD]) == ([], [])


def test_empty_list_ok(tmp_path):
    assert _run(tmp_path, []) == ([], [])


def test_not_a_list(tmp_path):
    errors, _ = _run(tmp_path, "docs/specs/a.md")
    assert any("proves should be list" in e for e in errors)


def test_non_string_entry(tmp_path):
    errors, _ = _run(tmp_path, [5])
    assert any("proves[0] should be str" in e for e in errors)


def test_too_many_entries(tmp_path):
    entries = [f"docs/specs/a{i}.md" for i in range(17)]
    errors, _ = _run(tmp_path, entries)
    assert any("17 entries" in e and "16" in e for e in errors)


def test_sixteen_entries_ok(tmp_path):
    entries = [f"docs/specs/a{i}.md" for i in range(16)]
    errors, _ = _run(tmp_path, entries)
    assert errors == []


def test_duplicate(tmp_path):
    _make(tmp_path)
    errors, _ = _run(tmp_path, [_GOOD, _GOOD])
    assert any("proves[1]" in e and "duplicate" in e for e in errors)


def test_missing_prefix(tmp_path):
    errors, _ = _run(tmp_path, ["specs/a.md"])
    assert any("proves[0]" in e and "docs/specs/" in e for e in errors)


def test_missing_suffix(tmp_path):
    errors, _ = _run(tmp_path, ["docs/specs/a.txt"])
    assert any("proves[0]" in e and ".md" in e for e in errors)


def test_backslash(tmp_path):
    errors, _ = _run(tmp_path, ["docs\\specs\\a.md"])
    assert any("proves[0]" in e and "backslash" in e for e in errors)


def test_missing_file_is_warning_only(tmp_path):
    errors, warnings = _run(tmp_path, [_GOOD])
    assert errors == []
    assert len(warnings) == 1 and "proves[0]" in warnings[0] and _GOOD in warnings[0]


def test_shape_error_gives_no_existence_warning(tmp_path):
    errors, warnings = _run(tmp_path, ["bad.md"])
    assert errors and warnings == []


def test_validate_diastage_unchanged_signature_returns_errors_only():
    assert _validate_diastage(_stage(proves="x")) != []
