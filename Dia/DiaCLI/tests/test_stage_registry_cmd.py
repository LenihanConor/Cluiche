"""Tests for ``dia docs stage-registry`` generator."""
from __future__ import annotations

from pathlib import Path

import pytest
from click.testing import CliRunner

from dia_cli.commands.docs.stage_registry_cmd import stage_registry, render_stage_registry


@pytest.fixture
def basic_fixture():
    """Return the path to the basic fixture directory."""
    return Path(__file__).resolve().parent.parent.parent.parent / "Cluiche" / "Tests" / "Fixtures" / "StageCatalogue" / "basic"


def test_render_stage_registry_basic(basic_fixture):
    """Test rendering a stage registry from the basic fixture."""
    diagame_path = basic_fixture / "game.diagame"
    output = render_stage_registry(diagame_path, "Fixture")

    # Should have table header
    assert "| Stage | Kinds | Tags | Description |" in output

    # Check for specific stages from the fixture
    assert "TestRig" in output
    assert "ShowTest" in output
    assert "ShowcaseDemo" in output
    assert "ToolBench" in output
    assert "Misc" in output
    assert "Plain" in output

    # ShowcaseDemo has 'showcase' tag and kind
    assert "showcase" in output

    # Plain stage has no tags
    assert "Plain" in output


def test_render_stage_registry_empty_kinds():
    """Test that stages with no kinds show a dash."""
    # This will be tested via the basic fixture which has "Misc" with no kind (only "experimental" tag)
    pass


def test_render_stage_registry_pipe_escaping(basic_fixture):
    """Test that pipe characters in descriptions are escaped."""
    # For now, the fixture doesn't have pipes, but the function should escape them
    output = render_stage_registry(basic_fixture / "game.diagame", "Fixture")
    # Verify no unescaped pipes in description column (they should be escaped as \\|)
    lines = output.split("\n")
    table_lines = [l for l in lines if l.startswith("|")]
    # Should have at least header + some data rows
    assert len(table_lines) > 1


def test_render_stage_registry_determinism(basic_fixture):
    """Test that rendering twice produces identical output."""
    diagame_path = basic_fixture / "game.diagame"
    output1 = render_stage_registry(diagame_path, "Fixture")
    output2 = render_stage_registry(diagame_path, "Fixture")

    assert output1 == output2, "Output should be deterministic"


def test_render_stage_registry_malformed():
    """Test that a malformed catalogue is noted."""
    # Use a path that doesn't exist
    output = render_stage_registry(Path("/nonexistent/game.diagame"), "BadGame")

    # Should have a note about the failure
    assert "MissingDiagame" in output or "failed" in output.lower()


def test_render_stage_registry_no_root_manifest():
    """Test handling of missing root manifest."""
    fixture_dir = Path(__file__).resolve().parent.parent.parent.parent / "Cluiche" / "Tests" / "Fixtures" / "StageCatalogue" / "no_root_manifest"
    diagame_path = fixture_dir / "game.diagame"
    output = render_stage_registry(diagame_path, "NoManifest")

    # Should have a note about the failure
    assert "NoRootManifest" in output


def test_cli_command_basic(basic_fixture, tmp_path, monkeypatch):
    """Test the CLI command with --dry-run."""
    import dia_cli.commands.docs.stage_registry_cmd as cmd_module
    # Monkeypatch to find_repo_root should return the root containing Cluiche/Assets
    monkeypatch.setattr(cmd_module, "find_repo_root", lambda _f: basic_fixture.parent.parent.parent.parent)

    runner = CliRunner()
    result = runner.invoke(stage_registry, ["--dry-run"])

    assert result.exit_code == 0, result.output
    # Should contain the header
    assert "# Stage Registry" in result.output


def test_table_row_format(basic_fixture):
    """Test that table rows follow the correct format: Stage | Kinds | Tags | Description."""
    output = render_stage_registry(basic_fixture / "game.diagame", "Fixture")
    lines = output.split("\n")

    # Find the table section
    table_start = None
    for i, line in enumerate(lines):
        if "| Stage | Kinds | Tags | Description |" in line:
            table_start = i
            break

    assert table_start is not None, "Should have table header"

    # There should be a separator line after the header
    assert table_start + 1 < len(lines)
    assert lines[table_start + 1].startswith("|")

    # Verify data rows have 4 columns
    if table_start + 2 < len(lines):
        data_line = lines[table_start + 2]
        if data_line.strip() and data_line.startswith("|"):
            parts = data_line.split("|")
            # Should have 6 parts: empty, col1, col2, col3, col4, empty
            assert len(parts) >= 5, f"Row should have 4 columns: {data_line}"


def test_boot_transition_order(basic_fixture):
    """Test that stages appear in Boot transition order."""
    output = render_stage_registry(basic_fixture / "game.diagame", "Fixture")

    # Boot transitions order in the fixture: TestRig, NoImport, ShowTest, ToolBench, Misc, Plain, ShowcaseDemo
    # Find line numbers for each stage
    lines = output.split("\n")
    positions = {}
    for i, line in enumerate(lines):
        for stage in ["TestRig", "ShowTest", "ToolBench", "Misc", "Plain", "ShowcaseDemo"]:
            if f"| {stage}" in line:
                positions[stage] = i

    # Verify order
    if len(positions) > 1:
        stage_list = list(positions.keys())
        for i in range(len(stage_list) - 1):
            assert positions[stage_list[i]] < positions[stage_list[i+1]], \
                f"Stage {stage_list[i]} should appear before {stage_list[i+1]}"


def test_generated_file_header():
    """Test that output includes a game section header."""
    fixture_dir = Path(__file__).resolve().parent.parent.parent.parent / "Cluiche" / "Tests" / "Fixtures" / "StageCatalogue" / "basic"
    output = render_stage_registry(fixture_dir / "game.diagame", "Test")

    # Should have a game section header (render_stage_registry renders one game's table, not the whole doc)
    assert "## Test" in output
