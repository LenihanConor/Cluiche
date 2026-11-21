"""Tests for `dia reflect` change detection (_compute_version)."""
from __future__ import annotations

import copy
import json

from dia_cli.commands.reflect.reflect_handler import _compute_version

BASE = {
    "game": "g",
    "components": [
        {
            "type_id": "C",
            "debug_name": "C",
            "description": "d",
            "fields": [{"name": "speed", "type": "float", "default": 1.0}],
            "default_values": {"speed": 1.0},
        }
    ],
    "modules": [{"type_id": "M", "description": "m", "allowed_pus": ["Sim"]}],
    "processing_units": [],
}


def _write(tmp_path, schema, major=1, minor=0):
    p = tmp_path / "registeredtypes.diaschema"
    p.write_text(json.dumps({**schema, "version": {"major": major, "minor": minor}}), encoding="utf-8")
    return p


def test_identical_content_is_unchanged(tmp_path):
    p = _write(tmp_path, BASE, 2, 3)
    assert _compute_version(p, copy.deepcopy(BASE), is_breaking=False) is None


def test_field_metadata_change_bumps_minor(tmp_path):
    # Regression (2026-10-22): label/unit/min/max/default changes were skipped.
    p = _write(tmp_path, BASE, 2, 3)
    new = copy.deepcopy(BASE)
    new["components"][0]["fields"][0]["unit"] = "m/s"
    assert _compute_version(p, new, is_breaking=False) == {"major": 2, "minor": 4}


def test_default_values_change_bumps_minor(tmp_path):
    p = _write(tmp_path, BASE, 2, 3)
    new = copy.deepcopy(BASE)
    new["components"][0]["default_values"]["speed"] = 2.0
    assert _compute_version(p, new, is_breaking=False) == {"major": 2, "minor": 4}


def test_module_allowed_pus_change_bumps_minor(tmp_path):
    p = _write(tmp_path, BASE, 2, 3)
    new = copy.deepcopy(BASE)
    new["modules"][0]["allowed_pus"] = ["Main"]
    assert _compute_version(p, new, is_breaking=False) == {"major": 2, "minor": 4}


def test_field_removal_still_bumps_major(tmp_path):
    p = _write(tmp_path, BASE, 2, 3)
    new = copy.deepcopy(BASE)
    new["components"][0]["fields"] = []
    assert _compute_version(p, new, is_breaking=False) == {"major": 3, "minor": 0}


def test_missing_file_starts_at_1_0(tmp_path):
    assert _compute_version(tmp_path / "none", BASE, is_breaking=False) == {"major": 1, "minor": 0}
