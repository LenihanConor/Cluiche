"""Tests for dia check deps command."""
from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.cli.cli_check import _parse_frontmatter, _scan_includes


# ---------------------------------------------------------------------------
# Unit tests: frontmatter parser
# ---------------------------------------------------------------------------

class TestParseFrontmatter:
    def test_basic_fields(self):
        text = """\
---
schema: dia.module.v1
module_id: dia.core.containers
path: Dia/DiaCore/Containers/
dependent_modules:
  - dia.core.type
  - dia.core.crc
---
# Containers
"""
        fm = _parse_frontmatter(text)
        assert fm["module_id"] == "dia.core.containers"
        assert fm["path"] == "Dia/DiaCore/Containers/"
        assert fm["dependent_modules"] == ["dia.core.type", "dia.core.crc"]

    def test_empty_deps(self):
        text = """\
---
module_id: dia.core.crc
path: Dia/DiaCore/CRC/
dependent_modules: []
---
"""
        fm = _parse_frontmatter(text)
        assert fm["dependent_modules"] == []

    def test_no_frontmatter(self):
        text = "# Just a markdown file\n"
        fm = _parse_frontmatter(text)
        assert fm == {}

    def test_quoted_values(self):
        text = """\
---
module_id: "dia.core.json"
path: 'Dia/DiaCore/Json/'
dependent_modules: []
---
"""
        fm = _parse_frontmatter(text)
        assert fm["module_id"] == "dia.core.json"
        assert fm["path"] == "Dia/DiaCore/Json/"


# ---------------------------------------------------------------------------
# Unit tests: include scanner
# ---------------------------------------------------------------------------

class TestScanIncludes:
    def test_angle_brackets(self, tmp_path):
        src = tmp_path / "test.cpp"
        src.write_text('#include <DiaCore/Containers/Array.h>\n', encoding="utf-8")
        includes = _scan_includes(src)
        assert "DiaCore/Containers/Array.h" in includes

    def test_quotes(self, tmp_path):
        src = tmp_path / "test.cpp"
        src.write_text('#include "DiaCore/CRC/StringCRC.h"\n', encoding="utf-8")
        includes = _scan_includes(src)
        assert "DiaCore/CRC/StringCRC.h" in includes

    def test_multiple(self, tmp_path):
        src = tmp_path / "test.h"
        src.write_text(
            '#include <DiaCore/Type/TypeId.h>\n'
            '#include "DiaMaths/Vector/Vector2D.h"\n',
            encoding="utf-8"
        )
        includes = _scan_includes(src)
        assert "DiaCore/Type/TypeId.h" in includes
        assert "DiaMaths/Vector/Vector2D.h" in includes

    def test_nonexistent_file(self, tmp_path):
        includes = _scan_includes(tmp_path / "missing.cpp")
        assert includes == set()


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_check_command_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["check", "--help"])
    assert result.exit_code == 0
    assert "deps" in result.output


def test_check_deps_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["check", "deps", "--help"])
    assert result.exit_code == 0
    assert "--fix" in result.output
    assert "--verbose" in result.output


# ---------------------------------------------------------------------------
# Integration: fake repo
# ---------------------------------------------------------------------------

def _build_fake_repo(tmp_path: Path) -> Path:
    """Build a minimal repo with two modules for dep checking."""
    sln_dir = tmp_path / "Cluiche"
    sln_dir.mkdir(parents=True)
    (sln_dir / "Cluiche.sln").write_text("", encoding="utf-8")

    # Module A: dia.core.containers at Dia/DiaCore/Containers/
    mod_a_dir = tmp_path / "Dia" / "DiaCore" / "Containers"
    mod_a_dir.mkdir(parents=True)
    (mod_a_dir / "Array.h").write_text('#pragma once\nclass Array {};\n', encoding="utf-8")

    docs_dir = tmp_path / "Dia" / "DiaCore" / "Docs"
    docs_dir.mkdir(parents=True)
    (docs_dir / "dia.core.containers.architecture.module.md").write_text("""\
---
module_id: dia.core.containers
path: Dia/DiaCore/Containers/
dependent_modules: []
---
""", encoding="utf-8")

    # Module B: dia.maths.vector at Dia/DiaMaths/Vector/
    mod_b_dir = tmp_path / "Dia" / "DiaMaths" / "Vector"
    mod_b_dir.mkdir(parents=True)
    # This module includes from DiaCore/Containers/ but doesn't declare the dep
    (mod_b_dir / "Vec2.h").write_text('#include <DiaCore/Containers/Array.h>\n', encoding="utf-8")

    maths_docs = tmp_path / "Dia" / "DiaMaths" / "Docs"
    maths_docs.mkdir(parents=True)
    (maths_docs / "dia.maths.vector.architecture.module.md").write_text("""\
---
module_id: dia.maths.vector
path: Dia/DiaMaths/Vector/
dependent_modules: []
---
""", encoding="utf-8")

    return tmp_path


class TestCheckDepsIntegration:
    def test_finds_missing_dep(self, tmp_path):
        repo = _build_fake_repo(tmp_path)
        with patch("dia_cli.cli.cli_check.find_repo_root", return_value=repo):
            runner = CliRunner()
            result = runner.invoke(cli, ["check", "deps"])
        assert result.exit_code == 0
        assert "MISSING DEPENDENCY" in result.output
        assert "dia.maths.vector" in result.output
        assert "dia.core.containers" in result.output

    def test_verbose_shows_modules(self, tmp_path):
        repo = _build_fake_repo(tmp_path)
        with patch("dia_cli.cli.cli_check.find_repo_root", return_value=repo):
            runner = CliRunner()
            result = runner.invoke(cli, ["check", "deps", "--verbose"])
        assert "dia.core.containers" in result.output
        assert "dia.maths.vector" in result.output

    def test_clean_repo_reports_ok(self, tmp_path):
        """When deps are correctly declared, report OK."""
        repo = _build_fake_repo(tmp_path)
        # Fix the dep
        md = repo / "Dia/DiaMaths/Docs/dia.maths.vector.architecture.module.md"
        md.write_text("""\
---
module_id: dia.maths.vector
path: Dia/DiaMaths/Vector/
dependent_modules:
  - dia.core.containers
---
""", encoding="utf-8")
        with patch("dia_cli.cli.cli_check.find_repo_root", return_value=repo):
            runner = CliRunner()
            result = runner.invoke(cli, ["check", "deps"])
        assert "OK" in result.output
        assert "MISSING" not in result.output
