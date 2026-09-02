"""sln_sync — rewrite Cluiche.sln solution folders to numbered layer names.

Reads layer: fields from all dia.*.architecture.module.md files, then
updates the .sln NestedProjects section so every Dia library project sits
under the correct numbered folder (1.0-Core, 1.1-Services, etc.).

Non-Dia projects (CluicheTest, GoogleTests, executables, editors) are left
in their existing folders.
"""
from __future__ import annotations

import re
import uuid
from pathlib import Path
from typing import Optional

from .arch_module_map import build_module_map

# ---------------------------------------------------------------------------
# Layer → folder name mapping (from spec)
# ---------------------------------------------------------------------------

_LAYER_TO_FOLDER: dict[str, str] = {
    "foundation/core":        "1.0-Core",
    "foundation/maths":       "1.1-Maths",
    "foundation/services":    "1.1-Services",
    "foundation/platform":    "1.2-Platform",
    "foundation/application": "1.2-Application",
    "foundation/assets":      "2.0-Assets",
    "assets/core":            "2.0-Assets",
    "assets/tools":           "2.1-Assets-Tools",
    "domain/visual/core":     "3.0-Visual",
    "domain/visual/tools":    "3.1-Visual-Tools",
    "domain/physics/core":    "3.0-Physics",
    "domain/physics/tools":   "3.1-Physics-Tools",
    "domain/animation/core":  "3.0-Animation",
    "domain/animation/tools": "3.1-Animation-Tools",
}

_SOLUTION_FOLDER_TYPE = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}"
_CPP_PROJECT_TYPE = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"


def _folder_guid(folder_name: str) -> str:
    """Deterministic GUID for a numbered solution folder."""
    return "{" + str(uuid.uuid5(uuid.NAMESPACE_DNS, f"cluiche.sln.folder.{folder_name}")).upper() + "}"


def _parse_project_guids(sln_text: str) -> dict[str, str]:
    """Return {project_name: guid} for all C++ projects in the .sln."""
    pattern = re.compile(
        r'Project\("' + re.escape(_CPP_PROJECT_TYPE) + r'"\)\s*=\s*"([^"]+)"\s*,\s*"[^"]+"\s*,\s*"(\{[^}]+\})"'
    )
    return {m.group(1): m.group(2) for m in pattern.finditer(sln_text)}


def _parse_folder_guids(sln_text: str) -> dict[str, str]:
    """Return {folder_name: guid} for all solution folders."""
    pattern = re.compile(
        r'Project\("' + re.escape(_SOLUTION_FOLDER_TYPE) + r'"\)\s*=\s*"([^"]+)"\s*,\s*"[^"]+"\s*,\s*"(\{[^}]+\})"'
    )
    return {m.group(1): m.group(2) for m in pattern.finditer(sln_text)}


def _parse_nested_projects(sln_text: str) -> dict[str, str]:
    """Return {child_guid: parent_guid} from NestedProjects section."""
    result = {}
    in_nested = False
    for line in sln_text.splitlines():
        if "GlobalSection(NestedProjects)" in line:
            in_nested = True
            continue
        if in_nested:
            if "EndGlobalSection" in line:
                break
            m = re.match(r"\s*(\{[^}]+\})\s*=\s*(\{[^}]+\})", line)
            if m:
                result[m.group(1)] = m.group(2)
    return result


# ---------------------------------------------------------------------------
# Core sync logic
# ---------------------------------------------------------------------------

def compute_assignments(
    repo_root: Path,
) -> tuple[dict[str, str], dict[str, str], list[str]]:
    """Compute the desired folder assignments.

    Returns:
        project_to_folder: {project_name → folder_name}  (only Dia library projects with known layers)
        folder_names: set of folder names needed
        warnings: list of warning strings
    """
    module_map, warnings = build_module_map(repo_root)

    # Build path → project_name map: "Dia/DiaCore" → "DiaCore"
    path_to_project: dict[str, str] = {}
    for module_id, info in module_map.items():
        if not info.path:
            continue
        # Only top-level Dia/ projects (not sub-modules)
        parts = info.path.strip("/").split("/")
        if len(parts) == 2 and parts[0] == "Dia":
            path_to_project[info.path] = parts[1]

    # For each top-level project, find a layer (prefer the root module doc)
    project_layers: dict[str, str] = {}
    for module_id, info in module_map.items():
        if not info.path or not info.layer:
            continue
        parts = info.path.strip("/").split("/")
        if len(parts) == 2 and parts[0] == "Dia":
            project_name = parts[1]
            # Only override if not already set (first match wins — root module doc)
            if project_name not in project_layers:
                project_layers[project_name] = info.layer

    project_to_folder: dict[str, str] = {}
    for project_name, layer in project_layers.items():
        folder_name = _LAYER_TO_FOLDER.get(layer)
        if folder_name:
            project_to_folder[project_name] = folder_name
        else:
            warnings.append(f"WARN: no folder mapping for layer '{layer}' on {project_name}")

    folders_needed = set(project_to_folder.values())
    return project_to_folder, folders_needed, warnings


def run_sln_sync(
    repo_root: Path,
    dry_run: bool = False,
) -> tuple[list[str], list[str], int]:
    """Run the SLN folder sync.

    Returns (changes, warnings, exit_code).
    changes: human-readable list of what changed (or would change).
    exit_code: 0 = success.
    """
    sln_path = repo_root / "Cluiche" / "Cluiche.sln"
    if not sln_path.exists():
        return [], [f"ERROR: solution not found: {sln_path}"], 1

    sln_text = sln_path.read_text(encoding="utf-8")

    project_to_folder, folders_needed, warnings = compute_assignments(repo_root)

    existing_projects = _parse_project_guids(sln_text)
    existing_folders = _parse_folder_guids(sln_text)
    existing_nested = _parse_nested_projects(sln_text)

    changes: list[str] = []

    # Determine which folders need to be created
    folders_to_create = folders_needed - set(existing_folders.keys())

    # Determine which nested assignments need to change
    reassignments: list[tuple[str, str, str, str]] = []  # (project_name, project_guid, old_folder_name, new_folder_name)
    for project_name, desired_folder in sorted(project_to_folder.items()):
        project_guid = existing_projects.get(project_name)
        if not project_guid:
            warnings.append(f"WARN: project '{project_name}' not found in .sln (skipping)")
            continue

        desired_folder_guid = _folder_guid(desired_folder)
        current_parent_guid = existing_nested.get(project_guid)
        current_folder_name = next(
            (name for name, guid in existing_folders.items() if guid == current_parent_guid),
            "<none>"
        ) if current_parent_guid else "<none>"

        if current_parent_guid != desired_folder_guid:
            reassignments.append((project_name, project_guid, current_folder_name, desired_folder))

    if not folders_to_create and not reassignments:
        changes.append("Solution folders already in sync — no changes needed.")
        return changes, warnings, 0

    # Report what will change
    for folder_name in sorted(folders_to_create):
        changes.append(f"CREATE folder: {folder_name}")
    for project_name, _, old_folder, new_folder in reassignments:
        changes.append(f"MOVE   {project_name}: {old_folder} → {new_folder}")

    if dry_run:
        return changes, warnings, 0

    # Apply changes to sln_text
    # 1. Insert new folder Project(...) declarations before the Global section
    for folder_name in sorted(folders_to_create):
        guid = _folder_guid(folder_name)
        folder_decl = f'Project("{_SOLUTION_FOLDER_TYPE}") = "{folder_name}", "{folder_name}", "{guid}"\nEndProject\n'
        # Insert just before "Global"
        global_idx = sln_text.find("\nGlobal\n")
        if global_idx == -1:
            global_idx = sln_text.find("\nGlobal\r\n")
        if global_idx == -1:
            warnings.append("WARN: could not find Global section — folder declaration not inserted")
        else:
            sln_text = sln_text[:global_idx + 1] + folder_decl + sln_text[global_idx + 1:]

    # 2. Update NestedProjects section
    # Track which reassignments were applied by rewriting an existing line —
    # a project with NO current nested entry (old_folder == "<none>") has no
    # line to rewrite and must instead get a brand-new line inserted just
    # before EndGlobalSection.
    handled_child_guids: set[str] = set()
    new_nested_lines: list[str] = []
    in_nested = False
    for line in sln_text.splitlines(keepends=True):
        if "GlobalSection(NestedProjects)" in line:
            in_nested = True
            new_nested_lines.append(line)
            continue
        if in_nested and "EndGlobalSection" in line:
            # Insert brand-new entries for projects that had no prior nesting
            # (existing-line rewrite below only handles projects already nested).
            for project_name, project_guid, old_folder, new_folder in sorted(reassignments):
                if old_folder == "<none>" and project_guid not in handled_child_guids:
                    new_parent_guid = _folder_guid(new_folder)
                    new_nested_lines.append(f"\t\t{project_guid} = {new_parent_guid}\n")
                    handled_child_guids.add(project_guid)
            in_nested = False
            new_nested_lines.append(line)
            continue
        if in_nested:
            m = re.match(r"(\s*)(\{[^}]+\})(\s*=\s*)(\{[^}]+\})(.*)", line)
            if m:
                child_guid = m.group(2)
                # Check if this child is being reassigned
                reassigned = next(
                    (r for r in reassignments if r[1] == child_guid), None
                )
                if reassigned:
                    _, _, _, new_folder = reassigned
                    new_parent_guid = _folder_guid(new_folder)
                    new_nested_lines.append(
                        f"{m.group(1)}{child_guid}{m.group(3)}{new_parent_guid}{m.group(5)}\n"
                    )
                    handled_child_guids.add(child_guid)
                    continue
            new_nested_lines.append(line)
        else:
            new_nested_lines.append(line)

    sln_text = "".join(new_nested_lines)
    sln_path.write_text(sln_text, encoding="utf-8")

    return changes, warnings, 0
