"""Tests for dia_cli/cli_main.py's command-discovery robustness.

``DiaCLI._find_modules()``/``_generate_sys_path()`` walk the real filesystem
via ``os.walk`` looking for ``cli/``-named directories, using a hardcoded
``IGNORED`` exact-name list (``.venv``, ``dist``, ``node_modules``, ``tests``)
to skip build/dependency trees. A differently-named virtualenv (e.g.
``.venv-x64``) is not in that list, so the walk descends into its
site-packages and picks up unrelated packages' own ``cli/``-shaped
directories (``pip/_internal/cli/``, ``markdown_it/cli/``) as if they were
real ``dia`` commands -- confirmed by an actual session where exactly this
happened and polluted command discovery. ``_is_venv_dir`` fixes this by
detecting a real virtualenv via its ``pyvenv.cfg`` marker file, regardless
of the directory's name.
"""
from __future__ import annotations

from dia_cli import cli_main


def test_is_venv_dir_detects_pyvenv_cfg(tmp_path):
    venv_dir = tmp_path / "myvenv"
    venv_dir.mkdir()
    (venv_dir / "pyvenv.cfg").write_text("home = /usr/bin\n")

    assert cli_main._is_venv_dir(venv_dir) is True


def test_is_venv_dir_false_for_plain_directory(tmp_path):
    plain_dir = tmp_path / "not_a_venv"
    plain_dir.mkdir()

    assert cli_main._is_venv_dir(plain_dir) is False


def test_is_venv_dir_false_for_nonexistent_path(tmp_path):
    assert cli_main._is_venv_dir(tmp_path / "does_not_exist") is False


def test_find_modules_skips_differently_named_venv(monkeypatch, tmp_path):
    """Regression: a venv not literally named '.venv' used to be walked into,
    picking up an unrelated package's own cli/ directory as a fake command."""
    real_cli = tmp_path / "cli"
    real_cli.mkdir()
    (real_cli / "real_command.py").write_text("def cli():\n    pass\n")

    venv_site_cli = tmp_path / "myvenv" / "Lib" / "site-packages" / "somepkg" / "cli"
    venv_site_cli.mkdir(parents=True)
    (tmp_path / "myvenv" / "pyvenv.cfg").write_text("home = /usr/bin\n")
    (venv_site_cli / "spurious.py").write_text("def cli():\n    pass\n")

    monkeypatch.setattr(cli_main, "_root_path", tmp_path)
    dia_cli_instance = cli_main.DiaCLI()

    modules = dia_cli_instance._find_modules()

    assert "real_command" in modules
    assert "spurious" not in modules


def test_generate_sys_path_skips_differently_named_venv(monkeypatch, tmp_path):
    """Same regression as above, for _generate_sys_path's separate walk."""
    real_cli = tmp_path / "realpkg" / "cli"
    real_cli.mkdir(parents=True)

    venv_cli = tmp_path / "myvenv" / "Lib" / "site-packages" / "somepkg" / "cli"
    venv_cli.mkdir(parents=True)
    (tmp_path / "myvenv" / "pyvenv.cfg").write_text("home = /usr/bin\n")

    added_paths: list[str] = []
    monkeypatch.setattr(cli_main.sys, "path", added_paths)
    monkeypatch.setattr(cli_main, "_root_path", tmp_path)

    cli_main._generate_sys_path()

    assert str((tmp_path / "realpkg").resolve()) in added_paths
    assert not any("myvenv" in path for path in added_paths)
