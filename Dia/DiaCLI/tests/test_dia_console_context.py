"""Tests for dia_console/context.py (console-project-context.md).

Runs against the real repo's pipeline.toml -- the repo has exactly one real
project (no CoW sibling, no discovery mechanism), so there is nothing to fake
here that the real file doesn't already exercise deterministically:
diasfml/diauiultralight are real, permanently-hidden internal build-dependency
targets, and googletest/cluichetest/cluicheeditor are real, permanently
user-facing targets. Golden-list style (asserts membership, not a count) so
adding a new target later doesn't break this test.
"""
from __future__ import annotations

from dia_cli.utils.repo_root import find_repo_root
from dia_console.context import TargetInfo, list_targets


def _repo_root():
    return find_repo_root(__file__)


def test_list_targets_excludes_hidden_targets():
    names = {t.name for t in list_targets(_repo_root())}
    assert "diasfml" not in names
    assert "diauiultralight" not in names


def test_list_targets_includes_real_non_hidden_targets():
    names = {t.name for t in list_targets(_repo_root())}
    assert {"googletest", "cluichetest", "cluicheeditor"}.issubset(names)


def test_list_targets_returns_target_info_with_app_name():
    by_name = {t.name: t for t in list_targets(_repo_root())}
    assert isinstance(by_name["cluichetest"], TargetInfo)
    assert by_name["cluichetest"].app_name == "CluicheTest"
