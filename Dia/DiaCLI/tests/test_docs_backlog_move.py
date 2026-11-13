"""Tests for `dia docs backlog move` row matching."""
from __future__ import annotations

from dia_cli.commands.docs.backlog_cmd import find_entry_lines, move_to_history, strike_row

BACKLOG = """# Cluiche Backlog

| Project | Item | Kind | Deliverable | Spec / plan | Depends On |
|---|---|---|---|---|---|
| CoW | S-1 Terrain | System | Uses A4 Weather for climate | x | A4 Weather |
| CoW | A4 Weather | System | Weekly CA | x | S-1 Terrain |
| CoW | GameSpeedController (A2) | System | speed | x | |
| ~~Dia~~ | ~~DiaCellHistory~~ | System | done | | |
| Dia | `dia docs backlog move` matches by substring | struck A4 Weather rows | |
"""


def _lines():
    return BACKLOG.splitlines()


def test_matches_whole_item_cell_only():
    lines = _lines()
    # "A4 Weather" appears in the S-1 row's notes and Depends On: must not match.
    assert [lines[i] for i in find_entry_lines(lines, "A4 Weather")] == [lines[5]]


def test_substring_of_item_does_not_match():
    assert find_entry_lines(_lines(), "Weather") == []
    assert find_entry_lines(_lines(), "S-1") == []


def test_case_and_punctuation_insensitive():
    lines = _lines()
    assert find_entry_lines(lines, "a4-weather") == [5]
    assert find_entry_lines(lines, "gamespeedcontroller") == [6]
    assert find_entry_lines(lines, "GameSpeedController (A2)") == [6]


def test_struck_rows_and_headers_never_match():
    lines = _lines()
    assert find_entry_lines(lines, "DiaCellHistory") == []
    assert find_entry_lines(lines, "Item") == [2]  # header text only matches literally


def test_strike_row_strikes_project_and_item():
    assert strike_row("| CoW | A4 Weather | System | notes |", "A4 Weather") == (
        "| ~~CoW~~ | ~~A4 Weather~~ | System | notes |"
    )


def test_strike_row_first_column_name_strikes_only_first_cell():
    assert strike_row("| DiaCamera3D | [spec](x.md) | notes |", "diacamera3d") == (
        "| ~~DiaCamera3D~~ | [spec](x.md) | notes |"
    )


def test_move_strikes_only_named_row(tmp_path):
    (tmp_path / "docs").mkdir()
    backlog = tmp_path / "docs" / "BACKLOG.md"
    backlog.write_text(BACKLOG, encoding="utf-8")

    ok, msg = move_to_history(tmp_path, "A4 Weather")

    assert ok, msg
    out = backlog.read_text(encoding="utf-8").splitlines()
    assert out[5].startswith("| ~~CoW~~ | ~~A4 Weather~~ |")
    assert out[4] == _lines()[4]  # S-1 row untouched
    assert out[8] == _lines()[8]
    hist = (tmp_path / "docs" / "BACKLOG-HISTORY.md").read_text(encoding="utf-8")
    assert "| CoW | A4 Weather |" in hist


def test_move_refuses_ambiguous_name_without_all(tmp_path):
    (tmp_path / "docs").mkdir()
    backlog = tmp_path / "docs" / "BACKLOG.md"
    text = BACKLOG + "| CoW | A4 Weather | Spec | second table |\n"
    backlog.write_text(text, encoding="utf-8")

    ok, msg = move_to_history(tmp_path, "A4 Weather")
    assert not ok and "--all" in msg
    assert backlog.read_text(encoding="utf-8") == text

    ok, _ = move_to_history(tmp_path, "A4 Weather", allow_multiple=True)
    assert ok
    assert backlog.read_text(encoding="utf-8").count("~~A4 Weather~~") == 2
