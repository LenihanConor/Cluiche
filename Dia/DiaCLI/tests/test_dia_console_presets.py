"""Tests for dia_console/presets.py and dia_cli/cli/preset.py (console-presets.md).

File-IO tests (`load_presets`/`save_preset`/`delete_preset`) run against
`tmp_path`, never the real repo's `.dia/` directory. `build_execute_request`
is pure. The `dia preset run` tests spawn a real subprocess of a deterministic
fake `dia` stand-in script (same pattern as test_dia_console_execution.py's
`fake_service` fixture) so the merge-by-id -> resolve -> argv -> subprocess
pipeline is exercised for real, without needing the real DiaCLI tree or a
real build.
"""
from __future__ import annotations

import sys
import textwrap

import click
import pytest

from dia_console.model import (
    ArgumentDescriptor,
    ArgumentType,
    CommandDescriptor,
    OptionDescriptor,
)
from dia_console.presets import (
    PresetDescriptor,
    build_execute_request,
    delete_preset,
    load_presets,
    save_preset,
)
from dia_console.registry import CommandRegistry


# ===========================================================================
# load_presets
# ===========================================================================

def test_load_presets_missing_files_returns_empty_list(tmp_path):
    assert load_presets(tmp_path) == []


def test_load_presets_reads_shared_file_only(tmp_path):
    (tmp_path / ".dia").mkdir()
    (tmp_path / ".dia" / "console-presets.yaml").write_text(
        textwrap.dedent("""\
            presets:
              - id: smoke-e2e
                name: E2E Smoke
                command: test.e2e
                values:
                  suite: smoke
        """),
        encoding="utf-8",
    )
    presets = load_presets(tmp_path)
    assert len(presets) == 1
    preset = presets[0]
    assert preset == PresetDescriptor(
        id="smoke-e2e", name="E2E Smoke", command_id="test.e2e",
        values={"suite": "smoke"}, source="shared",
    )


def test_load_presets_reads_local_file_only(tmp_path):
    (tmp_path / ".dia").mkdir()
    (tmp_path / ".dia" / "console-presets.local.yaml").write_text(
        textwrap.dedent("""\
            presets:
              - id: my-quick-run
                name: My Quick Run
                command: run
                values:
                  target: cluichetest
        """),
        encoding="utf-8",
    )
    presets = load_presets(tmp_path)
    assert len(presets) == 1
    assert presets[0].source == "local"
    assert presets[0].id == "my-quick-run"


def test_load_presets_merges_non_colliding_entries_from_both_files(tmp_path):
    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir()
    (dia_dir / "console-presets.yaml").write_text(
        "presets:\n  - id: shared-one\n    name: Shared One\n    command: check.arch\n    values: {}\n",
        encoding="utf-8",
    )
    (dia_dir / "console-presets.local.yaml").write_text(
        "presets:\n  - id: local-one\n    name: Local One\n    command: run\n    values: {}\n",
        encoding="utf-8",
    )
    ids = {p.id for p in load_presets(tmp_path)}
    assert ids == {"shared-one", "local-one"}


def test_load_presets_local_wins_on_id_collision(tmp_path):
    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir()
    (dia_dir / "console-presets.yaml").write_text(
        textwrap.dedent("""\
            presets:
              - id: dup
                name: Shared Name
                command: test.e2e
                values:
                  suite: smoke
        """),
        encoding="utf-8",
    )
    (dia_dir / "console-presets.local.yaml").write_text(
        textwrap.dedent("""\
            presets:
              - id: dup
                name: Local Name
                command: run
                values:
                  target: cluichetest
        """),
        encoding="utf-8",
    )
    presets = load_presets(tmp_path)
    assert len(presets) == 1
    winner = presets[0]
    assert winner.source == "local"
    assert winner.name == "Local Name"
    assert winner.command_id == "run"
    assert winner.values == {"target": "cluichetest"}


def test_load_presets_missing_presets_key_degrades_to_empty(tmp_path):
    (tmp_path / ".dia").mkdir()
    (tmp_path / ".dia" / "console-presets.yaml").write_text("{}\n", encoding="utf-8")
    assert load_presets(tmp_path) == []


# ===========================================================================
# save_preset / delete_preset
# ===========================================================================

def test_save_preset_creates_file_if_missing(tmp_path):
    preset = PresetDescriptor(
        id="new-one", name="New One", command_id="run", values={"target": "x"}, source="local")
    save_preset(tmp_path, preset, scope="local")

    saved = load_presets(tmp_path)
    assert len(saved) == 1
    assert saved[0] == preset
    assert not (tmp_path / ".dia" / "console-presets.yaml").exists()


def test_save_preset_replaces_existing_entry_with_same_id(tmp_path):
    original = PresetDescriptor(
        id="dup", name="Original", command_id="run", values={"target": "a"}, source="shared")
    save_preset(tmp_path, original, scope="shared")

    updated = PresetDescriptor(
        id="dup", name="Updated", command_id="run", values={"target": "b"}, source="shared")
    save_preset(tmp_path, updated, scope="shared")

    saved = load_presets(tmp_path)
    assert len(saved) == 1
    assert saved[0].name == "Updated"
    assert saved[0].values == {"target": "b"}


def test_save_preset_is_idempotent(tmp_path):
    preset = PresetDescriptor(
        id="dup", name="Name", command_id="run", values={"target": "a"}, source="shared")
    save_preset(tmp_path, preset, scope="shared")
    save_preset(tmp_path, preset, scope="shared")
    assert len(load_presets(tmp_path)) == 1


def test_save_preset_only_touches_target_file_not_the_other(tmp_path):
    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir()
    local_path = dia_dir / "console-presets.local.yaml"
    local_path.write_text(
        "presets:\n  - id: keep-me\n    name: Keep Me\n    command: run\n    values: {}\n",
        encoding="utf-8",
    )
    local_before = local_path.read_text(encoding="utf-8")

    preset = PresetDescriptor(
        id="shared-new", name="Shared New", command_id="check.arch", values={}, source="shared")
    save_preset(tmp_path, preset, scope="shared")

    assert local_path.read_text(encoding="utf-8") == local_before
    ids = {p.id for p in load_presets(tmp_path)}
    assert ids == {"keep-me", "shared-new"}


def test_delete_preset_removes_entry_only_from_target_file(tmp_path):
    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir()
    (dia_dir / "console-presets.yaml").write_text(
        "presets:\n  - id: gone\n    name: Gone\n    command: run\n    values: {}\n",
        encoding="utf-8",
    )
    (dia_dir / "console-presets.local.yaml").write_text(
        "presets:\n  - id: gone\n    name: Local Gone\n    command: run\n    values: {}\n",
        encoding="utf-8",
    )

    delete_preset(tmp_path, "gone", scope="shared")

    remaining = load_presets(tmp_path)
    assert len(remaining) == 1
    assert remaining[0].source == "local"


def test_delete_preset_unknown_id_is_idempotent_noop(tmp_path):
    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir()
    (dia_dir / "console-presets.yaml").write_text(
        "presets:\n  - id: stays\n    name: Stays\n    command: run\n    values: {}\n",
        encoding="utf-8",
    )
    delete_preset(tmp_path, "does-not-exist", scope="shared")
    delete_preset(tmp_path, "does-not-exist", scope="shared")
    assert {p.id for p in load_presets(tmp_path)} == {"stays"}


# ===========================================================================
# build_execute_request
# ===========================================================================

@pytest.fixture
def sample_descriptor():
    return CommandDescriptor(
        id="grp.cmd",
        path=("grp", "cmd"),
        name="cmd",
        description="",
        category="grp",
        arguments=(
            ArgumentDescriptor(
                name="target", help="", type=ArgumentType.STRING, required=True,
                default=None, multiple=False, raw_click_type="String"),
        ),
        options=(
            OptionDescriptor(
                name="config", help="", type=ArgumentType.STRING, required=False,
                default=None, multiple=False, is_flag=False, cli_flag="--config"),
        ),
    )


def test_build_execute_request_splits_arguments_and_options(sample_descriptor):
    request = build_execute_request(sample_descriptor, {"target": "cluichetest", "config": "Debug"})
    assert request.command_id == "grp.cmd"
    assert request.arguments == {"target": "cluichetest"}
    assert request.options == {"config": "Debug"}
    assert request.project_id is None


def test_build_execute_request_drops_unknown_keys(sample_descriptor):
    request = build_execute_request(
        sample_descriptor, {"target": "t", "config": "Debug", "typo_field": "x"})
    assert "typo_field" not in request.arguments
    assert "typo_field" not in request.options


def test_build_execute_request_passes_through_project_id(sample_descriptor):
    request = build_execute_request(sample_descriptor, {"target": "t"}, project_id="cluichetest")
    assert request.project_id == "cluichetest"


# ===========================================================================
# dia preset run — real-subprocess-style integration, against a fake `dia`
# ===========================================================================

FAKE_DIA = '''
import sys

def main():
    argv = sys.argv[1:]
    if argv[:1] == ["--log-json"]:
        argv = argv[2:]
    print("fake_dia ran: " + " ".join(argv), flush=True)
    return 0

sys.exit(main())
'''


@pytest.fixture
def fake_dia_script(tmp_path):
    script = tmp_path / "fake_dia.py"
    script.write_text(FAKE_DIA, encoding="utf-8")
    return script


@pytest.fixture
def fake_dia_registry():
    @click.group()
    def root():
        """Root."""

    @root.command("e2e")
    @click.option("--suite", default=None)
    def e2e(suite):
        """Fake e2e test command."""

    @root.command("run")
    @click.option("--target", default=None)
    def run(target):
        """Fake run command."""

    return CommandRegistry.from_click_app(root)


@pytest.mark.integration
def test_run_preset_resolves_a_real_shared_preset_entry_end_to_end(
    tmp_path, fake_dia_script, fake_dia_registry,
):
    from dia_console.execution import ExecutionService
    from dia_cli.cli.preset import run_preset

    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir()
    (dia_dir / "console-presets.yaml").write_text(
        textwrap.dedent("""\
            presets:
              - id: smoke-e2e
                name: E2E Smoke
                command: e2e
                values:
                  suite: smoke
        """),
        encoding="utf-8",
    )

    service = ExecutionService(
        fake_dia_registry,
        argv_prefix=[sys.executable, str(fake_dia_script)],
        repo_root=tmp_path,
        log_root=tmp_path / "logs",
    )

    returncode = run_preset(
        tmp_path, "smoke-e2e", registry=fake_dia_registry, execution_service=service)
    assert returncode == 0


@pytest.mark.integration
def test_run_preset_unknown_id_raises_click_exception(tmp_path, fake_dia_registry):
    from dia_cli.cli.preset import run_preset

    (tmp_path / ".dia").mkdir()
    (tmp_path / ".dia" / "console-presets.yaml").write_text("presets: []\n", encoding="utf-8")

    with pytest.raises(click.ClickException):
        run_preset(tmp_path, "does-not-exist", registry=fake_dia_registry)


@pytest.mark.integration
def test_local_preset_collision_overrides_shared_end_to_end(
    tmp_path, fake_dia_script, fake_dia_registry, capfd,
):
    """Task 13's acceptance test: local wins the whole load->run pipeline, not just load_presets."""
    from dia_console.execution import ExecutionService
    from dia_cli.cli.preset import run_preset

    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir()
    (dia_dir / "console-presets.yaml").write_text(
        textwrap.dedent("""\
            presets:
              - id: dup
                name: Shared Version
                command: e2e
                values:
                  suite: smoke
        """),
        encoding="utf-8",
    )
    (dia_dir / "console-presets.local.yaml").write_text(
        textwrap.dedent("""\
            presets:
              - id: dup
                name: Local Version
                command: run
                values:
                  target: cluichetest
        """),
        encoding="utf-8",
    )

    service = ExecutionService(
        fake_dia_registry,
        argv_prefix=[sys.executable, str(fake_dia_script)],
        repo_root=tmp_path,
        log_root=tmp_path / "logs",
    )

    returncode = run_preset(
        tmp_path, "dup", registry=fake_dia_registry, execution_service=service)
    assert returncode == 0

    captured = capfd.readouterr()
    assert "--target cluichetest" in captured.out
    assert "--suite" not in captured.out


def test_list_presets_text_reports_id_name_command_source(tmp_path):
    from dia_cli.cli.preset import list_presets_text

    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir()
    (dia_dir / "console-presets.yaml").write_text(
        "presets:\n  - id: a\n    name: A\n    command: run\n    values: {}\n",
        encoding="utf-8",
    )
    text = list_presets_text(tmp_path)
    assert "a" in text and "A" in text and "run" in text and "shared" in text


def test_preset_module_never_invokes_click_in_process():
    import inspect
    from dia_cli.cli import preset as preset_module

    source = inspect.getsource(preset_module)
    for banned in (".invoke(", "CliRunner", "standalone_mode"):
        assert banned not in source, f"in-process Click invocation via {banned!r}"
