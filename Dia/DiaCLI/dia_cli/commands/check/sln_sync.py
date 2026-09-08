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
    "foundation/maths/tools": "1.1-Maths-Tools",
    "foundation/services":    "1.1-Services",
    "foundation/services/tools": "1.1-Services-Tools",
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
    "domain/gameplay/core":   "3.0-Gameplay",
    "domain/gameplay/tools":  "3.1-Gameplay-Tools",
    "tools/editor":           "4.0-Editors",
    "tools/inspector":        "4.0-Inspectors",
}

# Folder → parent-folder name, for nesting inside the .sln tree. Anything not
# listed here nests directly under the top-level "Dia" folder — mirrors the
# existing convention where an X.1-tools folder nests under its X.0 sibling,
# and Foundation sub-groups nest under 1.0-Core.
_FOLDER_PARENT: dict[str, str] = {
    "1.1-Maths":            "1.0-Core",
    "1.1-Maths-Tools":      "1.1-Maths",
    "1.1-Services":         "1.0-Core",
    "1.1-Services-Tools":   "1.1-Services",
    "1.2-Application":      "1.0-Core",
    "1.2-Platform":         "1.0-Core",
    "2.1-Assets-Tools":     "2.0-Assets",
    "3.1-Visual-Tools":     "3.0-Visual",
    "3.1-Physics-Tools":    "3.0-Physics",
    "3.1-Animation-Tools":  "3.0-Animation",
    "3.1-Gameplay-Tools":   "3.0-Gameplay",
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


def _parse_project_entries(sln_text: str) -> dict[str, tuple[str, str]]:
    """Return {project_name: (vcxproj_rel_path, guid)} for all C++ projects."""
    pattern = re.compile(
        r'Project\("' + re.escape(_CPP_PROJECT_TYPE) + r'"\)\s*=\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"(\{[^}]+\})"'
    )
    return {m.group(1): (m.group(2), m.group(3)) for m in pattern.finditer(sln_text)}


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


# ---------------------------------------------------------------------------
# Target-layout doc generation
# ---------------------------------------------------------------------------

_FOLDER_GROUP_NAMES = {"1": "Foundation", "2": "Assets", "3": "Domains", "4": "Cross-Cutting Tools"}

# One folder directly under Dia/, one file — regardless of whether the
# vcxproj filename matches the folder name (renames sometimes leave them
# out of sync, e.g. DiaEntityTemplateEditor/DiaBlueprintEditor.vcxproj).
_TOP_LEVEL_DIA_PROJECT_RE = re.compile(r"^\.\./Dia/[^/]+/[^/]+\.vcxproj$")


def _folder_group(folder_name: str) -> str:
    major = folder_name.split(".", 1)[0]
    return _FOLDER_GROUP_NAMES.get(major, "Other")


def _folder_sort_key(folder_name: str) -> tuple[float, str]:
    try:
        return (float(folder_name.split("-", 1)[0]), folder_name)
    except ValueError:
        return (999.0, folder_name)


def generate_folders_doc(repo_root: Path) -> str:
    """Render the current layer → folder assignment as markdown.

    Fully derived from the .sln + module docs each call — nothing here is
    hand-maintained, so it can't drift the way a manually edited doc can.
    """
    sln_path = repo_root / "Cluiche" / "Cluiche.sln"
    sln_text = sln_path.read_text(encoding="utf-8")

    project_to_folder, _, _ = compute_assignments(repo_root)
    entries = _parse_project_entries(sln_text)
    existing_folders = _parse_folder_guids(sln_text)
    existing_nested = _parse_nested_projects(sln_text)
    folder_guid_to_name = {guid: name for name, guid in existing_folders.items()}

    managed = set(project_to_folder.keys())
    unassigned: list[tuple[str, str]] = []
    non_dia: list[tuple[str, str]] = []
    for name, (vcxproj_rel, guid) in entries.items():
        if name in managed:
            continue
        current_folder = folder_guid_to_name.get(existing_nested.get(guid), "<none>")
        norm = vcxproj_rel.replace("\\", "/")
        if _TOP_LEVEL_DIA_PROJECT_RE.match(norm):
            unassigned.append((name, current_folder))
        else:
            non_dia.append((name, current_folder))
    unassigned.sort()

    by_folder: dict[str, list[str]] = {}
    for name, folder in project_to_folder.items():
        by_folder.setdefault(folder, []).append(name)

    groups: dict[str, list[str]] = {}
    for folder in by_folder:
        groups.setdefault(_folder_group(folder), []).append(folder)

    lines = [
        "# DiaArchitecture — Target Solution Folder Layout",
        "",
        "**Parent:** [diaarchitecture.md](diaarchitecture.md)",
        "",
        "_Auto-generated by `dia check sln-sync`. Do not edit manually._",
        "",
        "Current assignment of every Dia `.vcxproj` to a numbered Visual Studio solution folder, derived from the `layer:` field on each module doc.",
        "",
        "---",
        "",
    ]

    ordered_group_names = list(dict.fromkeys(_FOLDER_GROUP_NAMES.values())) + ["Other"]
    for group_name in ordered_group_names:
        folder_names = groups.get(group_name)
        if not folder_names:
            continue
        lines.append(f"## {group_name}")
        lines.append("")
        for folder in sorted(folder_names, key=_folder_sort_key):
            lines.append(f"### {folder}")
            lines.append(" ".join(f"`{p}`" for p in sorted(by_folder[folder])))
            lines.append("")
        lines.append("---")
        lines.append("")

    if unassigned:
        lines += [
            "## Unassigned (no module doc / recognized layer yet)",
            "",
            "These stay in their current `.sln` folder until a module doc with a "
            "recognized `layer:` value exists. `dia check sln-sync` will move them "
            "automatically once it does.",
            "",
            "| Project | Current folder |",
            "|---------|-----------------|",
        ]
        for name, folder in unassigned:
            lines.append(f"| {name} | {folder} |")
        lines.append("")
        lines.append("---")
        lines.append("")

    lines += [
        "## Non-Dia projects (not managed by `sln-sync`)",
        "",
        "Not a top-level `Dia/<Name>/<Name>.vcxproj` library module (executables, "
        "editors, nested sub-projects) — kept in whatever folder they're currently nested under:",
        "",
        "| Project | Folder |",
        "|---------|--------|",
    ]
    for name, folder in sorted(non_dia):
        lines.append(f"| {name} | {folder} |")
    lines.append("")

    return "\n".join(lines)


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

    # GUID to use for every folder name — the folder's real .sln GUID if it
    # already exists (folders are often created by hand with arbitrary GUIDs,
    # not the deterministic uuid5 one), otherwise the deterministic GUID that
    # will be declared below for a brand-new folder.
    folder_guids: dict[str, str] = dict(existing_folders)
    for folder_name in folders_to_create:
        folder_guids[folder_name] = _folder_guid(folder_name)

    # Determine which folder→folder nestings need to change — new folders
    # need a parent assigned, and existing folders may have drifted (e.g.
    # created without one). Anything not in _FOLDER_PARENT nests under "Dia".
    dia_folder_guid = existing_folders.get("Dia")
    folder_reassignments: list[tuple[str, str, str, str]] = []  # (folder_name, folder_guid, old_parent_name, new_parent_name)
    if dia_folder_guid:
        folder_guids.setdefault("Dia", dia_folder_guid)
        for folder_name in sorted(folders_needed):
            desired_parent_name = _FOLDER_PARENT.get(folder_name, "Dia")
            desired_parent_guid = folder_guids.get(desired_parent_name)
            if not desired_parent_guid:
                warnings.append(f"WARN: parent folder '{desired_parent_name}' for '{folder_name}' not found (skipping nest)")
                continue
            child_guid = folder_guids[folder_name]
            current_parent_guid = existing_nested.get(child_guid)
            if current_parent_guid != desired_parent_guid:
                current_parent_name = next(
                    (name for name, guid in existing_folders.items() if guid == current_parent_guid),
                    "<none>"
                ) if current_parent_guid else "<none>"
                folder_reassignments.append((folder_name, child_guid, current_parent_name, desired_parent_name))
    else:
        warnings.append("WARN: top-level 'Dia' solution folder not found — cannot nest new layer folders")

    # Determine which project nested assignments need to change
    project_reassignments: list[tuple[str, str, str, str]] = []  # (project_name, project_guid, old_folder_name, new_folder_name)
    for project_name, desired_folder in sorted(project_to_folder.items()):
        project_guid = existing_projects.get(project_name)
        if not project_guid:
            warnings.append(f"WARN: project '{project_name}' not found in .sln (skipping)")
            continue

        desired_folder_guid = folder_guids[desired_folder]
        current_parent_guid = existing_nested.get(project_guid)
        current_folder_name = next(
            (name for name, guid in existing_folders.items() if guid == current_parent_guid),
            "<none>"
        ) if current_parent_guid else "<none>"

        if current_parent_guid != desired_folder_guid:
            project_reassignments.append((project_name, project_guid, current_folder_name, desired_folder))

    reassignments = folder_reassignments + project_reassignments

    if not folders_to_create and not reassignments:
        changes.append("Solution folders already in sync — no changes needed.")
        if not dry_run:
            _write_folders_doc(repo_root, changes)
        return changes, warnings, 0

    # Report what will change
    for folder_name in sorted(folders_to_create):
        changes.append(f"CREATE folder: {folder_name}")
    for folder_name, _, old_parent, new_parent in folder_reassignments:
        changes.append(f"NEST   {folder_name}: {old_parent} → {new_parent}")
    for project_name, _, old_folder, new_folder in project_reassignments:
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
                    new_parent_guid = folder_guids[new_folder]
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
                    new_parent_guid = folder_guids[new_folder]
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

    _write_folders_doc(repo_root, changes)

    return changes, warnings, 0


def _write_folders_doc(repo_root: Path, changes: list[str]) -> None:
    doc_path = (
        repo_root / "docs" / "specs" / "applications" / "dia" / "systems"
        / "diaarchitecture" / "diaarchitecture.folders.md"
    )
    doc_path.write_text(generate_folders_doc(repo_root), encoding="utf-8")
    changes.append(f"Regenerated {doc_path.relative_to(repo_root).as_posix()}")
