"""dia launch / dia run --stage / --preset: validated against the game's files, passed through to the exe."""
from __future__ import annotations

import shutil
from pathlib import Path

import pytest
from click.testing import CliRunner

from tests.test_launch_dev_aliases import _EXES, _TOML  # noqa: F401  (shared fixture data)

FIXTURE = Path(__file__).resolve().parents[3] / "Cluiche" / "Tests" / "Fixtures" / "StageCatalogue" / "presets"


@pytest.fixture
def repo(tmp_path, monkeypatch):
    from dia_cli.cli import launch

    (tmp_path / "pipeline.toml").write_text(_TOML, encoding="utf-8")
    for rel in _EXES.values():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("")
    for app, game in (("CluicheTest", "cluichetest"), ("CoW", "cow")):
        dst = tmp_path / "Cluiche" / "Assets" / app
        shutil.copytree(FIXTURE, dst)
        (dst / "game.diagame").rename(dst / f"{game}.diagame")
    monkeypatch.setattr(launch, "find_repo_root", lambda _anchor: tmp_path)
    calls = []

    class _Result:
        returncode = 0

    monkeypatch.setattr(launch.subprocess, "run", lambda cmd, cwd=None, **kw: calls.append(cmd) or _Result())
    return tmp_path, calls


def test_stage_and_preset_passed_through(repo):
    from dia_cli.cli import launch
    _, calls = repo
    assert launch.launch_target("cow", "Debug", stage="WithPresets", preset="alpine42") == 0
    cmd = calls[0]
    i = cmd.index("--stage")
    assert cmd[i:i + 4] == ["--stage", "WithPresets", "--preset", "alpine42"]


def test_stage_without_preset(repo):
    from dia_cli.cli import launch
    _, calls = repo
    assert launch.launch_target("cow", "Debug", stage="NoPresets") == 0
    assert "--stage" in calls[0] and "--preset" not in calls[0]


def test_no_stage_adds_no_args(repo):
    from dia_cli.cli import launch
    _, calls = repo
    assert launch.launch_target("cow", "Debug") == 0
    assert "--stage" not in calls[0]


def test_unknown_stage_lists_valid_names(repo, capsys):
    from dia_cli.cli import launch
    _, calls = repo
    assert launch.launch_target("cow", "Debug", stage="Nope") == 2
    err = capsys.readouterr().err
    assert "Nope" in err and "WithPresets" in err and "NoPresets" in err
    assert calls == []


def test_unknown_preset_lists_valid_names(repo, capsys):
    from dia_cli.cli import launch
    _, calls = repo
    assert launch.launch_target("cow", "Debug", stage="WithPresets", preset="nope") == 2
    err = capsys.readouterr().err
    assert "nope" in err and "alpine42" in err and "bare" in err
    assert calls == []


def test_preset_without_stage_is_error(repo, capsys):
    from dia_cli.cli import launch
    _, calls = repo
    assert launch.launch_target("cow", "Debug", preset="alpine42") == 2
    assert "--stage" in capsys.readouterr().err
    assert calls == []


def test_stage_on_non_app_target_is_error(repo, capsys):
    from dia_cli.cli import launch
    _, calls = repo
    assert launch.launch_target("googletest", "Debug", stage="X") == 2
    assert calls == []


def test_launch_cli_options(repo):
    from dia_cli.cli import launch
    _, calls = repo
    res = CliRunner().invoke(launch.cli, ["cow", "--stage", "WithPresets", "--preset", "bare"])
    assert res.exit_code == 0, res.output
    assert "bare" in calls[0]


def test_run_validates_before_build(repo, monkeypatch):
    from dia_cli.cli import run
    root, _ = repo
    monkeypatch.setattr(run, "find_repo_root", lambda _a: root)
    built = []
    monkeypatch.setattr(run, "_run_pipeline", lambda *a, **k: built.append(1) or 0)
    res = CliRunner().invoke(run.cli, ["cow", "--stage", "Nope"])
    assert res.exit_code == 2
    assert built == []


def test_run_passes_stage_and_preset(repo, monkeypatch):
    from dia_cli.cli import run
    root, calls = repo
    monkeypatch.setattr(run, "_run_pipeline", lambda *a, **k: 0)
    monkeypatch.setattr(run, "find_repo_root", lambda _a: root)
    res = CliRunner().invoke(run.cli, ["cow", "--stage", "WithPresets", "--preset", "alpine42"])
    assert res.exit_code == 0, res.output
    assert "alpine42" in calls[0]
