"""Tests for the composed-module-list check: every declared module `dependencies` entry must be present in the
composed manifest (REQUIRES_DEPENDENCY_MISSING). `soft_dependencies` are never required."""
import json
from pathlib import Path

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli, _validate_diagame

FIXTURES = Path(__file__).resolve().parents[3] / "Cluiche" / "Tests" / "Fixtures" / "WeatherManifest"


def _mod(instance_id, **extra):
    m = {"instance_id": instance_id, "type_id": instance_id, "stages": [], "dependencies": [], "channels": []}
    m.update(extra)
    return m


def _app(*mods):
    return {"version": 3, "processing_units": [
        {"instance_id": "Sim", "frequency_hz": 30, "dedicated_thread": False, "modules": list(mods)}]}


def _game(tmp_path, *apps):
    imports = []
    for i, app in enumerate(apps):
        (tmp_path / f"m{i}.diaapp").write_text(json.dumps(app), encoding="utf-8")
        imports.append({"path": f"m{i}.diaapp", "type": "manifest"})
    return {"name": "g", "version": 3, "config": {"name": "g", "window": {"title": "g"}}, "imports": imports}


def _dep_errors(game, tmp_path):
    errors, _ = _validate_diagame(game, tmp_path / "g.diagame")
    return [e for e in errors if "REQUIRES_DEPENDENCY_MISSING" in e]


def test_all_dependencies_present_passes(tmp_path):
    game = _game(tmp_path, _app(_mod("A"), _mod("B", dependencies=["A"])))
    assert _dep_errors(game, tmp_path) == []


def test_dependency_in_other_imported_manifest_passes(tmp_path):
    game = _game(tmp_path, _app(_mod("B", dependencies=["A"])), _app(_mod("A")))
    assert _dep_errors(game, tmp_path) == []


def test_missing_dependency_names_both_modules(tmp_path):
    game = _game(tmp_path, _app(_mod("B", dependencies=["A"])))
    errs = _dep_errors(game, tmp_path)
    assert len(errs) == 1 and "'B'" in errs[0] and "'A'" in errs[0]


def test_soft_dependencies_never_required(tmp_path):
    game = _game(tmp_path, _app(_mod("B", soft_dependencies=["Nope"])))
    assert _dep_errors(game, tmp_path) == []


# -- A4 WeatherModule AC 1 fixtures ------------------------------------------------------------------------------

def _run(fixture_dir):
    runner = CliRunner()
    return runner.invoke(validate_cli, ["manifest", "--path", str(FIXTURES / fixture_dir / "host.diagame")])


def test_fixture_valid_has_no_errors():
    result = _run("valid")
    assert result.exit_code == 0, result.output


def test_fixture_negative_order_rejected():
    result = _run("negative_order")
    assert result.exit_code == 1 and "REQUIRES_TICK_AFTER_ORDER" in result.output


def test_fixture_negative_missing_dependency_rejected():
    result = _run("negative_missing_dependency")
    assert result.exit_code == 1 and "REQUIRES_DEPENDENCY_MISSING" in result.output
    assert "WeatherModule" in result.output and "WorldGenerationModule" in result.output
