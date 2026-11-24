"""Tests for .diastage `tags` / `description` validation (Stage Catalogue F1)."""
from __future__ import annotations

import json
from unittest.mock import patch

import pytest

from dia_cli.cli.cli_validate import _validate_diastage, _stage_kinds


def _stage(**extra) -> dict:
    d = {"name": "S", "manifest": "m.diaapp", "config": {"path_aliases": {}}}
    d.update(extra)
    return d


def _errs(**extra) -> list[str]:
    return _validate_diastage(_stage(**extra))


def test_valid_without_new_fields():
    assert _errs() == []


def test_valid_with_new_fields():
    assert _errs(tags=["tool", "worldgen"], description="Live terrain editor") == []


def test_empty_tags_array_ok():
    assert _errs(tags=[]) == []


def test_vocabulary_matches_json_file():
    assert _stage_kinds() == ["showcase", "tool", "test"]


@pytest.mark.parametrize("tag", ["Worldgen", "has space", "under_score", "", "a" * 64, "-lead", "trail-", "dou--ble"])
def test_bad_kebab_or_length(tag):
    errs = _errs(tags=[tag])
    assert any("tags" in e for e in errs), errs


def test_63_char_tag_ok():
    assert _errs(tags=["a" * 63]) == []


def test_duplicate_tags():
    errs = _errs(tags=["worldgen", "worldgen"])
    assert any("tags" in e and "duplicate" in e for e in errs), errs


def test_seventeen_tags_rejected_sixteen_ok():
    ok = [f"tag-{chr(97 + i)}{chr(97 + i)}{chr(97 + i)}" for i in range(16)]
    assert _errs(tags=ok) == []
    errs = _errs(tags=ok + ["extra-one"])
    assert any("tags" in e and "16" in e for e in errs), errs


@pytest.mark.parametrize("tag,kind", [
    ("tools", "tool"),
    ("Tool", "tool"),
    ("tset", "test"),
    ("tests", "test"),
    ("Showcase", "showcase"),
    ("showcases", "showcase"),
    ("text", "test"),  # distance 1 from 'test': intended spec behaviour
])
def test_near_kind_rejected_naming_kind(tag, kind):
    errs = _errs(tags=[tag])
    assert any(f"tag '{tag}' looks like the kind '{kind}'" in e for e in errs), errs


@pytest.mark.parametrize("tag", ["tool", "test", "showcase", "worldgen", "tooling-x", "terrain", "ai"])
def test_free_form_and_exact_kinds_accepted(tag):
    assert _errs(tags=[tag]) == []


def test_description_120_ok_121_rejected():
    assert _errs(description="x" * 120) == []
    errs = _errs(description="x" * 121)
    assert any("description" in e and "120" in e for e in errs), errs


def test_wrong_types():
    assert any("tags" in e for e in _errs(tags="tool"))
    assert any("tags" in e for e in _errs(tags=[1]))
    assert any("description" in e for e in _errs(description=5))


def test_missing_vocabulary_file_is_clear_error(tmp_path):
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        errs = _errs(tags=["tool"])
    assert any("stage_kinds.json" in e for e in errs), errs
