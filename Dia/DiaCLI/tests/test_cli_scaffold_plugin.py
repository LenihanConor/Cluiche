"""Tests for dia scaffold plugin command."""
from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.scaffold.plugin_cmd import _derive_names, plugin


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _build_fake_repo(tmp_path: Path) -> Path:
    """Create minimum fake repo for scaffold plugin."""
    sln_dir = tmp_path / "Cluiche"
    sln_dir.mkdir(parents=True)
    (sln_dir / "Cluiche.sln").write_text("", encoding="utf-8")
    return tmp_path


# ---------------------------------------------------------------------------
# Name derivation tests
# ---------------------------------------------------------------------------

class TestDeriveNames:
    def test_pipeline_editor(self):
        n = _derive_names("PipelineEditor")
        assert n["project_name"] == "DiaPipelineEditor"
        assert n["plugin_class"] == "DiaPipelineEditorPlugin"
        assert n["namespace_name"] == "PipelineEditor"
        assert n["plugin_id_lower"] == "pipelineeditor"
        assert n["ui_path"] == "dia://plugins/pipelineeditor/index.html"

    def test_audio_mixer(self):
        n = _derive_names("AudioMixer")
        assert n["project_name"] == "DiaAudioMixer"
        assert n["plugin_class"] == "DiaAudioMixerPlugin"

    def test_simple_name(self):
        n = _derive_names("Profiler")
        assert n["project_name"] == "DiaProfiler"
        assert n["ui_path"] == "dia://plugins/profiler/index.html"


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_plugin_command_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["scaffold", "--help"])
    assert result.exit_code == 0
    assert "plugin" in result.output


def test_plugin_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["scaffold", "plugin", "--help"])
    assert result.exit_code == 0
    assert "NAME" in result.output
    assert "--layout" in result.output
    assert "--dry-run" in result.output


# ---------------------------------------------------------------------------
# Dry-run tests
# ---------------------------------------------------------------------------

class TestDryRun:
    def test_exits_zero(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "plugin", "TestPlugin", "--dry-run"])
        assert result.exit_code == 0

    def test_prints_header(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "plugin", "TestPlugin", "--dry-run"])
        assert "DiaTestPluginPlugin.h" in result.output

    def test_prints_impl(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "plugin", "TestPlugin", "--dry-run"])
        assert "DiaTestPluginPlugin.cpp" in result.output

    def test_prints_ui(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "plugin", "TestPlugin", "--dry-run"])
        assert "UI/index.html" in result.output

    def test_prints_vcxproj(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "plugin", "TestPlugin", "--dry-run"])
        assert "DiaTestPlugin.vcxproj" in result.output

    def test_prints_next_hint(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "plugin", "TestPlugin", "--dry-run"])
        assert "Cluiche.sln" in result.output
        assert "PluginLoaderModule.cpp" in result.output

    def test_creates_no_files(self, tmp_path):
        with patch("dia_cli.commands.scaffold.plugin_cmd.find_repo_root", return_value=tmp_path):
            runner = CliRunner()
            result = runner.invoke(cli, ["scaffold", "plugin", "TestPlugin", "--dry-run"])
        assert result.exit_code == 0
        all_files = list(tmp_path.rglob("*"))
        assert all_files == []


# ---------------------------------------------------------------------------
# Full-run tests
# ---------------------------------------------------------------------------

class TestFullRun:
    def _invoke(self, tmp_path: Path, name="TestEditor", layout="dockable"):
        repo = _build_fake_repo(tmp_path)
        with patch("dia_cli.commands.scaffold.plugin_cmd.find_repo_root", return_value=repo):
            runner = CliRunner()
            args = ["scaffold", "plugin", name, "--layout", layout]
            result = runner.invoke(cli, args)
        return result, repo

    def test_exits_zero(self, tmp_path):
        result, _ = self._invoke(tmp_path)
        assert result.exit_code == 0, result.output

    def test_header_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        assert (repo / "Dia/DiaTestEditor/DiaTestEditorPlugin.h").exists()

    def test_impl_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        assert (repo / "Dia/DiaTestEditor/DiaTestEditorPlugin.cpp").exists()

    def test_ui_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        assert (repo / "Dia/DiaTestEditor/UI/index.html").exists()

    def test_vcxproj_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        assert (repo / "Dia/DiaTestEditor/DiaTestEditor.vcxproj").exists()

    def test_filters_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        assert (repo / "Dia/DiaTestEditor/DiaTestEditor.vcxproj.filters").exists()

    # --- content checks ---

    def test_header_has_plugin_class(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaTestEditor/DiaTestEditorPlugin.h").read_text(encoding="utf-8")
        assert "class DiaTestEditorPlugin" in text

    def test_header_has_ui_path(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaTestEditor/DiaTestEditorPlugin.h").read_text(encoding="utf-8")
        assert "dia://plugins/testeditor/index.html" in text

    def test_header_dockable_layout(self, tmp_path):
        _, repo = self._invoke(tmp_path, layout="dockable")
        text = (repo / "Dia/DiaTestEditor/DiaTestEditorPlugin.h").read_text(encoding="utf-8")
        assert "kDockable" in text

    def test_header_fullscreen_layout(self, tmp_path):
        _, repo = self._invoke(tmp_path, name="FullEditor", layout="fullscreen")
        text = (repo / "Dia/DiaFullEditor/DiaFullEditorPlugin.h").read_text(encoding="utf-8")
        assert "kFullScreen" in text

    def test_impl_has_register_macro(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaTestEditor/DiaTestEditorPlugin.cpp").read_text(encoding="utf-8")
        assert 'REGISTER_EDITOR_PLUGIN(DiaTestEditorPlugin, "DiaTestEditor")' in text

    def test_impl_has_onload(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaTestEditor/DiaTestEditorPlugin.cpp").read_text(encoding="utf-8")
        assert "DiaTestEditorPlugin::OnLoad" in text

    def test_ui_has_title(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaTestEditor/UI/index.html").read_text(encoding="utf-8")
        assert "<title>DiaTestEditor</title>" in text

    def test_vcxproj_has_project_name(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaTestEditor/DiaTestEditor.vcxproj").read_text(encoding="utf-8")
        assert "<ProjectName>DiaTestEditor</ProjectName>" in text

    def test_vcxproj_has_guid(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaTestEditor/DiaTestEditor.vcxproj").read_text(encoding="utf-8")
        assert "<ProjectGuid>" in text

    def test_filters_has_source_filter(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaTestEditor/DiaTestEditor.vcxproj.filters").read_text(encoding="utf-8")
        assert '<Filter Include="Source">' in text

    def test_guid_is_deterministic(self, tmp_path):
        _, repo1 = self._invoke(tmp_path, name="Stable")
        text1 = (repo1 / "Dia/DiaStable/DiaStable.vcxproj").read_text(encoding="utf-8")
        # Run again in a new tmp
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            _, repo2 = self._invoke(Path(td), name="Stable")
            text2 = (repo2 / "Dia/DiaStable/DiaStable.vcxproj").read_text(encoding="utf-8")
        assert text1 == text2
