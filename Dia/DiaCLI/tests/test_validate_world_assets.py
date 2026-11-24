"""Shipped CoW world data files (guarantees, strata) must validate against their sidecars."""
from __future__ import annotations

import json

from dia_cli.commands.validate.data_rules import build_schema_index, validate_data_document
from dia_cli.utils.repo_root import find_repo_root

WORLD = find_repo_root(__file__) / "Cluiche" / "Assets" / "CoW" / "Global" / "World"


def _load(name: str) -> dict:
    return json.loads((WORLD / name).read_text(encoding="utf-8"))


def test_guarantees_json_validates_against_sidecar():
    index = build_schema_index([_load("guarantees.schema.json")])
    findings = validate_data_document(_load("guarantees.json"), "guarantees.json", index, is_pack=False)
    assert findings == []


def test_guarantees_typo_is_still_caught():
    index = build_schema_index([_load("guarantees.schema.json")])
    doc = _load("guarantees.json")
    doc["site_kinds"][0]["protectd"] = True
    findings = validate_data_document(doc, "guarantees.json", index, is_pack=False)
    assert [(f.rule, f.key) for f in findings] == [("data.no-schema-entry", "site_kinds[].protectd")]


def test_strata_json_validates_against_sidecar():
    index = build_schema_index([_load("strata.schema.json")])
    findings = validate_data_document(_load("strata.json"), "strata.json", index, is_pack=False)
    assert findings == []


def test_strata_typo_is_still_caught():
    index = build_schema_index([_load("strata.schema.json")])
    doc = _load("strata.json")
    doc["sets"][0]["strata"][0]["shar"] = 0.1
    findings = validate_data_document(doc, "strata.json", index, is_pack=False)
    assert [(f.rule, f.key) for f in findings] == [("data.no-schema-entry", "sets[].strata[].shar")]
