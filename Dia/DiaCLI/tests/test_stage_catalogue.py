"""Tests for dia_cli.utils.stage_catalogue (shared fixtures with the C++ loader)."""
import json
from pathlib import Path

import pytest

from dia_cli.utils import stage_catalogue as sc

FIXTURES = Path(__file__).resolve().parents[3] / "Cluiche" / "Tests" / "Fixtures" / "StageCatalogue"
EXPECTED = json.loads((FIXTURES / "expected_catalogue.json").read_text(encoding="utf-8"))


def _load(case):
    path = FIXTURES / case / "game.diagame"
    if case == "missing":
        path = FIXTURES / "missing" / "game.diagame"
    return sc.load(path)


@pytest.mark.parametrize("case", ["basic", "over_cap", "malformed", "no_root_manifest", "missing"])
def test_fixture_matches_expected(case):
    result, cat = _load(case)
    assert sc.to_expected_json(result, cat, summary_only=(case == "over_cap")) == EXPECTED[case]


def test_load_failure_has_no_catalogue():
    result, cat = _load("malformed")
    assert result == sc.MALFORMED
    assert cat is None


def _basic():
    result, cat = _load("basic")
    assert result == sc.OK
    return cat


def test_find():
    cat = _basic()
    assert cat.find("ShowTest") == 2
    assert cat.find("Nope") == -1


def test_filter_all_tags_must_match():
    cat = _basic()
    assert cat.filter(["test", "showcase"]) == [2]
    assert cat.filter(["tool", "world-gen"]) == [3]
    assert cat.filter(["test"]) == [0, 2]


def test_filter_none_and_case():
    cat = _basic()
    assert cat.filter(["test", "tool"]) == []
    assert cat.filter(["TEST"]) == []
    assert cat.filter([]) == list(range(7))


def test_group_by_kind_two_kinds_and_other():
    rows = _basic().group_by_kind()
    assert [r for r in rows if r["entry"] == 2] == [
        {"group": "showcase", "entry": 2}, {"group": "test", "entry": 2}]
    assert {"group": "other", "entry": 4} in rows  # free-form only


def test_manifest_found_by_name_when_source_layout_differs(tmp_path):
    """Source trees may keep the root manifest flat while the .diagame names the deployed path (CoW)."""
    import json, shutil
    from pathlib import Path
    src = Path(__file__).resolve().parents[3] / "Cluiche" / "Tests" / "Fixtures" / "StageCatalogue" / "basic"
    shutil.copytree(src, tmp_path / "g")
    g = tmp_path / "g" / "game.diagame"
    data = json.loads(g.read_text(encoding="utf-8"))
    for imp in data["imports"]:
        if imp["type"] == "manifest":
            imp["path"] = "global/misc/ApplicationFlow/" + imp["path"].split("/")[-1]
    g.write_text(json.dumps(data), encoding="utf-8")
    result, cat = sc.load(g)
    assert result == sc.OK and len(cat.entries) == 7


def test_presets_carry_names_and_descriptions_only():
    result, cat = _load("presets")
    assert result == sc.OK
    with_presets = cat.entries[cat.find("WithPresets")]
    assert with_presets.presets == [{"name": "alpine42", "description": "Alpine, seed 42"},
                                    {"name": "bare", "description": ""}]
    assert cat.entries[cat.find("NoPresets")].presets == []
