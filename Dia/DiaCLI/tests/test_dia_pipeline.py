"""Unit tests for DiaPipeline (pipeline-config, stages, runner, CLI)."""
import textwrap
from pathlib import Path
from unittest.mock import MagicMock, patch, call
import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.pipeline.pipeline_config import (
    load_pipeline_config, PipelineConfigError, VALID_STAGES,
)
from dia_cli.commands.pipeline.path_resolver import resolve_variables, resolve_out_dir


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_MINIMAL_TOML = textwrap.dedent("""\
    [global]
    default_config = "Debug"
    default_platform = "x64"
    default_target = "googletest"

    [proto]
    proto_dir = "Dia/DiaDebugProtocol/proto"
    output_dir = "Dia/DiaDebugProtocol/proto/generated"
    language = "cpp"

    [targets.googletest]
    project = "Cluiche/Tests/GoogleTests/GoogleTests.vcxproj"
    stages = ["proto-compile", "compile-code", "asset-build", "package"]

    [targets.googletest.package]
    files = [
      { src = "External/Python311/python311.dll", dest = "$(OutDir)" },
    ]
""")


def _write_toml(tmp_path: Path, content: str = _MINIMAL_TOML) -> Path:
    p = tmp_path / "pipeline.toml"
    p.write_text(content)
    return tmp_path


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_pipeline_command_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])
    assert "pipeline" in result.output


def test_pipeline_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["pipeline", "--help"])
    assert result.exit_code == 0
    assert "--config" in result.output
    assert "--target" in result.output
    assert "--stage" in result.output
    assert "--force" in result.output
    assert "--docker" in result.output


# ---------------------------------------------------------------------------
# pipeline_config: load_pipeline_config
# ---------------------------------------------------------------------------

def test_load_config_missing_toml(tmp_path):
    with pytest.raises(PipelineConfigError, match="pipeline.toml not found"):
        load_pipeline_config(tmp_path)


def test_load_config_parses_global(tmp_path):
    _write_toml(tmp_path)
    cfg = load_pipeline_config(tmp_path)
    assert cfg.global_cfg.default_config == "Debug"
    assert cfg.global_cfg.default_platform == "x64"
    assert cfg.global_cfg.default_target == "googletest"


def test_load_config_parses_proto(tmp_path):
    _write_toml(tmp_path)
    cfg = load_pipeline_config(tmp_path)
    assert cfg.proto.proto_dir == "Dia/DiaDebugProtocol/proto"
    assert cfg.proto.language == "cpp"


def test_load_config_parses_target(tmp_path):
    _write_toml(tmp_path)
    cfg = load_pipeline_config(tmp_path)
    assert "googletest" in cfg.targets
    assert cfg.targets["googletest"].project == "Cluiche/Tests/GoogleTests/GoogleTests.vcxproj"


def test_load_config_parses_package_files(tmp_path):
    _write_toml(tmp_path)
    cfg = load_pipeline_config(tmp_path)
    files = cfg.targets["googletest"].package.files
    assert len(files) == 1
    assert files[0].src == "External/Python311/python311.dll"
    assert files[0].dest == "$(OutDir)"


def test_load_config_invalid_stage_name(tmp_path):
    bad = _MINIMAL_TOML.replace(
        'stages = ["proto-compile", "compile-code", "asset-build", "package"]',
        'stages = ["proto-compile", "bogus-stage"]',
    )
    _write_toml(tmp_path, bad)
    with pytest.raises(PipelineConfigError, match="unknown stage"):
        load_pipeline_config(tmp_path)


# ---------------------------------------------------------------------------
# path_resolver
# ---------------------------------------------------------------------------

def test_resolve_out_dir_debug(tmp_path):
    result = resolve_out_dir(tmp_path, "Debug", "x64")
    assert result == tmp_path / "Cluiche" / "bin" / "Debug" / "x64"


def test_resolve_out_dir_release(tmp_path):
    result = resolve_out_dir(tmp_path, "Release", "x64")
    assert result == tmp_path / "Cluiche" / "bin" / "Release" / "x64"


def test_resolve_variables_out_dir(tmp_path):
    result = resolve_variables("$(OutDir)file.dll", "Debug", "x64", tmp_path)
    assert "Cluiche/bin/Debug/x64" in result
    assert result.endswith("file.dll")


def test_resolve_variables_configuration(tmp_path):
    result = resolve_variables("bin/$(Configuration)/foo.dll", "Release", "x64", tmp_path)
    assert "bin/Release/foo.dll" in result


def test_resolve_variables_platform(tmp_path):
    result = resolve_variables("lib/$(Platform)/x.lib", "Debug", "x64", tmp_path)
    assert "lib/x64/x.lib" in result


# ---------------------------------------------------------------------------
# CLI: validation errors exit 2
# ---------------------------------------------------------------------------

def _make_runner_with_toml(tmp_path):
    _write_toml(tmp_path)
    return CliRunner()


@patch("utils.repo_root.find_repo_root")
def test_cli_unknown_target_exits_2(mock_root, tmp_path):
    mock_root.return_value = _write_toml(tmp_path)
    runner = CliRunner()
    result = runner.invoke(cli, ["pipeline", "--target", "badtarget"])
    assert result.exit_code == 2
    assert "unknown target" in result.output.lower() or "unknown target" in (result.output + (result.exception and str(result.exception) or "")).lower()


@patch("utils.repo_root.find_repo_root")
def test_cli_unknown_stage_exits_2(mock_root, tmp_path):
    mock_root.return_value = _write_toml(tmp_path)
    runner = CliRunner()
    result = runner.invoke(cli, ["pipeline", "--stage", "bogus-stage"])
    assert result.exit_code == 2


@patch("utils.repo_root.find_repo_root")
def test_cli_missing_toml_exits_2(mock_root, tmp_path):
    mock_root.return_value = tmp_path  # tmp_path has no pipeline.toml
    # Also patch the MDK-namespace version so both load paths raise
    with patch("commands.pipeline.pipeline_config.load_pipeline_config") as mock_load:
        from commands.pipeline.pipeline_config import PipelineConfigError as E
        mock_load.side_effect = E("pipeline.toml not found")
        runner = CliRunner()
        result = runner.invoke(cli, ["pipeline"])
    assert result.exit_code == 2


# ---------------------------------------------------------------------------
# proto_compile_stage
# ---------------------------------------------------------------------------

from dia_cli.commands.pipeline.stages import proto_compile_stage


def _make_proto_config(tmp_path):
    from dia_cli.commands.pipeline.pipeline_config import PipelineConfig, GlobalConfig, ProtoConfig
    return PipelineConfig(
        global_cfg=GlobalConfig(),
        proto=ProtoConfig(
            proto_dir="proto",
            output_dir="proto/generated",
        ),
        targets={},
    )


def test_proto_compile_skips_when_sentinel_present(tmp_path, caplog):
    cfg = _make_proto_config(tmp_path)
    sentinel = tmp_path / proto_compile_stage._SENTINEL_PATH
    sentinel.parent.mkdir(parents=True, exist_ok=True)
    sentinel.write_text("ok")
    import logging
    with caplog.at_level(logging.INFO, logger="root"):
        code = proto_compile_stage.run(cfg, "googletest", "Debug", force=False, repo_root=tmp_path)
    assert code == 0


def test_proto_compile_force_ignores_sentinel(tmp_path):
    cfg = _make_proto_config(tmp_path)
    sentinel = tmp_path / proto_compile_stage._SENTINEL_PATH
    sentinel.parent.mkdir(parents=True, exist_ok=True)
    sentinel.write_text("ok")
    with patch("dia_cli.commands.pipeline.stages.proto_compile_stage.subprocess.run") as mock_run:
        # protoc not found at default path → but we also patch to check it's attempted
        code = proto_compile_stage.run(cfg, "googletest", "Debug", force=True, repo_root=tmp_path)
    # protoc won't exist in tmp_path → exit 1 (not found)
    assert code == 1


def test_proto_compile_missing_protoc_exits_1(tmp_path):
    cfg = _make_proto_config(tmp_path)
    code = proto_compile_stage.run(cfg, "googletest", "Debug", force=True, repo_root=tmp_path)
    assert code == 1


@patch("dia_cli.commands.pipeline.stages.proto_compile_stage.subprocess.run")
def test_proto_compile_writes_sentinel_on_success(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=0)
    cfg = _make_proto_config(tmp_path)
    # Make protoc "exist" by writing a dummy file
    protoc = tmp_path / proto_compile_stage._DEFAULT_PROTOC
    protoc.parent.mkdir(parents=True, exist_ok=True)
    protoc.write_text("dummy")
    (tmp_path / "proto").mkdir(parents=True, exist_ok=True)
    (tmp_path / "proto/debug_protocol.proto").write_text("")
    code = proto_compile_stage.run(cfg, "googletest", "Debug", force=True, repo_root=tmp_path)
    assert code == 0
    assert (tmp_path / proto_compile_stage._SENTINEL_PATH).exists()


@patch("dia_cli.commands.pipeline.stages.proto_compile_stage.subprocess.run")
def test_proto_compile_no_sentinel_on_failure(mock_run, tmp_path):
    mock_run.return_value = MagicMock(returncode=1, stderr="error msg")
    cfg = _make_proto_config(tmp_path)
    protoc = tmp_path / proto_compile_stage._DEFAULT_PROTOC
    protoc.parent.mkdir(parents=True, exist_ok=True)
    protoc.write_text("dummy")
    (tmp_path / "proto").mkdir(parents=True, exist_ok=True)
    (tmp_path / "proto/debug_protocol.proto").write_text("")
    code = proto_compile_stage.run(cfg, "googletest", "Debug", force=True, repo_root=tmp_path)
    assert code == 1
    assert not (tmp_path / proto_compile_stage._SENTINEL_PATH).exists()


# ---------------------------------------------------------------------------
# compile_code_stage
# ---------------------------------------------------------------------------

from dia_cli.commands.pipeline.stages import compile_code_stage


def _make_compile_config(tmp_path, project_rel="Proj.vcxproj"):
    from dia_cli.commands.pipeline.pipeline_config import (
        PipelineConfig, GlobalConfig, ProtoConfig, TargetConfig, PackageConfig,
    )
    (tmp_path / project_rel).write_text("dummy")
    return PipelineConfig(
        global_cfg=GlobalConfig(),
        proto=ProtoConfig(),
        targets={"googletest": TargetConfig(project=project_rel, stages=[], package=PackageConfig())},
    )


@patch("dia_cli.commands.pipeline.stages.compile_code_stage._find_msbuild", return_value=None)
def test_compile_code_no_msbuild_exits_1(mock_find, tmp_path):
    cfg = _make_compile_config(tmp_path)
    code = compile_code_stage.run(cfg, "googletest", "Debug", False, tmp_path)
    assert code == 1


@patch("dia_cli.commands.pipeline.stages.compile_code_stage._find_msbuild")
@patch("dia_cli.commands.pipeline.stages.compile_code_stage.subprocess.run")
def test_compile_code_invokes_msbuild(mock_run, mock_find, tmp_path):
    fake_msbuild = tmp_path / "msbuild.exe"
    fake_msbuild.write_text("dummy")
    mock_find.return_value = fake_msbuild
    mock_run.return_value = MagicMock(returncode=0)
    cfg = _make_compile_config(tmp_path)
    code = compile_code_stage.run(cfg, "googletest", "Debug", False, tmp_path)
    assert code == 0
    args = mock_run.call_args[0][0]
    assert str(fake_msbuild) in args
    assert "/p:Configuration=Debug" in args
    assert "/p:Platform=x64" in args


@patch("dia_cli.commands.pipeline.stages.compile_code_stage._find_msbuild")
@patch("dia_cli.commands.pipeline.stages.compile_code_stage.subprocess.run")
def test_compile_code_propagates_nonzero(mock_run, mock_find, tmp_path):
    fake_msbuild = tmp_path / "msbuild.exe"
    fake_msbuild.write_text("dummy")
    mock_find.return_value = fake_msbuild
    mock_run.return_value = MagicMock(returncode=1)
    cfg = _make_compile_config(tmp_path)
    code = compile_code_stage.run(cfg, "googletest", "Debug", False, tmp_path)
    assert code == 1


# ---------------------------------------------------------------------------
# asset_build_stage
# ---------------------------------------------------------------------------

from dia_cli.commands.pipeline.stages import asset_build_stage


def test_asset_build_exits_0(tmp_path):
    from dia_cli.commands.pipeline.pipeline_config import PipelineConfig, GlobalConfig, ProtoConfig
    cfg = PipelineConfig(global_cfg=GlobalConfig(), proto=ProtoConfig(), targets={})
    code = asset_build_stage.run(cfg, "googletest", "Debug", False, tmp_path)
    assert code == 0


# ---------------------------------------------------------------------------
# package_stage
# ---------------------------------------------------------------------------

from dia_cli.commands.pipeline.stages import package_stage


def _make_package_config(tmp_path, files):
    from dia_cli.commands.pipeline.pipeline_config import (
        PipelineConfig, GlobalConfig, ProtoConfig, TargetConfig, PackageConfig, PackageFile,
    )
    pkg_files = [PackageFile(src=f["src"], dest=f["dest"]) for f in files]
    return PipelineConfig(
        global_cfg=GlobalConfig(),
        proto=ProtoConfig(),
        targets={
            "googletest": TargetConfig(
                project="",
                stages=[],
                package=PackageConfig(files=pkg_files),
            )
        },
    )


def test_package_copies_file(tmp_path):
    src_file = tmp_path / "External" / "Python311" / "python311.dll"
    src_file.parent.mkdir(parents=True, exist_ok=True)
    src_file.write_bytes(b"fake dll")

    cfg = _make_package_config(tmp_path, [
        {"src": "External/Python311/python311.dll", "dest": "$(OutDir)"},
    ])
    code = package_stage.run(cfg, "googletest", "Debug", force=True, repo_root=tmp_path)
    assert code == 0
    dest = tmp_path / "Cluiche" / "bin" / "Debug" / "x64" / "python311.dll"
    assert dest.exists()


def test_package_skips_when_staged(tmp_path):
    src_file = tmp_path / "lib" / "file.dll"
    src_file.parent.mkdir(parents=True, exist_ok=True)
    src_file.write_bytes(b"data")

    dest_dir = tmp_path / "Cluiche" / "bin" / "Debug" / "x64"
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest_file = dest_dir / "file.dll"
    dest_file.write_bytes(b"data")
    # Make dest newer than src
    import os, time
    os.utime(dest_file, (time.time() + 10, time.time() + 10))

    cfg = _make_package_config(tmp_path, [
        {"src": "lib/file.dll", "dest": "$(OutDir)"},
    ])
    code = package_stage.run(cfg, "googletest", "Debug", force=False, repo_root=tmp_path)
    assert code == 0


def test_package_force_recopies(tmp_path):
    src_file = tmp_path / "lib" / "file.dll"
    src_file.parent.mkdir(parents=True, exist_ok=True)
    src_file.write_bytes(b"new data")

    dest_dir = tmp_path / "Cluiche" / "bin" / "Debug" / "x64"
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest_file = dest_dir / "file.dll"
    dest_file.write_bytes(b"old data")
    import os, time
    os.utime(dest_file, (time.time() + 10, time.time() + 10))

    cfg = _make_package_config(tmp_path, [
        {"src": "lib/file.dll", "dest": "$(OutDir)"},
    ])
    code = package_stage.run(cfg, "googletest", "Debug", force=True, repo_root=tmp_path)
    assert code == 0
    assert dest_file.read_bytes() == b"new data"


def test_package_unmatched_glob_warns_not_fails(tmp_path):
    cfg = _make_package_config(tmp_path, [
        {"src": "nonexistent/*.dll", "dest": "$(OutDir)"},
    ])
    code = package_stage.run(cfg, "googletest", "Debug", force=True, repo_root=tmp_path)
    assert code == 0


def test_package_glob_expands(tmp_path):
    src_dir = tmp_path / "dlls"
    src_dir.mkdir()
    (src_dir / "a.dll").write_bytes(b"a")
    (src_dir / "b.dll").write_bytes(b"b")

    cfg = _make_package_config(tmp_path, [
        {"src": "dlls/*.dll", "dest": "$(OutDir)"},
    ])
    code = package_stage.run(cfg, "googletest", "Debug", force=True, repo_root=tmp_path)
    assert code == 0
    out = tmp_path / "Cluiche" / "bin" / "Debug" / "x64"
    assert (out / "a.dll").exists()
    assert (out / "b.dll").exists()


# ---------------------------------------------------------------------------
# pipeline_runner: integration with mock output
# ---------------------------------------------------------------------------

from dia_cli.commands.pipeline.pipeline_runner import run_pipeline


def _make_full_config(tmp_path):
    return _make_package_config(tmp_path, [])


def _mock_output():
    m = MagicMock()
    return m


_RUNNER_PREFIX = "dia_cli.commands.pipeline.pipeline_runner"


@patch(f"{_RUNNER_PREFIX}.proto_compile_stage.run", return_value=0)
@patch(f"{_RUNNER_PREFIX}.compile_code_stage.run", return_value=0)
@patch(f"{_RUNNER_PREFIX}.asset_build_stage.run", return_value=0)
@patch(f"{_RUNNER_PREFIX}.package_stage.run", return_value=0)
def test_runner_all_pass(m_pkg, m_asset, m_compile, m_proto, tmp_path):
    cfg = _make_full_config(tmp_path)
    out = _mock_output()
    code = run_pipeline(cfg, "googletest", ["proto-compile", "compile-code", "asset-build", "package"],
                        "Debug", False, out, tmp_path)
    assert code == 0
    out.run_completed.assert_called_once()


@patch(f"{_RUNNER_PREFIX}.proto_compile_stage.run", return_value=0)
@patch(f"{_RUNNER_PREFIX}.compile_code_stage.run", return_value=1)
def test_runner_stops_on_failure(m_compile, m_proto, tmp_path):
    cfg = _make_full_config(tmp_path)
    out = _mock_output()
    code = run_pipeline(cfg, "googletest", ["proto-compile", "compile-code", "asset-build", "package"],
                        "Debug", False, out, tmp_path)
    assert code == 1
    out.run_failed.assert_called_once()
    out.stage_failed.assert_called()


@patch(f"{_RUNNER_PREFIX}.proto_compile_stage.run", return_value=0)
@patch(f"{_RUNNER_PREFIX}.compile_code_stage.run", return_value=0)
@patch(f"{_RUNNER_PREFIX}.asset_build_stage.run", return_value=0)
@patch(f"{_RUNNER_PREFIX}.package_stage.run", return_value=0)
def test_runner_skips_stages_not_in_list(m_pkg, m_asset, m_compile, m_proto, tmp_path):
    cfg = _make_full_config(tmp_path)
    out = _mock_output()
    run_pipeline(cfg, "googletest", ["compile-code"], "Debug", False, out, tmp_path)
    m_proto.assert_not_called()
    m_compile.assert_called_once()


def test_runner_emits_run_started(tmp_path):
    cfg = _make_full_config(tmp_path)
    out = _mock_output()
    with patch(f"{_RUNNER_PREFIX}.proto_compile_stage.run", return_value=0), \
         patch(f"{_RUNNER_PREFIX}.compile_code_stage.run", return_value=0), \
         patch(f"{_RUNNER_PREFIX}.asset_build_stage.run", return_value=0), \
         patch(f"{_RUNNER_PREFIX}.package_stage.run", return_value=0):
        run_pipeline(cfg, "googletest", ["proto-compile"], "Debug", False, out, tmp_path)
    out.run_started.assert_called_once()
