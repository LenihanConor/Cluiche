"""Tests for the optional module-declaration field `requires_tick_after` (manifest module order)."""
import copy
import json
from pathlib import Path
from unittest.mock import patch

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli, _validate_diaapp


def _mod(instance_id, **extra):
    m = {"instance_id": instance_id, "type_id": instance_id, "stages": [], "dependencies": [], "channels": []}
    m.update(extra)
    return m


def _app(*pus):
    """pus: (pu_id, [module dicts])."""
    return {
        "version": 3,
        "processing_units": [
            {"instance_id": pid, "frequency_hz": 30, "dedicated_thread": False, "modules": mods}
            for pid, mods in pus
        ],
    }


def _errors(data):
    return [e for e in _validate_diaapp(data) if "requires_tick_after" in e]


def test_valid_order_passes():
    app = _app(("Sim", [_mod("A"), _mod("B", requires_tick_after=["A"])]))
    assert _validate_diaapp(app) == []


def test_absent_field_no_error():
    assert _validate_diaapp(_app(("Sim", [_mod("B"), _mod("A")]))) == []


def test_referenced_module_later_is_error():
    app = _app(("Sim", [_mod("B", requires_tick_after=["A"]), _mod("A")]))
    errs = _errors(app)
    assert len(errs) == 1
    assert "Sim" in errs[0] and "'B'" in errs[0] and "'A'" in errs[0]
    assert "index 0" in errs[0] and "index 1" in errs[0]


def test_referenced_module_missing_is_error():
    app = _app(("Sim", [_mod("B", requires_tick_after=["Nope"])]))
    errs = _errors(app)
    assert len(errs) == 1 and "'Nope'" in errs[0] and "not found" in errs[0]


def test_reference_to_other_pu_is_error():
    app = _app(("Main", [_mod("A")]), ("Sim", [_mod("B", requires_tick_after=["A"])]))
    errs = _errors(app)
    assert len(errs) == 1 and "'A'" in errs[0] and "Sim" in errs[0]


def test_self_reference_is_error():
    app = _app(("Sim", [_mod("A", requires_tick_after=["A"])]))
    assert len(_errors(app)) == 1


def test_cycle_is_error():
    app = _app(("Sim", [_mod("A", requires_tick_after=["B"]), _mod("B", requires_tick_after=["A"])]))
    assert len(_errors(app)) >= 1


def test_non_array_value_is_error():
    app = _app(("Sim", [_mod("A"), _mod("B", requires_tick_after="A")]))
    errs = _errors(app)
    assert len(errs) == 1 and "array" in errs[0]


def test_non_string_element_is_error():
    app = _app(("Sim", [_mod("A"), _mod("B", requires_tick_after=[1])]))
    assert len(_errors(app)) == 1


def test_multiple_constraints_one_module():
    ok = _app(("Sim", [_mod("A"), _mod("B"), _mod("C", requires_tick_after=["A", "B"])]))
    assert _validate_diaapp(ok) == []
    bad = _app(("Sim", [_mod("A"), _mod("C", requires_tick_after=["A", "B"]), _mod("B")]))
    errs = _errors(bad)
    assert len(errs) == 1 and "'B'" in errs[0]


def test_cli_fails_swapped_copy_of_real_manifest(tmp_path):
    """Negative check: the real DeferredStep stage manifest with two modules swapped must fail."""
    real = Path(__file__).resolve().parents[3] / "Cluiche" / "Assets" / "CluicheTest" / "Stages" / \
        "DeferredStepTestStage" / "misc" / "ApplicationFlow" / "deferred_step_test_stage.diaapp"
    data = json.loads(real.read_text(encoding="utf-8"))
    assert _validate_diaapp(data) == []
    swapped = copy.deepcopy(data)
    mods = swapped["processing_units"][1]["modules"]
    mods[0], mods[1] = mods[1], mods[0]
    p = tmp_path / "swapped.diaapp"
    p.write_text(json.dumps(swapped), encoding="utf-8")
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        result = CliRunner().invoke(validate_cli, ["manifest", "--path", str(p)])
    assert result.exit_code == 1
    assert "requires_tick_after" in result.output


# ── across manifests composed by a .diagame ─────────────────────────────────────────────────────────────────────────

def _write_game(tmp_path, *apps):
    """apps: one v3 app dict per import, imported in order."""
    imports = []
    for i, app in enumerate(apps):
        name = f"m{i}.diaapp"
        (tmp_path / name).write_text(json.dumps(app), encoding="utf-8")
        imports.append({"path": name, "type": "manifest"})
    return {"name": "g", "version": 3, "config": {"name": "g", "window": {"title": "g"}}, "imports": imports}


def _game_errors(game, tmp_path):
    from dia_cli.cli.cli_validate import _validate_diagame
    errors, _ = _validate_diagame(game, tmp_path / "g.diagame")
    return [e for e in errors if "requires_tick_after" in e]


def test_diagame_cross_manifest_order_satisfied_passes(tmp_path):
    game = _write_game(tmp_path,
                       _app(("Sim", [_mod("A")])),
                       _app(("Sim", [_mod("B", requires_tick_after=["A"])])))
    # Each manifest alone would fail (A is not in the second one); the composed order is what counts, so only
    # assert the composed check itself raises nothing.
    from dia_cli.cli.cli_validate import _compose_game_manifest, _check_requires_tick_after
    composed, _ = _compose_game_manifest(game, tmp_path / "g.diagame")
    assert [e for pu in composed["processing_units"] for e in _check_requires_tick_after(pu)] == []


def test_diagame_cross_manifest_order_violated_is_error(tmp_path):
    game = _write_game(tmp_path,
                       _app(("Sim", [_mod("B", requires_tick_after=["A"])])),
                       _app(("Sim", [_mod("A")])))
    errs = _game_errors(game, tmp_path)
    assert any("'B'" in e and "'A'" in e and "REQUIRES_TICK_AFTER_ORDER" in e and "composed g.diagame" in e for e in errs)


def test_diagame_cross_manifest_missing_reference_is_error(tmp_path):
    game = _write_game(tmp_path, _app(("Sim", [_mod("B", requires_tick_after=["Nope"])])))
    errs = _game_errors(game, tmp_path)
    assert any("'Nope'" in e and "REQUIRES_TICK_AFTER_MISSING" in e for e in errs)


# ── World Step F2 AC 3: WorldStepModule -> DeferredStepModule -> consumers ──────────────────────────────────────────

def _worldstep_chain(order):
    """Modules keyed by name, each with the requires_tick_after the CoW plan (F2 task 4) prescribes."""
    decl = {
        "WorldStepModule": _mod("WorldStepModule"),
        "DeferredStepModule": _mod("DeferredStepModule", requires_tick_after=["WorldStepModule"]),
        "WeatherModule": _mod("WeatherModule", requires_tick_after=["DeferredStepModule"]),
    }
    return _app(("Sim", [decl[n] for n in order]))


def test_worldstep_chain_correct_order_passes():
    assert _validate_diaapp(_worldstep_chain(["WorldStepModule", "DeferredStepModule", "WeatherModule"])) == []


def test_worldstep_consumer_before_deferredstep_names_both():
    errs = _errors(_worldstep_chain(["WorldStepModule", "WeatherModule", "DeferredStepModule"]))
    assert len(errs) == 1 and "'WeatherModule'" in errs[0] and "'DeferredStepModule'" in errs[0]


def test_worldstep_deferredstep_before_worldstep_names_both():
    errs = _errors(_worldstep_chain(["DeferredStepModule", "WorldStepModule", "WeatherModule"]))
    assert len(errs) == 1 and "'DeferredStepModule'" in errs[0] and "'WorldStepModule'" in errs[0]
