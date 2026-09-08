"""dia scaffold sln-add — register a project in Cluiche.sln."""
from __future__ import annotations

import re
import uuid
from pathlib import Path
from typing import List, Optional

import click

from dia_cli.utils.repo_root import find_repo_root


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

_CPP_PROJECT_TYPE_GUID = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

_FOLDER_GUIDS = {
    "library": "{2291F464-9B87-42F1-AA90-34FE22DD5F9B}",
    "editor": "{E58B6FF6-1D5F-4E6C-80B3-28E5A75EFD0F}",
    "executables": "{86CA7611-D55D-411B-8784-187D12107EE0}",
}

_CONFIGS = [
    ("Debug|x64", "Debug|x64"),
    ("Debug-Asan|x64", "Debug-Asan|x64"),
    ("Debug-Ubsan|x64", "Debug-Ubsan|x64"),
    ("MinSizeRel|x64", "Release|x64"),
    ("Release|x64", "Release|x64"),
    ("RelWithDebInfo|x64", "Release|x64"),
]


# ---------------------------------------------------------------------------
# GUID helpers
# ---------------------------------------------------------------------------

def _make_project_guid(project_name: str) -> str:
    """Deterministic GUID from project name."""
    return "{" + str(uuid.uuid5(uuid.NAMESPACE_DNS, f"cluiche.sln.{project_name}")).upper() + "}"


# ---------------------------------------------------------------------------
# .sln manipulation
# ---------------------------------------------------------------------------

def _find_last_endproject(text: str) -> int:
    """Return the index after the last 'EndProject' line (before Global)."""
    lines = text.splitlines(keepends=True)
    last_idx = -1
    for i, line in enumerate(lines):
        if line.strip() == "EndProject":
            last_idx = i
    if last_idx == -1:
        raise ValueError("No 'EndProject' found in .sln")
    # Return character offset after that line
    offset = sum(len(lines[j]) for j in range(last_idx + 1))
    return offset


def _add_project_to_sln(
    sln_text: str,
    project_name: str,
    vcxproj_rel_path: str,
    project_guid: str,
    deps: List[str],
    folder: str,
) -> str:
    """Insert project declaration, config entries, and nested-projects entry."""

    # 1. Build project declaration block
    decl_lines = [
        f'Project("{_CPP_PROJECT_TYPE_GUID}") = "{project_name}", "{vcxproj_rel_path}", "{project_guid}"'
    ]
    if deps:
        decl_lines.append("\tProjectSection(ProjectDependencies) = postProject")
        for dep_guid in deps:
            decl_lines.append(f"\t\t{dep_guid} = {dep_guid}")
        decl_lines.append("\tEndProjectSection")
    decl_lines.append("EndProject")
    decl_block = "\n".join(decl_lines) + "\n"

    # 2. Insert after last EndProject (before Global)
    lines = sln_text.splitlines(keepends=True)
    last_ep_idx = -1
    for i, line in enumerate(lines):
        if line.strip() == "EndProject":
            last_ep_idx = i
    if last_ep_idx == -1:
        raise ValueError("No 'EndProject' found in .sln")
    lines.insert(last_ep_idx + 1, decl_block)
    sln_text = "".join(lines)

    # 3. Add configuration entries
    config_lines = []
    for sln_cfg, proj_cfg in _CONFIGS:
        config_lines.append(f"\t\t{project_guid}.{sln_cfg}.ActiveCfg = {proj_cfg}")
        config_lines.append(f"\t\t{project_guid}.{sln_cfg}.Build.0 = {proj_cfg}")
    config_block = "\n".join(config_lines) + "\n"

    # Insert before EndGlobalSection of ProjectConfigurationPlatforms
    marker = "GlobalSection(ProjectConfigurationPlatforms) = postSolution"
    cfg_section_start = sln_text.find(marker)
    if cfg_section_start == -1:
        raise ValueError("Cannot find ProjectConfigurationPlatforms section")
    # Find the EndGlobalSection for this section
    end_marker = "EndGlobalSection"
    search_start = cfg_section_start + len(marker)
    end_pos = sln_text.find(end_marker, search_start)
    if end_pos == -1:
        raise ValueError("Cannot find end of ProjectConfigurationPlatforms")
    # Insert config block before EndGlobalSection
    sln_text = sln_text[:end_pos] + config_block + "\t" + sln_text[end_pos:]

    # 4. Add NestedProjects entry (folder assignment)
    folder_guid = _FOLDER_GUIDS.get(folder, _FOLDER_GUIDS["library"])
    nested_marker = "GlobalSection(NestedProjects) = preSolution"
    nested_start = sln_text.find(nested_marker)
    if nested_start != -1:
        nested_end = sln_text.find("EndGlobalSection", nested_start + len(nested_marker))
        if nested_end != -1:
            nested_entry = f"\t\t{project_guid} = {folder_guid}\n"
            sln_text = sln_text[:nested_end] + nested_entry + "\t" + sln_text[nested_end:]

    return sln_text


def _find_project_guid_by_name(sln_text: str, name: str) -> Optional[str]:
    """Look up a project GUID by name in the .sln."""
    pattern = re.compile(
        r'Project\("[^"]+"\)\s*=\s*"' + re.escape(name) + r'"\s*,\s*"[^"]+"\s*,\s*"([^"]+)"'
    )
    m = pattern.search(sln_text)
    return m.group(1) if m else None


# ---------------------------------------------------------------------------
# Click command
# ---------------------------------------------------------------------------

@click.command("sln-add")
@click.argument("project_name")
@click.option(
    "--folder",
    type=click.Choice(["library", "editor", "executables"], case_sensitive=False),
    default="library",
    show_default=True,
    help="Solution folder to place the project in.",
)
@click.option(
    "--deps",
    default=None,
    metavar="DEP1,DEP2,...",
    help="Comma-separated project names this depends on (looks up their GUIDs).",
)
@click.option(
    "--dry-run",
    is_flag=True,
    default=False,
    help="Print what would change without modifying the .sln file.",
)
def sln_add(project_name: str, folder: str, deps: Optional[str], dry_run: bool) -> None:
    """Add a project to Cluiche.sln with configs and folder placement.

    PROJECT_NAME is the project to add (e.g. DiaPipelineEditor).
    The .vcxproj must already exist at Dia/<name>/<name>.vcxproj.
    """
    repo_root = find_repo_root(__file__)
    sln_path = repo_root / "Cluiche" / "Cluiche.sln"

    if not sln_path.exists():
        raise click.ClickException(f"Solution not found: {sln_path}")

    # Determine vcxproj path (relative to sln dir)
    vcxproj_path = repo_root / "Dia" / project_name / f"{project_name}.vcxproj"
    if not vcxproj_path.exists() and not dry_run:
        raise click.ClickException(
            f"vcxproj not found: {vcxproj_path}\n"
            f"Run 'dia scaffold plugin' or 'dia scaffold module' first."
        )
    vcxproj_rel = f"..\\Dia\\{project_name}\\{project_name}.vcxproj"

    project_guid = _make_project_guid(project_name)

    # Read .sln
    sln_text = sln_path.read_text(encoding="utf-8")

    # Check if already present
    if project_name in sln_text and project_guid in sln_text:
        click.echo(f"{project_name} already in solution.")
        return

    # Resolve dependency GUIDs
    dep_guids: List[str] = []
    if deps:
        for dep_name in deps.split(","):
            dep_name = dep_name.strip()
            guid = _find_project_guid_by_name(sln_text, dep_name)
            if guid:
                dep_guids.append(guid)
            else:
                click.echo(f"  WARNING: dependency '{dep_name}' not found in .sln, skipping")

    if dry_run:
        click.echo(f"[dry-run] Would add to Cluiche.sln:\n")
        click.echo(f"  Project:  {project_name}")
        click.echo(f"  GUID:     {project_guid}")
        click.echo(f"  vcxproj:  {vcxproj_rel}")
        click.echo(f"  Folder:   {folder}")
        if dep_guids:
            click.echo(f"  Deps:     {len(dep_guids)} project(s)")
        click.echo(f"\n  Configs:  {len(_CONFIGS)} entries (Debug, Release, Asan, Ubsan, etc.)")
        return

    # Apply changes
    sln_text = _add_project_to_sln(sln_text, project_name, vcxproj_rel, project_guid, dep_guids, folder)
    sln_path.write_text(sln_text, encoding="utf-8")

    click.echo(f"\n  Added {project_name} to Cluiche.sln")
    click.echo(f"  GUID:   {project_guid}")
    click.echo(f"  Folder: {folder}")
    if dep_guids:
        click.echo(f"  Deps:   {len(dep_guids)} project(s)")
    click.echo(f"  Configs: {len(_CONFIGS) * 2} lines added")
