"""dia scaffold stage — create all boilerplate for a new CluicheTest test stage."""
from __future__ import annotations

import json
from pathlib import Path
from typing import List, Optional

import click

from dia_cli.utils.repo_root import find_repo_root


# ---------------------------------------------------------------------------
# Name derivation
# ---------------------------------------------------------------------------

def _derive_names(name: str) -> dict:
    """Derive all name variants from a PascalCase domain name like 'RigidBody2D'."""
    snake = name.lower()                          # rigidbody2d
    stage_name = f"{name}TestStage"               # RigidBody2DTestStage
    snake_stage = f"{snake}_test_stage"           # rigidbody2d_test_stage
    stage_id = f"stage.{snake_stage}"             # stage.rigidbody2d_test_stage
    module_name = f"{name}TestStageModule"        # RigidBody2DTestStageModule
    diastage_file = f"{snake_stage}.diastage"     # rigidbody2d_test_stage.diastage
    diaapp_file = f"{snake_stage}.diaapp"         # rigidbody2d_test_stage.diaapp
    return dict(
        name=name,
        snake=snake,
        stage_name=stage_name,
        snake_stage=snake_stage,
        stage_id=stage_id,
        module_name=module_name,
        diastage_file=diastage_file,
        diaapp_file=diaapp_file,
    )


# ---------------------------------------------------------------------------
# Content generators
# ---------------------------------------------------------------------------

def _diastage_content(n: dict) -> str:
    data = {
        "name": n["stage_name"],
        "manifest": f"Stages/{n['stage_name']}/misc/ApplicationFlow/{n['diaapp_file']}",
        "config": {
            "path_aliases": {
                "stage_root": "."
            }
        }
    }
    return json.dumps(data, indent=4)


def _diaapp_content(n: dict, extra_modules: List[str]) -> str:
    # Build dependency list: always include AutomationModule, then extras
    deps = ["AutomationModule"] + extra_modules
    deps_json = ", ".join(f'"{d}"' for d in deps)

    data = {
        "version": 3,
        "processing_units": [
            {
                "instance_id": "MainPU",
                "frequency_hz": 30,
                "dedicated_thread": False,
                "modules": [
                    {
                        "instance_id": n["module_name"],
                        "type_id": n["module_name"],
                        "stages": [n["stage_name"]],
                        "dependencies": deps,
                        "channels": []
                    }
                ]
            },
            {
                "instance_id": "SimPU",
                "frequency_hz": 30,
                "dedicated_thread": True,
                "modules": []
            }
        ]
    }
    return json.dumps(data, indent=4)


def _header_content(n: dict, budget: int) -> str:
    return f"""\
#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>

namespace CluicheTest {{

class {n['module_name']} : public TestStageModuleBase
{{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "TODO: describe what this stage tests";
    explicit {n['module_name']}(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override {{ return {budget}; }}
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;

private:
    // TODO: add domain-specific state here
}};

}} // namespace CluicheTest
"""


def _impl_content(n: dict) -> str:
    mn = n["module_name"]
    stage_name = n["stage_name"]
    snake = n["snake"]
    snake_stage = n["snake_stage"]
    return f"""\
#include "Modules/TestStages/{mn}.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {{

const Dia::Core::StringCRC {mn}::kTypeId("{mn}");

{mn}::{mn}(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{{}}

Dia::Core::StringCRC {mn}::GetStageName() const
{{
    return Dia::Core::StringCRC("{stage_name}");
}}

const Dia::Core::StringCRC* {mn}::GetCheckpointNames(unsigned int& outCount) const
{{
    static const Dia::Core::StringCRC names[] = {{
        Dia::Core::StringCRC("test.{snake}.passed")
    }};
    outCount = 1;
    return names;
}}

void {mn}::OnStart(Dia::Automation::AutomationService* service)
{{
    // TODO: set up scene

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.{snake}.passed"),
        [this]() -> Dia::Automation::CheckpointResult {{
            return {{ false, "pending", 0.0f }};
        }});
}}

void {mn}::OnUpdate(float /*deltaTime*/)
{{
    // TODO: check pass condition, call ReportPassed() when satisfied
}}

}} // namespace CluicheTest

namespace {{ using {mn}_ = CluicheTest::{mn}; }}
DIA_MODULE({mn}_);
"""


# ---------------------------------------------------------------------------
# File/JSON update helpers
# ---------------------------------------------------------------------------

def _update_cluiche_main_diaapp(path: Path, stage_name: str) -> None:
    """Add stage entry, wire Boot↔stage transitions, and update relevant module stage lists."""
    data = json.loads(path.read_text(encoding="utf-8"))

    # Add to top-level stages array with back-transition to Boot
    new_stage_entry = {"name": stage_name, "transitions": ["Boot"], "auto_advance": False}
    data["stages"].append(new_stage_entry)

    # Add stage_name to Boot's transitions so it appears in the Boot menu
    for stage in data.get("stages", []):
        if stage.get("name") == "Boot":
            if stage_name not in stage["transitions"]:
                stage["transitions"].append(stage_name)

    # Update relevant module stage lists in all PUs
    _STAGE_LIST_MODULES = {
        "TestStageHUDModule",
        "VisualDebuggerModule",
        "VisualDebuggerConsoleModule",
    }
    for pu in data.get("processing_units", []):
        for mod in pu.get("modules", []):
            if mod.get("instance_id") in _STAGE_LIST_MODULES or mod.get("type_id") in _STAGE_LIST_MODULES:
                if stage_name not in mod["stages"]:
                    mod["stages"].append(stage_name)

    path.write_text(json.dumps(data, indent=4), encoding="utf-8")


def _update_diagame(path: Path, stage_name: str, diastage_file: str) -> None:
    """Add stage import entry to .diagame imports list."""
    data = json.loads(path.read_text(encoding="utf-8"))
    new_import = {
        "path": f"Stages/{stage_name}/{diastage_file}",
        "type": "stage"
    }
    data["imports"].append(new_import)
    path.write_text(json.dumps(data, indent=4), encoding="utf-8")


def _update_catalogue(path: Path, n: dict) -> None:
    """Add two catalogue entries (stage + manifest) to assets array."""
    data = json.loads(path.read_text(encoding="utf-8"))
    stage_entry = {
        "id": n["stage_id"],
        "type": "stage",
        "source_path": f"Stages/{n['stage_name']}/{n['diastage_file']}",
        "scope": "global",
        "tags": ["misc"],
        "references": [
            {"type": "contains", "target": f"manifest.{n['snake_stage']}"}
        ]
    }
    manifest_entry = {
        "id": f"manifest.{n['snake_stage']}",
        "type": "manifest",
        "source_path": f"Stages/{n['stage_name']}/misc/ApplicationFlow/{n['diaapp_file']}",
        "scope": "stage",
        "stage_name": n["stage_name"],
        "tags": ["misc/ApplicationFlow"]
    }
    data["assets"].append(stage_entry)
    data["assets"].append(manifest_entry)
    path.write_text(json.dumps(data, indent=4), encoding="utf-8")


def _insert_after_line(text: str, search: str, insertion: str) -> str:
    """Insert insertion after the first line containing search."""
    lines = text.splitlines(keepends=True)
    result = []
    inserted = False
    for line in lines:
        result.append(line)
        if not inserted and search in line:
            # Preserve the newline style of the matched line
            result.append(insertion + "\n")
            inserted = True
    if not inserted:
        raise ValueError(f"Could not find anchor '{search}' in text")
    return "".join(result)


def _update_vcxproj(path: Path, module_name: str) -> None:
    text = path.read_text(encoding="utf-8")
    cpp_anchor = r"TestStageHUDModule.cpp"
    h_anchor = r"TestStageHUDModule.h"
    cpp_line = f'    <ClCompile Include="Modules\\TestStages\\{module_name}.cpp" />'
    h_line = f'    <ClInclude Include="Modules\\TestStages\\{module_name}.h" />'
    text = _insert_after_line(text, cpp_anchor, cpp_line)
    text = _insert_after_line(text, h_anchor, h_line)
    path.write_text(text, encoding="utf-8")


def _update_vcxproj_filters(path: Path, module_name: str) -> None:
    text = path.read_text(encoding="utf-8")
    cpp_anchor = r"TestStageHUDModule.cpp"
    h_anchor = r"TestStageHUDModule.h"
    cpp_block = (
        f'    <ClCompile Include="Modules\\TestStages\\{module_name}.cpp">\n'
        f'      <Filter>ApplicationFlow\\Modules\\TestStages</Filter>\n'
        f'    </ClCompile>'
    )
    h_block = (
        f'    <ClInclude Include="Modules\\TestStages\\{module_name}.h">\n'
        f'      <Filter>ApplicationFlow\\Modules\\TestStages</Filter>\n'
        f'    </ClInclude>'
    )
    text = _insert_after_line(text, cpp_anchor, cpp_block)
    text = _insert_after_line(text, h_anchor, h_block)
    path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# Click command
# ---------------------------------------------------------------------------

@click.command("stage")
@click.argument("name")
@click.option(
    "--modules",
    default=None,
    metavar="MODULE1,MODULE2,...",
    help="Comma-separated extra module type IDs to add as dependencies (AutomationModule always included).",
)
@click.option(
    "--dry-run",
    is_flag=True,
    default=False,
    help="Print what would be created/modified without writing any files.",
)
@click.option(
    "--budget",
    default=300,
    show_default=True,
    type=int,
    metavar="FRAMES",
    help="Frame budget for the stage (default: 300).",
)
def stage(name: str, modules: Optional[str], dry_run: bool, budget: int) -> None:
    """Scaffold all boilerplate for a new CluicheTest test stage.

    NAME is a PascalCase domain name, e.g. RigidBody2D.
    """
    n = _derive_names(name)
    extra_modules: List[str] = [m.strip() for m in modules.split(",")] if modules else []

    repo_root = find_repo_root(__file__)

    # Define all paths
    stage_dir = repo_root / "Cluiche" / "Assets" / "CluicheTest" / "Stages" / n["stage_name"]
    diastage_path = stage_dir / n["diastage_file"]
    diaapp_dir = stage_dir / "misc" / "ApplicationFlow"
    diaapp_path = diaapp_dir / n["diaapp_file"]

    modules_dir = repo_root / "Cluiche" / "CluicheTest" / "Modules" / "TestStages"
    header_path = modules_dir / f"{n['module_name']}.h"
    impl_path = modules_dir / f"{n['module_name']}.cpp"

    cluiche_main_path = (
        repo_root / "Cluiche" / "Assets" / "CluicheTest" / "Global" / "misc"
        / "ApplicationFlow" / "cluiche_main.diaapp"
    )
    diagame_path = repo_root / "Cluiche" / "Assets" / "CluicheTest" / "cluichetest.diagame"
    catalogue_path = repo_root / "Cluiche" / "Assets" / "CluicheTest" / "assets.catalogue.json"
    vcxproj_path = repo_root / "Cluiche" / "CluicheTest" / "CluicheTest.vcxproj"
    filters_path = repo_root / "Cluiche" / "CluicheTest" / "CluicheTest.vcxproj.filters"

    rel = lambda p: str(p.relative_to(repo_root)).replace("\\", "/")

    created: List[str] = []
    updated: List[str] = []

    if dry_run:
        click.echo(f"[dry-run] Would create/modify the following files for stage '{n['stage_name']}':\n")
        click.echo(f"  Create  {rel(diastage_path)}")
        click.echo(f"  Create  {rel(diaapp_path)}")
        click.echo(f"  Create  {rel(header_path)}")
        click.echo(f"  Create  {rel(impl_path)}")
        click.echo(f"  Update  {rel(cluiche_main_path)}")
        click.echo(f"  Update  {rel(diagame_path)}")
        click.echo(f"  Update  {rel(catalogue_path)}")
        click.echo(f"  Update  {rel(vcxproj_path)}")
        click.echo(f"  Update  {rel(filters_path)}")
        return

    # 1. Create .diastage
    diastage_path.parent.mkdir(parents=True, exist_ok=True)
    diastage_path.write_text(_diastage_content(n), encoding="utf-8")
    created.append(rel(diastage_path))

    # 2. Create .diaapp
    diaapp_path.parent.mkdir(parents=True, exist_ok=True)
    diaapp_path.write_text(_diaapp_content(n, extra_modules), encoding="utf-8")
    created.append(rel(diaapp_path))

    # 3. Create module header
    modules_dir.mkdir(parents=True, exist_ok=True)
    header_path.write_text(_header_content(n, budget), encoding="utf-8")
    created.append(rel(header_path))

    # 4. Create module implementation
    impl_path.write_text(_impl_content(n), encoding="utf-8")
    created.append(rel(impl_path))

    # 5. Update cluiche_main.diaapp
    _update_cluiche_main_diaapp(cluiche_main_path, n["stage_name"])
    updated.append(rel(cluiche_main_path))

    # 6. Update cluichetest.diagame
    _update_diagame(diagame_path, n["stage_name"], n["diastage_file"])
    updated.append(rel(diagame_path))

    # 7. Update assets.catalogue.json
    _update_catalogue(catalogue_path, n)
    updated.append(rel(catalogue_path))

    # 8. Update CluicheTest.vcxproj
    _update_vcxproj(vcxproj_path, n["module_name"])
    updated.append(rel(vcxproj_path))

    # 9. Update CluicheTest.vcxproj.filters
    _update_vcxproj_filters(filters_path, n["module_name"])
    updated.append(rel(filters_path))

    # Summary
    click.echo("")
    for path_str in created:
        click.echo(f"✓ Created  {path_str}")
    for path_str in updated:
        click.echo(f"✓ Updated  {path_str}")
    click.echo(
        f"\nNext: fill in OnUpdate() logic and update kDescription in {n['module_name']}.h"
    )
