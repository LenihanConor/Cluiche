"""The reflect stage only regenerates the tracked schema from Debug builds."""
from pathlib import Path
from unittest.mock import MagicMock

from dia_cli.commands.pipeline.stages import reflect_stage


def _config():
    cfg = MagicMock()
    cfg.targets = {"cluichetest": object()}
    return cfg


def test_release_skips_schema_dump(monkeypatch):
    calls = []
    monkeypatch.setattr(reflect_stage, "handle_reflect", lambda **kw: calls.append(kw) or 0)
    rc = reflect_stage.run(_config(), "cluichetest", "Release", False, Path("."))
    assert rc == 0
    assert calls == []


def test_debug_runs_schema_dump(monkeypatch):
    calls = []
    monkeypatch.setattr(reflect_stage, "handle_reflect", lambda **kw: calls.append(kw) or 0)
    rc = reflect_stage.run(_config(), "cluichetest", "Debug", False, Path("."))
    assert rc == 0
    assert len(calls) == 1 and calls[0]["config"] == "Debug"
