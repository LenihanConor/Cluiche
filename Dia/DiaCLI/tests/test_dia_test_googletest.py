"""Unit tests for dia test googletest (AC1-AC9 from diatest/googletest.md spec)."""
import xml.etree.ElementTree as ET
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.test.googletest_runner import find_binary, run
from dia_cli.commands.test.xml_merger import merge_xml


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_binary(tmp_path: Path, config: str = "Debug") -> Path:
    binary = tmp_path / "Cluiche" / "bin" / "GoogleTests" / config / "x64" / "GoogleTests.exe"
    binary.parent.mkdir(parents=True, exist_ok=True)
    binary.write_bytes(b"fake exe")
    return binary


def _make_staged(tmp_path: Path, config: str = "Debug") -> Path:
    binary = _make_binary(tmp_path, config)
    (binary.parent / "python311.dll").write_bytes(b"fake dll")
    return binary


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_googletest_subcommand_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "--help"])
    assert result.exit_code == 0
    assert "googletest" in result.output


def test_googletest_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "googletest", "--help"])
    assert result.exit_code == 0
    assert "--filter" in result.output
    assert "--config" in result.output
    assert "--verbose" in result.output
    assert "--docker" in result.output


# ---------------------------------------------------------------------------
# find_binary
# ---------------------------------------------------------------------------

def test_find_binary_returns_path_when_exists(tmp_path):
    _make_binary(tmp_path)
    assert find_binary(tmp_path, "Debug") is not None


def test_find_binary_returns_none_when_missing(tmp_path):
    assert find_binary(tmp_path, "Debug") is None


def test_find_binary_config_selects_release(tmp_path):
    _make_binary(tmp_path, "Release")
    assert find_binary(tmp_path, "Release") is not None
    assert find_binary(tmp_path, "Debug") is None


# ---------------------------------------------------------------------------
# AC5: exit 2 if binary missing
# ---------------------------------------------------------------------------

def test_run_exits_2_when_binary_missing(tmp_path, capsys):
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False)
    assert code == 2
    captured = capsys.readouterr()
    assert "GoogleTests.exe not found" in captured.out
    assert "dia pipeline" in captured.out


# ---------------------------------------------------------------------------
# AC6: exit 2 if python311.dll missing
# ---------------------------------------------------------------------------

def test_run_exits_2_when_python_dll_missing(tmp_path, capsys):
    _make_binary(tmp_path)  # binary exists but no dll
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False)
    assert code == 2
    captured = capsys.readouterr()
    assert "Runtime dependencies" in captured.out
    assert "dia pipeline" in captured.out


# ---------------------------------------------------------------------------
# AC1: runs binary and exits with its return code
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_invokes_binary(mock_run, tmp_path):
    binary = _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False, run_all=False)
    assert code == 0
    args = mock_run.call_args[0][0]
    assert str(binary) == args[0]


@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_propagates_nonzero_exit(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=1)
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False, run_all=False)
    assert code == 1


# ---------------------------------------------------------------------------
# AC2: cwd is set to binary directory
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_uses_binary_dir_as_cwd(mock_run, tmp_path):
    binary = _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern=None,
        verbose=False, docker=False, run_all=False)
    cwd = mock_run.call_args[1]["cwd"]
    assert cwd == str(binary.parent)


# ---------------------------------------------------------------------------
# AC3: --filter passes --gtest_filter
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_filter_passes_gtest_filter(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern="TestArray*",
        verbose=False, docker=False, run_all=False)
    args = mock_run.call_args[0][0]
    assert "--gtest_filter=TestArray*" in args


# ---------------------------------------------------------------------------
# AC4: --config Release uses Release binary
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_config_release_uses_release_binary(mock_run, tmp_path):
    _make_staged(tmp_path, "Release")
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Release", filter_pattern=None,
        verbose=False, docker=False, run_all=False)
    args = mock_run.call_args[0][0]
    assert "Release" in args[0]


# ---------------------------------------------------------------------------
# AC9: --verbose passes --gtest_print_time
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_verbose_passes_gtest_verbose(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern=None,
        verbose=True, docker=False, run_all=False)
    args = mock_run.call_args[0][0]
    assert "--gtest_print_time=1" in args


# ---------------------------------------------------------------------------
# AC8: --docker image missing → exit 3
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_docker_missing_image_exits_3(mock_run, tmp_path, capsys):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=1)  # inspect fails
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=True)
    assert code == 3
    captured = capsys.readouterr()
    assert "not found" in captured.out


# ---------------------------------------------------------------------------
# AC7: --docker forwards all flags
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_docker_forwards_flags(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.side_effect = [
        MagicMock(returncode=0),  # image inspect
        MagicMock(returncode=0),  # docker run
    ]
    run(repo_root=tmp_path, config="Release", filter_pattern="Foo*",
        verbose=True, docker=True)
    docker_cmd = mock_run.call_args_list[1][0][0]
    joined = " ".join(docker_cmd)
    assert "--config" in joined
    assert "Release" in joined
    assert "--filter" in joined
    assert "Foo*" in joined
    assert "--verbose" in joined


# ---------------------------------------------------------------------------
# SLOW_ suite tagging: default filter injection and --all flag
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_default_injects_slow_filter(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern=None, verbose=False, docker=False, run_all=False)
    args = mock_run.call_args[0][0]
    assert "--gtest_filter=-SLOW_*" in args


@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_all_skips_default_filter(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern=None, verbose=False, docker=False, run_all=True)
    args = mock_run.call_args[0][0]
    assert "--gtest_filter=-SLOW_*" not in args
    assert not any("gtest_filter" in a for a in args)


@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_explicit_filter_overrides_default(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    run(repo_root=tmp_path, config="Debug", filter_pattern="TestArray*", verbose=False, docker=False, run_all=False)
    args = mock_run.call_args[0][0]
    assert "--gtest_filter=TestArray*" in args
    assert "--gtest_filter=-SLOW_*" not in args


@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_warns_untagged_slow_suites(mock_run, tmp_path, capsys):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    # Pre-create a fake XML with a slow untagged suite
    xml_path = tmp_path / "Cluiche" / "out" / "GoogleTests" / "last_run.xml"
    xml_path.parent.mkdir(parents=True, exist_ok=True)
    xml_path.write_text(
        '<?xml version="1.0"?>'
        '<testsuites>'
        '<testsuite name="PythonBindings" time="1.2" tests="3" />'
        '<testsuite name="SLOW_Physics" time="2.0" tests="5" />'
        '</testsuites>'
    )
    # Run (XML already exists; subprocess won't overwrite in this mock scenario)
    run(repo_root=tmp_path, config="Debug", filter_pattern=None, verbose=False, docker=False, run_all=False)
    captured = capsys.readouterr()
    assert "WARNING" in captured.out
    assert "PythonBindings" in captured.out
    assert "SLOW_Physics" not in captured.out  # already tagged


@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_no_warning_when_all_tagged(mock_run, tmp_path, capsys):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    xml_path = tmp_path / "Cluiche" / "out" / "GoogleTests" / "last_run.xml"
    xml_path.parent.mkdir(parents=True, exist_ok=True)
    xml_path.write_text(
        '<?xml version="1.0"?>'
        '<testsuites>'
        '<testsuite name="SLOW_PythonBindings" time="1.2" tests="3" />'
        '</testsuites>'
    )
    run(repo_root=tmp_path, config="Debug", filter_pattern=None, verbose=False, docker=False, run_all=False)
    captured = capsys.readouterr()
    assert "WARNING" not in captured.out


# ---------------------------------------------------------------------------
# _resolve_full_suite_config
# ---------------------------------------------------------------------------

def test_run_all_without_explicit_config_uses_full_suite_config(tmp_path):
    """--all without --config should resolve full_suite_config (Release) from pipeline config."""
    from dia_cli.cli.run import _resolve_full_suite_config
    mock_target = MagicMock()
    mock_target.full_suite_config = "Release"
    mock_cfg = MagicMock()
    mock_cfg.targets = {"googletest": mock_target}
    with patch("dia_cli.commands.pipeline.pipeline_config.load_pipeline_config", return_value=mock_cfg):
        result = _resolve_full_suite_config(tmp_path, "googletest")
    assert result == "Release"


def test_resolve_full_suite_config_returns_release_on_missing_toml(tmp_path):
    """_resolve_full_suite_config returns 'Release' when pipeline.toml is missing."""
    from dia_cli.cli.run import _resolve_full_suite_config
    # tmp_path has no pipeline.toml → load_pipeline_config raises PipelineConfigError
    result = _resolve_full_suite_config(tmp_path, "googletest")
    assert result == "Release"


def test_resolve_full_suite_config_returns_release_for_unknown_target(tmp_path):
    """_resolve_full_suite_config returns 'Release' when target is not in pipeline config."""
    from dia_cli.cli.run import _resolve_full_suite_config
    mock_cfg = MagicMock()
    mock_cfg.targets = {}
    with patch("dia_cli.commands.pipeline.pipeline_config.load_pipeline_config", return_value=mock_cfg):
        result = _resolve_full_suite_config(tmp_path, "nonexistent")
    assert result == "Release"


# ---------------------------------------------------------------------------
# xml_merger tests
# ---------------------------------------------------------------------------

def test_merge_xml_combines_testsuites(tmp_path):
    a = tmp_path / "a.xml"
    b = tmp_path / "b.xml"
    a.write_text('<?xml version="1.0"?><testsuites><testsuite name="A" tests="1"/></testsuites>')
    b.write_text('<?xml version="1.0"?><testsuites><testsuite name="B" tests="2"/></testsuites>')
    out = tmp_path / "merged.xml"
    merge_xml([a, b], out)
    root = ET.parse(str(out)).getroot()
    names = {ts.get("name") for ts in root.findall("testsuite")}
    assert names == {"A", "B"}


def test_merge_xml_skips_missing_files(tmp_path):
    a = tmp_path / "a.xml"
    a.write_text('<?xml version="1.0"?><testsuites><testsuite name="A" tests="1"/></testsuites>')
    out = tmp_path / "merged.xml"
    merge_xml([a, tmp_path / "missing.xml"], out)
    root = ET.parse(str(out)).getroot()
    assert len(root.findall("testsuite")) == 1


def test_merge_xml_skips_malformed_files(tmp_path):
    a = tmp_path / "a.xml"
    bad = tmp_path / "bad.xml"
    a.write_text('<?xml version="1.0"?><testsuites><testsuite name="A" tests="1"/></testsuites>')
    bad.write_text("not valid xml <<<")
    out = tmp_path / "merged.xml"
    merge_xml([a, bad], out)
    root = ET.parse(str(out)).getroot()
    assert len(root.findall("testsuite")) == 1


def test_merge_xml_creates_parent_dirs(tmp_path):
    a = tmp_path / "a.xml"
    a.write_text('<?xml version="1.0"?><testsuites><testsuite name="A" tests="1"/></testsuites>')
    out = tmp_path / "nested" / "dir" / "merged.xml"
    merge_xml([a], out)
    assert out.exists()


def test_merge_xml_empty_inputs_creates_empty_root(tmp_path):
    out = tmp_path / "merged.xml"
    merge_xml([], out)
    root = ET.parse(str(out)).getroot()
    assert root.tag == "testsuites"
    assert len(list(root)) == 0


# ---------------------------------------------------------------------------
# shard runner: shards=0 uses normal single-process path
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_shards_0_uses_normal_path(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False, run_all=False, shards=0)
    assert code == 0
    # shards=0 → single subprocess invocation (not list_tests + N shards)
    assert mock_run.call_count == 1


@patch("dia_cli.commands.test.googletest_runner.subprocess.run")
def test_run_shards_1_uses_normal_path(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0)
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False, run_all=False, shards=1)
    assert code == 0
    assert mock_run.call_count == 1


# ---------------------------------------------------------------------------
# shard runner: shards>1 delegates to run_shards
# ---------------------------------------------------------------------------

@patch("dia_cli.commands.test.shard_runner.subprocess.run")
def test_run_shards_delegates_when_shards_gt_1(mock_run, tmp_path):
    _make_staged(tmp_path)
    # First call is --gtest_list_tests, subsequent are shard runs
    list_output = "SuiteA.\n  TestOne\n  TestTwo\nSuiteB.\n  TestThree\n  TestFour\n"
    mock_run.side_effect = [
        MagicMock(returncode=0, stdout=list_output),  # list_tests
        MagicMock(returncode=0),                       # shard 0
        MagicMock(returncode=0),                       # shard 1
    ]
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False, run_all=False, shards=2)
    assert code == 0
    # list_tests + 2 shard runs = 3 subprocess calls
    assert mock_run.call_count == 3


@patch("dia_cli.commands.test.shard_runner.subprocess.run")
def test_run_shards_returns_nonzero_if_any_shard_fails(mock_run, tmp_path):
    _make_staged(tmp_path)
    list_output = "SuiteA.\n  TestOne\n  TestTwo\n"
    mock_run.side_effect = [
        MagicMock(returncode=0, stdout=list_output),  # list_tests
        MagicMock(returncode=0),                       # shard 0 passes
        MagicMock(returncode=1),                       # shard 1 fails
    ]
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False, run_all=False, shards=2)
    assert code != 0


@patch("dia_cli.commands.test.shard_runner.subprocess.run")
def test_run_shards_no_tests_found_returns_nonzero(mock_run, tmp_path):
    _make_staged(tmp_path)
    mock_run.return_value = MagicMock(returncode=0, stdout="")
    code = run(repo_root=tmp_path, config="Debug", filter_pattern=None,
               verbose=False, docker=False, run_all=False, shards=2)
    assert code != 0


@patch("dia_cli.commands.test.shard_runner.subprocess.run")
def test_run_shards_composes_with_filter(mock_run, tmp_path):
    _make_staged(tmp_path)
    list_output = "SuiteA.\n  TestOne\n  TestTwo\n"
    mock_run.side_effect = [
        MagicMock(returncode=0, stdout=list_output),
        MagicMock(returncode=0),
        MagicMock(returncode=0),
    ]
    run(repo_root=tmp_path, config="Debug", filter_pattern="SuiteA*",
        verbose=False, docker=False, run_all=False, shards=2)
    list_cmd = mock_run.call_args_list[0][0][0]
    assert any("SuiteA*" in arg for arg in list_cmd)


@patch("dia_cli.commands.test.shard_runner.subprocess.run")
def test_run_shards_composes_with_all(mock_run, tmp_path):
    _make_staged(tmp_path)
    list_output = "SLOW_Suite.\n  TestOne\n  TestTwo\n"
    mock_run.side_effect = [
        MagicMock(returncode=0, stdout=list_output),
        MagicMock(returncode=0),
        MagicMock(returncode=0),
    ]
    run(repo_root=tmp_path, config="Debug", filter_pattern=None,
        verbose=False, docker=False, run_all=True, shards=2)
    list_cmd = mock_run.call_args_list[0][0][0]
    # --all means no -SLOW_* filter injected into list command
    assert not any("-SLOW_*" in arg for arg in list_cmd)


@patch("dia_cli.commands.test.shard_runner.subprocess.run")
def test_run_shards_creates_merged_xml(mock_run, tmp_path):
    _make_staged(tmp_path)
    list_output = "SuiteA.\n  TestOne\n"
    mock_run.side_effect = [
        MagicMock(returncode=0, stdout=list_output),
        MagicMock(returncode=0),
    ]
    # Pre-create shard XML so merger can read it
    out_base = tmp_path / "Cluiche" / "out" / "GoogleTests"
    out_base.mkdir(parents=True, exist_ok=True)
    (out_base / "shard_0.xml").write_text(
        '<?xml version="1.0"?><testsuites><testsuite name="SuiteA" tests="1"/></testsuites>'
    )
    run(repo_root=tmp_path, config="Debug", filter_pattern=None,
        verbose=False, docker=False, run_all=False, shards=2)
    merged = tmp_path / "Cluiche" / "out" / "GoogleTests" / "merged.xml"
    assert merged.exists()


# ---------------------------------------------------------------------------
# --shards CLI option is present in help
# ---------------------------------------------------------------------------

def test_googletest_help_shows_shards():
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "googletest", "--help"])
    assert result.exit_code == 0
    assert "--shards" in result.output
