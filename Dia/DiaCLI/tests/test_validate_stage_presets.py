"""Tests for .diastage `presets` validation (Stage Catalogue launch-presets, AC 1-2)."""
from __future__ import annotations

import json

from dia_cli.cli.cli_validate import _validate_diastage, _validate_diastage_full

_STAGE = "MyStage"


def _app(extra_modules=None) -> dict:
    mods = [
        {"instance_id": "Local", "type_id": "Local", "stages": [_STAGE]},
        {"instance_id": "Other", "type_id": "Other", "stages": ["OtherStage"]},
        {"instance_id": "Global", "type_id": "Global", "stages": ["all"]},
    ]
    return {"version": 3, "processing_units": [{"instance_id": "SimPU", "modules": mods}]}


def _make(tmp_path, presets, write_app=True):
    stage_dir = tmp_path / "Stages" / _STAGE
    stage_dir.mkdir(parents=True)
    rel = f"Stages/{_STAGE}/app.diaapp"
    if write_app:
        (stage_dir / "app.diaapp").write_text(json.dumps(_app()), encoding="utf-8")
    data = {"name": _STAGE, "manifest": rel, "config": {"path_aliases": {}}}
    if presets is not None:
        data["presets"] = presets
    return data, stage_dir / "my.diastage"


def _run(tmp_path, presets, write_app=True):
    data, path = _make(tmp_path, presets, write_app)
    return _validate_diastage_full(data, repo_root=tmp_path, stage_path=path)[0]


def _p(name="alpine", **extra):
    d = {"name": name, "modules": {"Local": {"seed": 1}}}
    d.update(extra)
    return d


def test_valid(tmp_path):
    assert _run(tmp_path, [_p("a-1", description="d"), _p("b")]) == []


def test_no_presets_unchanged(tmp_path):
    assert _run(tmp_path, None) == []
    assert _validate_diastage({"name": "S", "manifest": "m", "config": {"path_aliases": {}}}) == []


def test_not_a_list(tmp_path):
    assert any("presets should be list" in e for e in _run(tmp_path, {}))


def test_seventeen_presets(tmp_path):
    errors = _run(tmp_path, [_p(f"p{i}") for i in range(17)])
    assert any("17 entries" in e and "16" in e for e in errors)


def test_sixteen_ok(tmp_path):
    assert _run(tmp_path, [_p(f"p{i}") for i in range(16)]) == []


def test_duplicate_name(tmp_path):
    errors = _run(tmp_path, [_p("a"), _p("a")])
    assert any("presets[1]" in e and "duplicate" in e for e in errors)


def test_bad_kebab_name(tmp_path):
    for bad in ("Alpine", "a_b", "-a", "a--b", ""):
        assert any("kebab" in e for e in _run(tmp_path / bad.replace("-", "x") if False else _fresh(tmp_path, bad), [_p(bad)])), bad


def _fresh(tmp_path, tag):
    d = tmp_path / f"t{abs(hash(tag))}"
    d.mkdir()
    return d


def test_name_too_long(tmp_path):
    assert any("kebab" in e for e in _run(tmp_path, [_p("a" * 32)]))
    assert not any("kebab" in e for e in _fresh_run(tmp_path))


def _fresh_run(tmp_path):
    return _run(_fresh(tmp_path, "ok31"), [_p("a" * 31)])


def test_missing_name(tmp_path):
    errors = _run(tmp_path, [{"modules": {}}])
    assert any("presets[0].name" in e for e in errors)


def test_description_too_long(tmp_path):
    errors = _run(tmp_path, [_p(description="x" * 121)])
    assert any("description" in e and "120" in e for e in errors)


def test_description_120_ok(tmp_path):
    assert _run(tmp_path, [_p(description="x" * 120)]) == []


def test_modules_not_object(tmp_path):
    errors = _run(tmp_path, [{"name": "a", "modules": []}])
    assert any("modules" in e and "object" in e for e in errors)


def test_module_value_not_object(tmp_path):
    errors = _run(tmp_path, [{"name": "a", "modules": {"Local": 5}}])
    assert any("Local" in e and "object" in e for e in errors)


def test_undeclared_instance(tmp_path):
    errors = _run(tmp_path, [{"name": "a", "modules": {"Nope": {}}}])
    assert any("Nope" in e and "not a module instance declared" in e for e in errors)


def test_all_scoped_instance(tmp_path):
    errors = _run(tmp_path, [{"name": "a", "modules": {"Global": {}}}])
    assert any("Global" in e and "all-scoped" in e and "not restarted" in e for e in errors)


def test_other_stage_scoped_instance(tmp_path):
    errors = _run(tmp_path, [{"name": "a", "modules": {"Other": {}}}])
    assert any("Other" in e and "not scoped" in e for e in errors)


def test_over_2kb(tmp_path):
    errors = _run(tmp_path, [{"name": "a", "modules": {"Local": {"blob": "x" * 2100}}}])
    assert any("presets[0]" in e and "2048" in e for e in errors)


def test_unresolvable_app_errors_when_modules_present(tmp_path):
    errors = _run(tmp_path, [_p()], write_app=False)
    assert any("cannot resolve" in e for e in errors)


def test_unresolvable_app_ok_when_no_modules(tmp_path):
    assert _run(tmp_path, [{"name": "a", "modules": {}}], write_app=False) == []
