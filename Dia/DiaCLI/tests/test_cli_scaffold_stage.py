"""Tests for dia scaffold stage command."""
from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.scaffold.stage_cmd import _derive_names, stage


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_CLUICHE_MAIN_DIAAPP = {
    "version": 3,
    "stages": [
        {"name": "Boot", "transitions": [], "auto_advance": False}
    ],
    "initial_stage": "Boot",
    "streams": [],
    "processing_units": [
        {
            "instance_id": "MainPU",
            "frequency_hz": 30,
            "dedicated_thread": False,
            "modules": []
        },
        {
            "instance_id": "RenderPU",
            "frequency_hz": 60,
            "dedicated_thread": True,
            "modules": [
                {
                    "instance_id": "TestStageHUDModule",
                    "type_id": "TestStageHUDModule",
                    "stages": ["DummyStage"],
                    "dependencies": ["DebugUI"],
                    "channels": []
                }
            ]
        }
    ]
}

_DIAGAME = {
    "config": {},
    "imports": [
        {"path": "global/misc/ApplicationFlow/cluiche_main.diaapp", "type": "manifest"}
    ],
    "name": "CluicheTest",
    "version": "0.1"
}

_CATALOGUE = {
    "version": 1,
    "assets": []
}

_VCXPROJ_SNIPPET = """\
  <ItemGroup>
    <ClCompile Include="Modules\\TestStages\\TestStageHUDModule.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="Modules\\TestStages\\TestStageHUDModule.h" />
  </ItemGroup>
"""

_FILTERS_SNIPPET = """\
  <ItemGroup>
    <ClCompile Include="Modules\\TestStages\\TestStageHUDModule.cpp">
      <Filter>ApplicationFlow\\Modules\\TestStages</Filter>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="Modules\\TestStages\\TestStageHUDModule.h">
      <Filter>ApplicationFlow\\Modules\\TestStages</Filter>
    </ClInclude>
  </ItemGroup>
"""


def _build_fake_repo(tmp_path: Path) -> Path:
    """Create minimum fake repo structure that scaffold stage expects to find."""
    # Repo root marker
    sln_dir = tmp_path / "Cluiche"
    sln_dir.mkdir(parents=True)
    (sln_dir / "Cluiche.sln").write_text("", encoding="utf-8")

    # cluiche_main.diaapp
    main_diaapp_dir = (
        tmp_path / "Cluiche" / "Assets" / "CluicheTest" / "Global" / "misc" / "ApplicationFlow"
    )
    main_diaapp_dir.mkdir(parents=True)
    (main_diaapp_dir / "cluiche_main.diaapp").write_text(
        json.dumps(_CLUICHE_MAIN_DIAAPP, indent=4), encoding="utf-8"
    )

    # diagame
    game_dir = tmp_path / "Cluiche" / "Assets" / "CluicheTest"
    game_dir.mkdir(parents=True, exist_ok=True)
    (game_dir / "cluichetest.diagame").write_text(
        json.dumps(_DIAGAME, indent=4), encoding="utf-8"
    )

    # catalogue
    (game_dir / "assets.catalogue.json").write_text(
        json.dumps(_CATALOGUE, indent=4), encoding="utf-8"
    )

    # vcxproj + filters
    ct_dir = tmp_path / "Cluiche" / "CluicheTest"
    ct_dir.mkdir(parents=True)
    (ct_dir / "CluicheTest.vcxproj").write_text(_VCXPROJ_SNIPPET, encoding="utf-8")
    (ct_dir / "CluicheTest.vcxproj.filters").write_text(_FILTERS_SNIPPET, encoding="utf-8")

    return tmp_path


# ---------------------------------------------------------------------------
# Name derivation unit tests
# ---------------------------------------------------------------------------

class TestDeriveNames:
    def test_simple_name(self):
        n = _derive_names("RigidBody2D")
        assert n["snake"] == "rigidbody2d"
        assert n["stage_name"] == "RigidBody2DTestStage"
        assert n["snake_stage"] == "rigidbody2d_test_stage"
        assert n["stage_id"] == "stage.rigidbody2d_test_stage"
        assert n["module_name"] == "RigidBody2DTestStageModule"
        assert n["diastage_file"] == "rigidbody2d_test_stage.diastage"
        assert n["diaapp_file"] == "rigidbody2d_test_stage.diaapp"

    def test_entity_name(self):
        n = _derive_names("Entity")
        assert n["snake"] == "entity"
        assert n["stage_name"] == "EntityTestStage"
        assert n["module_name"] == "EntityTestStageModule"

    def test_animation_name(self):
        n = _derive_names("Animation2D")
        assert n["snake"] == "animation2d"
        assert n["module_name"] == "Animation2DTestStageModule"


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_scaffold_command_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["scaffold", "--help"])
    assert result.exit_code == 0
    assert "stage" in result.output


def test_scaffold_stage_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["scaffold", "stage", "--help"])
    assert result.exit_code == 0
    assert "NAME" in result.output
    assert "--modules" in result.output
    assert "--dry-run" in result.output
    assert "--budget" in result.output


# ---------------------------------------------------------------------------
# dry-run: exits 0, lists files, creates nothing
# ---------------------------------------------------------------------------

class TestDryRun:
    def _invoke_dry_run(self, name="TestEntity", extra=""):
        runner = CliRunner()
        args = ["scaffold", "stage", name, "--dry-run"]
        if extra:
            args += ["--modules", extra]
        result = runner.invoke(cli, args)
        return result

    def test_dry_run_exits_zero(self):
        result = self._invoke_dry_run()
        assert result.exit_code == 0, result.output

    def test_dry_run_prints_diastage_path(self):
        result = self._invoke_dry_run()
        assert "testentity_test_stage.diastage" in result.output

    def test_dry_run_prints_diaapp_path(self):
        result = self._invoke_dry_run()
        assert "testentity_test_stage.diaapp" in result.output

    def test_dry_run_prints_header_path(self):
        result = self._invoke_dry_run()
        assert "TestEntityTestStageModule.h" in result.output

    def test_dry_run_prints_impl_path(self):
        result = self._invoke_dry_run()
        assert "TestEntityTestStageModule.cpp" in result.output

    def test_dry_run_prints_nine_paths(self):
        result = self._invoke_dry_run()
        lines = [l for l in result.output.splitlines() if "Create" in l or "Update" in l]
        assert len(lines) == 9

    def test_dry_run_creates_no_files(self, tmp_path):
        # Patch find_repo_root to point at a temp dir so even if it tries to write it fails cleanly
        with patch("dia_cli.commands.scaffold.stage_cmd.find_repo_root", return_value=tmp_path):
            runner = CliRunner()
            result = runner.invoke(cli, ["scaffold", "stage", "TestEntity", "--dry-run"])
        assert result.exit_code == 0
        # No new files in tmp_path beyond what we put there
        all_files = list(tmp_path.rglob("*"))
        assert all_files == []


# ---------------------------------------------------------------------------
# Full run: all files created with correct content
# ---------------------------------------------------------------------------

class TestFullRun:
    def _invoke(self, tmp_path: Path, name="TestEntity", modules_opt=None, budget=None):
        repo = _build_fake_repo(tmp_path)
        with patch("dia_cli.commands.scaffold.stage_cmd.find_repo_root", return_value=repo):
            runner = CliRunner()
            args = ["scaffold", "stage", name]
            if modules_opt:
                args += ["--modules", modules_opt]
            if budget is not None:
                args += ["--budget", str(budget)]
            result = runner.invoke(cli, args)
        return result, repo

    def test_exits_zero(self, tmp_path):
        result, _ = self._invoke(tmp_path)
        assert result.exit_code == 0, result.output

    def test_prints_created_summary(self, tmp_path):
        result, _ = self._invoke(tmp_path)
        assert "Created" in result.output
        assert "Updated" in result.output

    def test_prints_next_hint(self, tmp_path):
        result, _ = self._invoke(tmp_path)
        assert "Next:" in result.output
        assert "TestEntityTestStageModule" in result.output

    # --- file existence ---

    def test_diastage_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/Assets/Stages/TestEntityTestStage/testentity_test_stage.diastage"
        assert p.exists()

    def test_diaapp_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/Stages/TestEntityTestStage/misc/ApplicationFlow"
            / "testentity_test_stage.diaapp"
        )
        assert p.exists()

    def test_header_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.h"
        assert p.exists()

    def test_impl_created(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.cpp"
        assert p.exists()

    # --- diastage content ---

    def test_diastage_name_field(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/Assets/Stages/TestEntityTestStage/testentity_test_stage.diastage"
        data = json.loads(p.read_text(encoding="utf-8"))
        assert data["name"] == "TestEntityTestStage"

    def test_diastage_manifest_field(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/Assets/Stages/TestEntityTestStage/testentity_test_stage.diastage"
        data = json.loads(p.read_text(encoding="utf-8"))
        assert "testentity_test_stage.diaapp" in data["manifest"]

    # --- diaapp content ---

    def test_diaapp_version(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/Stages/TestEntityTestStage/misc/ApplicationFlow"
            / "testentity_test_stage.diaapp"
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        assert data["version"] == 3

    def test_diaapp_always_includes_automation_module(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/Stages/TestEntityTestStage/misc/ApplicationFlow"
            / "testentity_test_stage.diaapp"
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        main_pu = next(pu for pu in data["processing_units"] if pu["instance_id"] == "MainPU")
        mod = main_pu["modules"][0]
        assert "AutomationModule" in mod["dependencies"]

    def test_diaapp_module_type_id(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/Stages/TestEntityTestStage/misc/ApplicationFlow"
            / "testentity_test_stage.diaapp"
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        main_pu = next(pu for pu in data["processing_units"] if pu["instance_id"] == "MainPU")
        mod = main_pu["modules"][0]
        assert mod["type_id"] == "TestEntityTestStageModule"

    def test_diaapp_has_sim_pu(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/Stages/TestEntityTestStage/misc/ApplicationFlow"
            / "testentity_test_stage.diaapp"
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        sim_pu = next(
            (pu for pu in data["processing_units"] if pu["instance_id"] == "SimPU"), None
        )
        assert sim_pu is not None
        assert sim_pu["dedicated_thread"] is True

    # --- header content ---

    def test_header_class_declaration(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.h"
        text = p.read_text(encoding="utf-8")
        assert "class TestEntityTestStageModule" in text

    def test_header_includes_base(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.h"
        text = p.read_text(encoding="utf-8")
        assert "TestStageModuleBase.h" in text

    def test_header_budget_default(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.h"
        text = p.read_text(encoding="utf-8")
        assert "return 300;" in text

    def test_header_budget_custom(self, tmp_path):
        result, repo = self._invoke(tmp_path, budget=600)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.h"
        text = p.read_text(encoding="utf-8")
        assert "return 600;" in text

    # --- impl content ---

    def test_impl_type_id(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.cpp"
        text = p.read_text(encoding="utf-8")
        assert 'kTypeId("TestEntityTestStageModule")' in text

    def test_impl_get_stage_name(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.cpp"
        text = p.read_text(encoding="utf-8")
        assert '"TestEntityTestStage"' in text

    def test_impl_checkpoint_name(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.cpp"
        text = p.read_text(encoding="utf-8")
        assert '"test.testentity.passed"' in text

    def test_impl_dia_module_macro(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/Modules/TestStages/TestEntityTestStageModule.cpp"
        text = p.read_text(encoding="utf-8")
        assert "DIA_MODULE(" in text

    # --- cluiche_main.diaapp updates ---

    def test_main_diaapp_stage_added(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp"
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        stage_names = [s["name"] for s in data["stages"]]
        assert "TestEntityTestStage" in stage_names

    def test_main_diaapp_hud_module_updated(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp"
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        render_pu = next(pu for pu in data["processing_units"] if pu["instance_id"] == "RenderPU")
        hud = next(m for m in render_pu["modules"] if m["instance_id"] == "TestStageHUDModule")
        assert "TestEntityTestStage" in hud["stages"]

    def test_main_diaapp_boot_transitions_updated(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp"
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        boot = next(s for s in data["stages"] if s["name"] == "Boot")
        assert "TestEntityTestStage" in boot["transitions"]

    def test_main_diaapp_stage_back_transition_to_boot(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = (
            repo
            / "Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp"
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        new_stage = next(s for s in data["stages"] if s["name"] == "TestEntityTestStage")
        assert "Boot" in new_stage["transitions"]

    # --- diagame updates ---

    def test_diagame_import_added(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/Assets/CluicheTest/cluichetest.diagame"
        data = json.loads(p.read_text(encoding="utf-8"))
        import_paths = [i["path"] for i in data["imports"]]
        assert any("testentity_test_stage.diastage" in path for path in import_paths)

    def test_diagame_import_type_is_stage(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/Assets/CluicheTest/cluichetest.diagame"
        data = json.loads(p.read_text(encoding="utf-8"))
        new_import = next(
            i for i in data["imports"] if "testentity_test_stage.diastage" in i["path"]
        )
        assert new_import["type"] == "stage"

    # --- catalogue updates ---

    def test_catalogue_stage_entry_added(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/Assets/CluicheTest/assets.catalogue.json"
        data = json.loads(p.read_text(encoding="utf-8"))
        ids = [a["id"] for a in data["assets"]]
        assert "stage.testentity_test_stage" in ids

    def test_catalogue_manifest_entry_added(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/Assets/CluicheTest/assets.catalogue.json"
        data = json.loads(p.read_text(encoding="utf-8"))
        ids = [a["id"] for a in data["assets"]]
        assert "manifest.testentity_test_stage" in ids

    def test_catalogue_stage_references_manifest(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/Assets/CluicheTest/assets.catalogue.json"
        data = json.loads(p.read_text(encoding="utf-8"))
        stage_entry = next(a for a in data["assets"] if a["id"] == "stage.testentity_test_stage")
        ref_targets = [r["target"] for r in stage_entry["references"]]
        assert "manifest.testentity_test_stage" in ref_targets

    # --- vcxproj updates ---

    def test_vcxproj_cpp_added(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/CluicheTest.vcxproj"
        text = p.read_text(encoding="utf-8")
        assert "TestEntityTestStageModule.cpp" in text

    def test_vcxproj_h_added(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/CluicheTest.vcxproj"
        text = p.read_text(encoding="utf-8")
        assert "TestEntityTestStageModule.h" in text

    def test_vcxproj_filters_cpp_added(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/CluicheTest.vcxproj.filters"
        text = p.read_text(encoding="utf-8")
        assert "TestEntityTestStageModule.cpp" in text

    def test_vcxproj_filters_h_added(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/CluicheTest.vcxproj.filters"
        text = p.read_text(encoding="utf-8")
        assert "TestEntityTestStageModule.h" in text

    def test_vcxproj_filters_filter_path_correct(self, tmp_path):
        _, repo = self._invoke(tmp_path)
        p = repo / "Cluiche/CluicheTest/CluicheTest.vcxproj.filters"
        text = p.read_text(encoding="utf-8")
        assert "ApplicationFlow\\Modules\\TestStages" in text


# ---------------------------------------------------------------------------
# --modules flag: extra dependencies included
# ---------------------------------------------------------------------------

class TestModulesFlag:
    def _get_main_pu_module(self, repo: Path, stage_name: str, diaapp_file: str) -> dict:
        p = (
            repo
            / f"Cluiche/Assets/Stages/{stage_name}/misc/ApplicationFlow"
            / diaapp_file
        )
        data = json.loads(p.read_text(encoding="utf-8"))
        main_pu = next(pu for pu in data["processing_units"] if pu["instance_id"] == "MainPU")
        return main_pu["modules"][0]

    def test_single_extra_module(self, tmp_path):
        repo = _build_fake_repo(tmp_path)
        with patch("dia_cli.commands.scaffold.stage_cmd.find_repo_root", return_value=repo):
            runner = CliRunner()
            result = runner.invoke(
                cli, ["scaffold", "stage", "TestEntity", "--modules", "Physics2DModule"]
            )
        assert result.exit_code == 0, result.output
        mod = self._get_main_pu_module(
            repo, "TestEntityTestStage", "testentity_test_stage.diaapp"
        )
        assert "Physics2DModule" in mod["dependencies"]
        assert "AutomationModule" in mod["dependencies"]

    def test_multiple_extra_modules(self, tmp_path):
        repo = _build_fake_repo(tmp_path)
        with patch("dia_cli.commands.scaffold.stage_cmd.find_repo_root", return_value=repo):
            runner = CliRunner()
            result = runner.invoke(
                cli,
                ["scaffold", "stage", "TestEntity", "--modules", "Physics2DModule,EntityModule"],
            )
        assert result.exit_code == 0, result.output
        mod = self._get_main_pu_module(
            repo, "TestEntityTestStage", "testentity_test_stage.diaapp"
        )
        assert "Physics2DModule" in mod["dependencies"]
        assert "EntityModule" in mod["dependencies"]
        assert "AutomationModule" in mod["dependencies"]

    def test_no_modules_only_automation(self, tmp_path):
        repo = _build_fake_repo(tmp_path)
        with patch("dia_cli.commands.scaffold.stage_cmd.find_repo_root", return_value=repo):
            runner = CliRunner()
            result = runner.invoke(cli, ["scaffold", "stage", "TestEntity"])
        assert result.exit_code == 0, result.output
        mod = self._get_main_pu_module(
            repo, "TestEntityTestStage", "testentity_test_stage.diaapp"
        )
        assert mod["dependencies"] == ["AutomationModule"]
