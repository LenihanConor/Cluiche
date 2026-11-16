"""dia launch/run pass --dev-alias source=<abs path> to app executables."""
from __future__ import annotations

from pathlib import Path

import pytest
from click.testing import CliRunner

_TOML = """
[global]
default_target = "cluichetest"

[targets.googletest]
project = "Cluiche/GoogleTests/GoogleTests.vcxproj"
stages = ["compile-code"]
exe = "Cluiche/bin/GoogleTests/{config}/x64/GoogleTests.exe"

[targets.cluichetest]
project = "Cluiche/CluicheTest/CluicheTest.vcxproj"
stages = ["compile-code"]
app_name = "CluicheTest"

[targets.cow]
project = "Cluiche/CoW/CoW.vcxproj"
stages = ["compile-code"]
app_name = "CoW"

[targets.cluicheeditor]
project = "Cluiche/CluicheEditor/CluicheEditor.vcxproj"
stages = ["compile-code"]
exe = "Cluiche/bin/CluicheEditor/{config}/x64/CluicheEditor.exe"
"""

_EXES = {
    "googletest": "Cluiche/bin/GoogleTests/Debug/x64/GoogleTests.exe",
    "cluichetest": "Cluiche/bin/CluicheTest/Debug/x64/CluicheTest.exe",
    "cow": "Cluiche/bin/CoW/Debug/x64/CoW.exe",
    "cluicheeditor": "Cluiche/bin/CluicheEditor/Debug/x64/CluicheEditor.exe",
}


@pytest.fixture
def repo(tmp_path, monkeypatch):
    from dia_cli.cli import launch

    (tmp_path / "pipeline.toml").write_text(_TOML, encoding="utf-8")
    for rel in _EXES.values():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("")
    for app in ("CluicheTest", "CoW", "CluicheEditor"):
        (tmp_path / "Cluiche" / "Assets" / app).mkdir(parents=True)
    monkeypatch.setattr(launch, "find_repo_root", lambda _anchor: tmp_path)

    calls = []

    class _Result:
        returncode = 0

    monkeypatch.setattr(launch.subprocess, "run",
                        lambda cmd, cwd=None, **kw: calls.append(cmd) or _Result())
    return tmp_path, calls


def _alias_args(cmd):
    return [a for a in cmd if a == "--dev-alias" or a.startswith("source=")]


def test_resolve_dev_aliases_cluichetest(repo):
    from dia_cli.cli import launch
    root, _ = repo
    expected = root / "Cluiche" / "Assets" / "CluicheTest"
    assert launch._resolve_dev_aliases("cluichetest", root) == ["--dev-alias", f"source={expected}"]


def test_resolve_dev_aliases_missing_dir_returns_empty(repo):
    from dia_cli.cli import launch
    root, _ = repo
    (root / "Cluiche" / "Assets" / "CoW").rmdir()
    assert launch._resolve_dev_aliases("cow", root) == []


@pytest.mark.parametrize("target", ["googletest", "cluicheeditor"])
def test_resolve_dev_aliases_none_without_app_name(repo, target):
    from dia_cli.cli import launch
    root, _ = repo
    assert launch._resolve_dev_aliases(target, root) == []


@pytest.mark.parametrize("target,app", [("cluichetest", "CluicheTest"), ("cow", "CoW")])
def test_launch_passes_dev_alias(repo, target, app):
    from dia_cli.cli import launch
    root, calls = repo
    assert launch.launch_target(target, "Debug") == 0
    cmd = calls[0]
    assert cmd[0] == str(root / _EXES[target])
    assert cmd[1:3] == ["--dev-alias", f"source={root / 'Cluiche' / 'Assets' / app}"]


def test_launch_googletest_has_no_dev_alias(repo):
    from dia_cli.cli import launch
    _, calls = repo
    assert launch.launch_target("googletest", "Debug") == 0
    assert "--dev-alias" not in calls[0]


def test_source_override_replaces_value(repo, tmp_path_factory):
    from dia_cli.cli import launch
    _, calls = repo
    other = tmp_path_factory.mktemp("othersrc")
    assert launch.launch_target("cluichetest", "Debug", source_override=str(other)) == 0
    cmd = calls[0]
    assert cmd[1:3] == ["--dev-alias", f"source={other}"]
    assert cmd.count("--dev-alias") == 1


def test_cli_source_option_overrides(repo, tmp_path_factory):
    from dia_cli.cli import launch
    _, calls = repo
    other = tmp_path_factory.mktemp("othersrc2")
    res = CliRunner().invoke(launch.cli, ["cluichetest", "--source", str(other)])
    assert res.exit_code == 0, res.output
    assert f"source={other}" in calls[0]


def test_cli_source_relative_rejected(repo):
    from dia_cli.cli import launch
    _, calls = repo
    res = CliRunner().invoke(launch.cli, ["cluichetest", "--source", "relative/dir"])
    assert res.exit_code == 2
    assert "ERROR" in res.output
    assert calls == []


def test_cli_source_nonexistent_rejected(repo, tmp_path):
    from dia_cli.cli import launch
    _, calls = repo
    res = CliRunner().invoke(launch.cli, ["cluichetest", "--source", str(tmp_path / "nope")])
    assert res.exit_code == 2
    assert "ERROR" in res.output
    assert calls == []


def test_run_forwards_source(repo, monkeypatch, tmp_path_factory):
    from dia_cli.cli import launch, run
    seen = {}

    def fake(**kw):
        seen.update(kw)
        return 0

    monkeypatch.setattr(launch, "launch_target", fake)
    other = tmp_path_factory.mktemp("othersrc3")
    res = CliRunner().invoke(run.cli, ["cluichetest", "--no-build", "--source", str(other)])
    assert res.exit_code == 0, res.output
    assert seen["source_override"] == str(other)


# ---- --perf[=seconds] forwarding (Perf Inspector A4c) ----

def test_perf_args_translation():
    from dia_cli.cli import launch
    assert launch.perf_args(None) == []
    assert launch.perf_args("") == ["--perf"]
    assert launch.perf_args("30") == ["--perf=30"]


@pytest.mark.parametrize("bad", ["abc", "0", "-5", "1.5"])
def test_perf_args_rejects_bad_values(bad):
    import click
    from dia_cli.cli import launch
    with pytest.raises(click.BadParameter):
        launch.perf_args(bad)


def test_launch_bare_perf_forwarded(repo):
    from dia_cli.cli import launch
    _, calls = repo
    res = CliRunner().invoke(launch.cli, ["cluichetest", "--perf"])
    assert res.exit_code == 0, res.output
    assert "--perf" in calls[0]


def test_launch_perf_seconds_forwarded(repo):
    from dia_cli.cli import launch
    _, calls = repo
    res = CliRunner().invoke(launch.cli, ["cluichetest", "--perf=30"])
    assert res.exit_code == 0, res.output
    assert "--perf=30" in calls[0]
    assert "--perf" not in calls[0]


def test_launch_without_perf_has_no_perf_arg(repo):
    from dia_cli.cli import launch
    _, calls = repo
    res = CliRunner().invoke(launch.cli, ["cluichetest"])
    assert res.exit_code == 0, res.output
    assert not any(a.startswith("--perf") for a in calls[0])


def test_launch_bad_perf_rejected(repo):
    from dia_cli.cli import launch
    _, calls = repo
    res = CliRunner().invoke(launch.cli, ["cluichetest", "--perf=abc"])
    assert res.exit_code == 2
    assert calls == []


@pytest.mark.parametrize("argv,expected", [(["--perf"], ["--perf"]), (["--perf=12"], ["--perf=12"]), ([], [])])
def test_run_forwards_perf(repo, monkeypatch, argv, expected):
    from dia_cli.cli import launch, run
    seen = {}
    monkeypatch.setattr(launch, "launch_target", lambda **kw: seen.update(kw) or 0)
    res = CliRunner().invoke(run.cli, ["cluichetest", "--no-build"] + argv)
    assert res.exit_code == 0, res.output
    assert seen["extra_args"] == expected
