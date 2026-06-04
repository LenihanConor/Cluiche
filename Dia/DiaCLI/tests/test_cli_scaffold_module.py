"""Tests for dia scaffold module command."""
from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.scaffold.module_cmd import _derive_names, module


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_VCXPROJ_SNIPPET = """\
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build">
  <ItemGroup>
    <ClCompile Include="Existing\\Existing.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="Existing\\Existing.h" />
  </ItemGroup>
</Project>
"""

_FILTERS_SNIPPET = """\
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0">
  <ItemGroup>
    <ClCompile Include="Existing\\Existing.cpp">
      <Filter>Existing</Filter>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="Existing\\Existing.h">
      <Filter>Existing</Filter>
    </ClInclude>
  </ItemGroup>
</Project>
"""


def _build_fake_repo(tmp_path: Path) -> Path:
    """Create minimum fake repo for scaffold module."""
    sln_dir = tmp_path / "Cluiche"
    sln_dir.mkdir(parents=True)
    (sln_dir / "Cluiche.sln").write_text("", encoding="utf-8")

    # Parent project
    parent_dir = tmp_path / "Dia" / "DiaCore"
    parent_dir.mkdir(parents=True)
    (parent_dir / "DiaCore.vcxproj").write_text(_VCXPROJ_SNIPPET, encoding="utf-8")
    (parent_dir / "DiaCore.vcxproj.filters").write_text(_FILTERS_SNIPPET, encoding="utf-8")
    (parent_dir / "Docs").mkdir()

    return tmp_path


# ---------------------------------------------------------------------------
# Name derivation tests
# ---------------------------------------------------------------------------

class TestDeriveNames:
    def test_simple(self):
        n = _derive_names("DiaCore", "Serializer")
        assert n["module_id"] == "dia.core.serializer"
        assert n["parent_id"] == "dia.core"
        assert n["namespace_parent"] == "Core"
        assert n["full_path"] == "Dia/DiaCore/Serializer/"
        assert n["header_file"] == "Serializer.h"
        assert n["impl_file"] == "Serializer.cpp"

    def test_animation_parent(self):
        n = _derive_names("DiaAnimation2D", "Timeline")
        assert n["module_id"] == "dia.animation2d.timeline"
        assert n["parent_id"] == "dia.animation2d"
        assert n["namespace_parent"] == "Animation2D"

    def test_module_md_filename(self):
        n = _derive_names("DiaCore", "Serializer")
        assert n["module_md_file"] == "dia.core.serializer.architecture.module.md"


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_module_command_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["scaffold", "--help"])
    assert result.exit_code == 0
    assert "module" in result.output


def test_module_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["scaffold", "module", "--help"])
    assert result.exit_code == 0
    assert "PARENT" in result.output
    assert "NAME" in result.output
    assert "--layer" in result.output
    assert "--dry-run" in result.output


# ---------------------------------------------------------------------------
# Dry-run tests
# ---------------------------------------------------------------------------

class TestDryRun:
    def test_exits_zero(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "module", "DiaCore", "Widget", "--dry-run"])
        assert result.exit_code == 0

    def test_prints_header_path(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "module", "DiaCore", "Widget", "--dry-run"])
        assert "Widget.h" in result.output

    def test_prints_impl_path(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "module", "DiaCore", "Widget", "--dry-run"])
        assert "Widget.cpp" in result.output

    def test_prints_module_md_path(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "module", "DiaCore", "Widget", "--dry-run"])
        assert "dia.core.widget.architecture.module.md" in result.output

    def test_prints_vcxproj(self):
        runner = CliRunner()
        result = runner.invoke(cli, ["scaffold", "module", "DiaCore", "Widget", "--dry-run"])
        assert "DiaCore.vcxproj" in result.output

    def test_creates_no_files(self, tmp_path):
        with patch("dia_cli.commands.scaffold.module_cmd.find_repo_root", return_value=tmp_path):
            runner = CliRunner()
            result = runner.invoke(cli, ["scaffold", "module", "DiaCore", "Widget", "--dry-run"])
        assert result.exit_code == 0
        all_files = list(tmp_path.rglob("*"))
        assert all_files == []


# ---------------------------------------------------------------------------
# Full-run tests
# ---------------------------------------------------------------------------

class TestFullRun:
    def _invoke(self, tmp_path: Path, parent="DiaCore", name="Widget", layer="platform"):
        repo = _build_fake_repo(tmp_path)
        with patch("dia_cli.commands.scaffold.module_cmd.find_repo_root", return_value=repo):
            runner = CliRunner()
            args = ["scaffold", "module", parent, name, "--layer", layer]
            result = runner.invoke(cli, args)
        return result, repo

    def test_exits_zero(self, tmp_path):
        result, _ = self._invoke(tmp_path)
        assert result.exit_code == 0, result.output

    def test_header_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        assert (repo / "Dia/DiaCore/Widget/Widget.h").exists()

    def test_impl_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        assert (repo / "Dia/DiaCore/Widget/Widget.cpp").exists()

    def test_module_md_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        assert (repo / "Dia/DiaCore/Docs/dia.core.widget.architecture.module.md").exists()

    def test_header_has_namespace(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaCore/Widget/Widget.h").read_text(encoding="utf-8")
        assert "namespace Core" in text
        assert "class Widget" in text

    def test_impl_includes_header(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaCore/Widget/Widget.cpp").read_text(encoding="utf-8")
        assert '#include "DiaCore/Widget/Widget.h"' in text

    def test_module_md_has_module_id(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaCore/Docs/dia.core.widget.architecture.module.md").read_text(encoding="utf-8")
        assert "module_id: dia.core.widget" in text

    def test_module_md_has_layer(self, tmp_path):
        _, repo = self._invoke(tmp_path, layer="framework")
        text = (repo / "Dia/DiaCore/Docs/dia.core.widget.architecture.module.md").read_text(encoding="utf-8")
        assert "layer: framework" in text

    def test_vcxproj_updated(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaCore/DiaCore.vcxproj").read_text(encoding="utf-8")
        assert "Widget\\Widget.cpp" in text
        assert "Widget\\Widget.h" in text

    def test_vcxproj_filters_updated(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        text = (repo / "Dia/DiaCore/DiaCore.vcxproj.filters").read_text(encoding="utf-8")
        assert "Widget\\Widget.cpp" in text
        assert "Widget\\Widget.h" in text
        assert "<Filter>Widget</Filter>" in text

    def test_prints_next_hint(self, tmp_path):
        result, _ = self._invoke(tmp_path)
        assert "Next:" in result.output
        assert "module-registry.md" in result.output
