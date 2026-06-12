"""Unit tests for the bgfx shader cook: multi-root config loading + collision-free
output path computation.

Two concerns are covered:
  1. pipeline_config: source_roots list, legacy source_root back-compat, default fallback.
  2. cook_bgfx_shaders: two roots with same-named .sc files produce distinct output
     paths (the relative_to(root) logic that prevents 3d/ collisions), with shaderc
     stubbed out so no GPU / real binary is required.
"""
from __future__ import annotations

import textwrap
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from dia_cli.commands.pipeline.pipeline_config import (
    load_pipeline_config,
    BgfxShadersConfig,
    _DEFAULT_BGFX_SOURCE_ROOTS,
)
from dia_cli.commands.pipeline.bgfx_shader_cook import cook_bgfx_shaders


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_BASE_TOML = """\
[global]
default_config = "Debug"
default_platform = "x64"
default_target = "googletest"

[proto]
proto_dir = "Dia/DiaDebugProtocol/proto"

[targets.googletest]
project = "Cluiche/Tests/GoogleTests/GoogleTests.vcxproj"
stages = ["compile-code"]
"""


def _write_toml(tmp_path: Path, bgfx_section: str) -> Path:
    (tmp_path / "pipeline.toml").write_text(_BASE_TOML + bgfx_section)
    return tmp_path


# ---------------------------------------------------------------------------
# Config loading
# ---------------------------------------------------------------------------

def test_source_roots_list_loads_verbatim(tmp_path: Path):
    repo = _write_toml(tmp_path, textwrap.dedent("""\
        [bgfx_shaders]
        source_roots = ["Dia/DiaBgfx/Shaders", "Dia/DiaBgfx3D/Shaders"]
    """))

    cfg = load_pipeline_config(repo)

    assert cfg.bgfx_shaders.source_roots == [
        "Dia/DiaBgfx/Shaders",
        "Dia/DiaBgfx3D/Shaders",
    ]


def test_legacy_source_root_string_maps_to_list(tmp_path: Path):
    # Old configs used a single source_root string; loader must still accept it.
    repo = _write_toml(tmp_path, textwrap.dedent("""\
        [bgfx_shaders]
        source_root = "Dia/DiaBgfx/Shaders"
    """))

    cfg = load_pipeline_config(repo)

    assert cfg.bgfx_shaders.source_roots == ["Dia/DiaBgfx/Shaders"]


def test_missing_bgfx_section_uses_default_roots(tmp_path: Path):
    repo = _write_toml(tmp_path, "")  # no [bgfx_shaders] table at all

    cfg = load_pipeline_config(repo)

    assert cfg.bgfx_shaders.source_roots == list(_DEFAULT_BGFX_SOURCE_ROOTS)


def test_source_roots_takes_precedence_over_legacy(tmp_path: Path):
    # If both keys are present, the list wins (legacy is ignored).
    repo = _write_toml(tmp_path, textwrap.dedent("""\
        [bgfx_shaders]
        source_root = "Dia/Legacy/Shaders"
        source_roots = ["Dia/DiaBgfx/Shaders", "Dia/DiaBgfx3D/Shaders"]
    """))

    cfg = load_pipeline_config(repo)

    assert cfg.bgfx_shaders.source_roots == [
        "Dia/DiaBgfx/Shaders",
        "Dia/DiaBgfx3D/Shaders",
    ]


# ---------------------------------------------------------------------------
# Cook path computation (shaderc stubbed)
# ---------------------------------------------------------------------------

def _make_repo_with_two_roots(tmp_path: Path) -> Path:
    """Create a repo with two shader roots, each holding a vs_mesh.sc, plus a
    fake shaderc.exe. The DiaBgfx3D shader lives under a 3d/ subdir so the
    relative_to(root) logic must preserve that subdir in the output path."""
    # Fake shaderc binary (existence + stat is all the cook needs before subprocess).
    shaderc = tmp_path / "External" / "bgfx" / "tools" / "shaderc.exe"
    shaderc.parent.mkdir(parents=True, exist_ok=True)
    shaderc.write_bytes(b"\x00")

    root2d = tmp_path / "Dia" / "DiaBgfx" / "Shaders"
    root2d.mkdir(parents=True, exist_ok=True)
    (root2d / "vs_mesh.sc").write_text("// 2d vs_mesh")

    root3d = tmp_path / "Dia" / "DiaBgfx3D" / "Shaders" / "3d"
    root3d.mkdir(parents=True, exist_ok=True)
    (root3d / "vs_mesh.sc").write_text("// 3d vs_mesh")

    return tmp_path


def test_two_roots_same_filename_produce_distinct_output_paths(tmp_path: Path):
    repo = _make_repo_with_two_roots(tmp_path)

    cfg = BgfxShadersConfig(
        backends=["dx11"],
        source_roots=["Dia/DiaBgfx/Shaders", "Dia/DiaBgfx3D/Shaders"],
        output_root="out/$(AppName)/shaders",
        shaderc_path="External/bgfx/tools/shaderc.exe",
    )

    written_outputs: list[Path] = []

    def fake_run(cmd, *args, **kwargs):
        # cmd is the shaderc argv; find the -o output path and create the file
        # so the cook's post-conditions (sentinel write) succeed.
        out_idx = cmd.index("-o") + 1
        out_path = Path(cmd[out_idx])
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_bytes(b"\x00")
        written_outputs.append(out_path)
        result = MagicMock()
        result.returncode = 0
        result.stderr = ""
        return result

    with patch("dia_cli.commands.pipeline.bgfx_shader_cook.subprocess.run", side_effect=fake_run):
        rc = cook_bgfx_shaders(
            cfg=cfg,
            app_name="TestApp",
            build_config="Debug",
            platform="x64",
            force=True,
            repo_root=repo,
        )

    assert rc == 0
    # Both shaders cooked — no collision, no overwrite.
    assert len(written_outputs) == 2

    rels = sorted(str(p.relative_to(repo)).replace("\\", "/") for p in written_outputs)
    # 2D shader -> dx11/vs_mesh.bin ; 3D shader -> dx11/3d/vs_mesh.bin (subdir preserved)
    assert rels == [
        "out/TestApp/shaders/dx11/3d/vs_mesh.bin",
        "out/TestApp/shaders/dx11/vs_mesh.bin",
    ]


def test_missing_source_root_is_skipped_not_fatal(tmp_path: Path):
    repo = _make_repo_with_two_roots(tmp_path)

    # Second root does not exist on disk — cook must skip it, still cook the first.
    cfg = BgfxShadersConfig(
        backends=["dx11"],
        source_roots=["Dia/DiaBgfx/Shaders", "Dia/DoesNotExist/Shaders"],
        output_root="out/$(AppName)/shaders",
        shaderc_path="External/bgfx/tools/shaderc.exe",
    )

    written_outputs: list[Path] = []

    def fake_run(cmd, *args, **kwargs):
        out_path = Path(cmd[cmd.index("-o") + 1])
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_bytes(b"\x00")
        written_outputs.append(out_path)
        result = MagicMock()
        result.returncode = 0
        result.stderr = ""
        return result

    with patch("dia_cli.commands.pipeline.bgfx_shader_cook.subprocess.run", side_effect=fake_run):
        rc = cook_bgfx_shaders(
            cfg=cfg,
            app_name="TestApp",
            build_config="Debug",
            platform="x64",
            force=True,
            repo_root=repo,
        )

    assert rc == 0
    assert len(written_outputs) == 1  # only the existing root's shader
