"""Pipeline resume checkpoint: a failed run must not freeze compile-code (source may have changed)."""
from pathlib import Path
from unittest.mock import MagicMock, patch

from dia_cli.commands.pipeline import pipeline_runner as pr


def _run(tmp_path: Path, checkpoint: set, stages: list):
    calls = []

    def handler_for(name):
        def handler(**kwargs):
            calls.append(name)
            return 0
        return handler

    with patch.object(pr, "_load_checkpoint", return_value=set(checkpoint)), \
         patch.object(pr, "_save_checkpoint"), patch.object(pr, "_clear_checkpoint"), \
         patch.object(pr, "_get_handler", side_effect=handler_for):
        rc = pr.run_pipeline(config=MagicMock(), target="t", stages=stages, build_config="Debug",
                             force=False, output=MagicMock(), repo_root=tmp_path)
    return rc, calls


def test_compile_code_runs_even_when_checkpointed(tmp_path):
    rc, calls = _run(tmp_path, {"compile-code"}, ["compile-code", "reflect"])
    assert rc == 0
    assert calls == ["compile-code", "reflect"]


def test_other_checkpointed_stages_still_resume(tmp_path):
    rc, calls = _run(tmp_path, {"compile-code", "reflect", "build-assets"},
                     ["compile-code", "reflect", "build-assets", "deploy"])
    assert rc == 0
    assert calls == ["compile-code", "deploy"]
