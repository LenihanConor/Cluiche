"""dia scaffold stage — create all boilerplate for a new CluicheTest test stage.

With ``--app <App>`` it instead adds a stage to an existing ``dia scaffold app``
application (e.g. CoW), using that app's layout.
"""
from __future__ import annotations

import json
import re
from pathlib import Path
from typing import List, Optional

import click

from dia_cli.utils.repo_root import find_repo_root

from .app_cmd import check_stage_type_clash
from .app_templates import render_context, render_stage_files, render_template


# ---------------------------------------------------------------------------
# Name derivation
# ---------------------------------------------------------------------------

def _pascal_to_snake(name: str) -> str:
    """Convert PascalCase to snake_case using the same algorithm as AssetStageIdFromAppStage.

    Rules (matching AssetServiceModule.cpp):
    - Insert '_' before uppercase C when preceded by a lowercase letter.
    - Insert '_' before uppercase C when preceded by an uppercase letter that is
      itself followed by a lowercase letter (handles acronyms like '2DTest' → '2d_test').
    - Digits are passed through unchanged; no separator is inserted before uppercase
      immediately following a digit (so '2D' → '2d', not '2_d').
    """
    result = []
    for i, ch in enumerate(name):
        if ch.isupper():
            if i > 0:
                prev = name[i - 1]
                next_ch = name[i + 1] if i + 1 < len(name) else ''
                prev_lower = prev.islower()
                prev_upper = prev.isupper()
                next_lower = next_ch.islower()
                if prev_lower or (prev_upper and next_lower):
                    result.append('_')
            result.append(ch.lower())
        else:
            result.append(ch)
    return ''.join(result)


def _derive_names(name: str) -> dict:
    """Derive all name variants from a PascalCase domain name like 'RigidBody2D'."""
    snake = _pascal_to_snake(name)                 # rigid_body2d / debug_gallery
    stage_name = f"{name}TestStage"                # RigidBody2DTestStage
    snake_stage = f"{snake}_test_stage"            # rigid_body2d_test_stage
    stage_id = f"stage.{snake_stage}"              # stage.rigid_body2d_test_stage
    module_name = f"{name}TestStageModule"         # RigidBody2DTestStageModule
    diastage_file = f"{snake_stage}.diastage"      # rigid_body2d_test_stage.diastage
    diaapp_file = f"{snake_stage}.diaapp"          # rigid_body2d_test_stage.diaapp
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
    data = {
        "version": 3,
        "processing_units": [
            {
                "instance_id": "MainPU",
                "frequency_hz": 30,
                "dedicated_thread": False,
                "modules": []
            },
            {
                "instance_id": "SimPU",
                "frequency_hz": 30,
                "dedicated_thread": True,
                "modules": [
                    {
                        "instance_id": n["module_name"],
                        "type_id": n["module_name"],
                        "stages": [n["stage_name"]],
                        "dependencies": extra_modules,
                        "channels": [
                            {"id": "AutomationService", "role": "consumes"},
                            {"id": "RenderToSim", "role": "reads"}
                        ]
                    }
                ]
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
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
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
        "DebugPanelPageModule",
        "UIModule",
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
# --app mode: add a stage to an existing generated application
# ---------------------------------------------------------------------------

_IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def _read_text_nl(path: Path):
    """Read a file as LF-normalised text and return it with its original newline."""
    data = path.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in data else "\n"
    return data.replace("\r\n", "\n"), nl


def _write_text_nl(path: Path, text: str, nl: str) -> None:
    path.write_bytes(text.replace("\n", nl).encode("utf-8"))


def _insert_after_last(text: str, pattern: str, insertion: str) -> str:
    """Insert ``insertion`` (a line block) after the last line matching regex ``pattern``."""
    lines = text.split("\n")
    idx = None
    for i, line in enumerate(lines):
        if re.search(pattern, line):
            idx = i
    if idx is None:
        raise click.ClickException(f"Could not find anchor /{pattern}/ in project file.")
    lines[idx + 1:idx + 1] = insertion.split("\n")
    return "\n".join(lines)


def _app_vcxproj_text(text: str, module: str) -> str:
    cpp = f'    <ClCompile Include="Modules\\{module}.cpp" />'
    h = f'    <ClInclude Include="Modules\\{module}.h" />'
    text = _insert_after_last(text, r'<ClCompile Include="[^"]+" />', cpp)
    return _insert_after_last(text, r'<ClInclude Include="[^"]+" />', h)


def _app_filters_text(text: str, module: str) -> str:
    cpp = (f'    <ClCompile Include="Modules\\{module}.cpp">\n'
           f'      <Filter>Modules</Filter>\n'
           f'    </ClCompile>')
    h = (f'    <ClInclude Include="Modules\\{module}.h">\n'
         f'      <Filter>Modules</Filter>\n'
         f'    </ClInclude>')
    text = _insert_after_last(text, r'</ClCompile>|<ClCompile Include="[^"]+" />', cpp)
    return _insert_after_last(text, r'</ClInclude>|<ClInclude Include="[^"]+" />', h)


def _scaffold_app_stage(
    repo_root: Path, app: str, name: str, extra_modules: List[str], dry_run: bool, note_budget: bool
) -> None:
    """Add stage ``name`` to the generated application ``app`` (all touch points)."""
    if not _IDENT.match(app):
        raise click.ClickException(f"Invalid application name '{app}'.")
    if not _IDENT.match(name):
        raise click.ClickException(f"Invalid stage name '{name}': use a PascalCase C++ identifier, e.g. Combat.")
    if name == "Boot":
        raise click.ClickException("Stage name 'Boot' is reserved for the baseline boot stage.")

    ctx = render_context(app, [], stage=name)
    snake = ctx["stage_snake"]
    module = f"{name}Module"

    project_dir = repo_root / "Cluiche" / app
    assets_dir = repo_root / "Cluiche" / "Assets" / app
    vcxproj_path = project_dir / f"{app}.vcxproj"
    filters_path = project_dir / f"{app}.vcxproj.filters"
    main_path = assets_dir / f"{ctx['lower']}_main.diaapp"
    diagame_path = assets_dir / f"{ctx['lower']}.diagame"
    catalogue_path = assets_dir / "assets.catalogue.json"
    rel = lambda p: p.relative_to(repo_root).as_posix()

    if not vcxproj_path.is_file() or not assets_dir.is_dir():
        raise click.ClickException(
            f"Application '{app}' not found: expected {rel(vcxproj_path)} and {rel(assets_dir)}/. "
            f"Create it with 'dia scaffold app {app}'."
        )
    for p in (filters_path, main_path, diagame_path, catalogue_path):
        if not p.is_file():
            raise click.ClickException(f"Application '{app}' is missing {rel(p)}.")

    check_stage_type_clash(repo_root, name, hint="stage NAME")

    # New files come from the same templates `dia scaffold app` uses for its initial stage.
    files = render_stage_files(ctx)
    diaapp_rel = f"Cluiche/Assets/{app}/Stages/{name}/misc/ApplicationFlow/{snake}.diaapp"
    if extra_modules:
        data = json.loads(files[diaapp_rel])
        deps = data["processing_units"][1]["modules"][0]["dependencies"]
        deps.extend(m for m in extra_modules if m not in deps)
        files[diaapp_rel] = json.dumps(data, indent=4)
    for r in files:
        if (repo_root / r).exists():
            raise click.ClickException(f"Stage '{name}' already exists: {r}")

    # Compute every edit in memory first so a failure leaves the tree untouched.
    main_text, main_nl = _read_text_nl(main_path)
    main = json.loads(main_text)
    stages = main.setdefault("stages", [])
    if any(s.get("name") == name for s in stages):
        raise click.ClickException(f"Stage '{name}' already exists in {rel(main_path)}.")
    boot = next((s for s in stages if s.get("name") == "Boot"), None)
    if boot is None:
        raise click.ClickException(f"{rel(main_path)} has no 'Boot' stage to wire the new stage into.")
    stages.append({"name": name, "transitions": ["Boot"], "auto_advance": False})
    boot.setdefault("transitions", []).append(name)

    game_text, game_nl = _read_text_nl(diagame_path)
    game = json.loads(game_text)
    new_import = next(i for i in json.loads(render_template("app.diagame.j2", ctx))["imports"] if i["type"] == "stage")
    if new_import in game.get("imports", []):
        raise click.ClickException(f"Stage '{name}' already exists in {rel(diagame_path)}.")
    game.setdefault("imports", []).append(new_import)

    cat_text, cat_nl = _read_text_nl(catalogue_path)
    cat = json.loads(cat_text)
    existing_ids = {a.get("id") for a in cat.get("assets", [])}
    new_assets = [a for a in json.loads(render_template("assets.catalogue.json.j2", ctx))["assets"]
                  if a["id"] in (f"stage.{snake}", f"manifest.{snake}")]
    clash = [a["id"] for a in new_assets if a["id"] in existing_ids]
    if clash:
        raise click.ClickException(f"Stage '{name}' already exists in catalogue ({', '.join(clash)}).")
    cat["assets"].extend(new_assets)

    proj_text, proj_nl = _read_text_nl(vcxproj_path)
    if f"Modules\\{module}." in proj_text:
        raise click.ClickException(f"Stage '{name}' already exists in {rel(vcxproj_path)}.")
    flt_text, flt_nl = _read_text_nl(filters_path)
    edits = [
        (main_path, json.dumps(main, indent=4), main_nl),
        (diagame_path, json.dumps(game, indent=4), game_nl),
        (catalogue_path, json.dumps(cat, indent=4), cat_nl),
        (vcxproj_path, _app_vcxproj_text(proj_text, module), proj_nl),
        (filters_path, _app_filters_text(flt_text, module), flt_nl),
    ]
    ordered = sorted(files, key=lambda r: (not r.endswith((".h", ".cpp")), r))

    if dry_run:
        click.echo(f"[dry-run] Would create/modify the following files for stage '{name}' of app '{app}':\n")
        for r in ordered:
            click.echo(f"  Create  {r}")
        for p, _, _ in edits:
            click.echo(f"  Update  {rel(p)}")
        return

    for r in ordered:
        path = repo_root / r
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(files[r], encoding="utf-8")
    for p, text, nl in edits:
        _write_text_nl(p, text, nl)

    click.echo("")
    for r in ordered:
        click.echo(f"✓ Created  {r}")
    for p, _, _ in edits:
        click.echo(f"✓ Updated  {rel(p)}")
    if note_budget:
        click.echo("\nNote: --budget is ignored with --app (app stages have no frame budget).")
    click.echo(f"\nNext: fill in DoUpdate() logic in {module}.cpp")


# ---------------------------------------------------------------------------
# Click command
# ---------------------------------------------------------------------------

@click.command("stage")
@click.argument("name")
@click.option(
    "--modules",
    default=None,
    metavar="MODULE1,MODULE2,...",
    help="Comma-separated extra module type IDs to add as dependencies.",
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
@click.option(
    "--app",
    "app_name",
    default=None,
    metavar="APP",
    help=(
        "Add the stage to an existing application made by 'dia scaffold app' (e.g. CoW) "
        "instead of CluicheTest. NAME is then the stage name as-is (no 'TestStage' suffix): "
        "creates Cluiche/APP/Modules/NAMEModule.{h,cpp} and Cluiche/Assets/APP/Stages/NAME/, and "
        "updates the app's main .diaapp (stage + Boot transition), .diagame, catalogue and vcxproj."
    ),
)
@click.pass_context
def stage(ctx: click.Context, name: str, modules: Optional[str], dry_run: bool, budget: int,
          app_name: Optional[str]) -> None:
    """Scaffold all boilerplate for a new CluicheTest test stage.

    NAME is a PascalCase domain name, e.g. RigidBody2D. With --app, NAME is the
    stage name of an application stage, e.g. `dia scaffold stage Combat --app CoW`.
    """
    extra_modules: List[str] = [m.strip() for m in modules.split(",")] if modules else []

    repo_root = find_repo_root(__file__)

    if app_name is not None:
        budget_given = ctx.get_parameter_source("budget") != click.core.ParameterSource.DEFAULT
        _scaffold_app_stage(repo_root, app_name, name, extra_modules, dry_run, budget_given)
        return

    n = _derive_names(name)

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
