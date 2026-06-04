"""arch_module_map — build a module map from dia.*.architecture.module.md YAML files."""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class ModuleInfo:
    module_id: str
    path: str                      # e.g. "Dia/DiaCore"
    layer: Optional[str]           # e.g. "foundation/core" — None if missing
    forbidden: list[str]           # module_ids listed in dependencies.forbidden
    required: list[str]            # module_ids listed in dependencies.required
    doc_path: str                  # repo-relative path to the module.md file


def build_module_map(repo_root: Path) -> tuple[dict[str, ModuleInfo], list[str]]:
    """Return (module_map, warnings).

    module_map: module_id → ModuleInfo
    warnings: list of human-readable warning strings for modules with missing fields.
    """
    warnings: list[str] = []
    module_map: dict[str, ModuleInfo] = {}

    dia_root = repo_root / "Dia"
    for md_file in sorted(dia_root.rglob("dia.*.architecture.module.md")):
        rel_path = md_file.relative_to(repo_root).as_posix()
        fm = _parse_frontmatter(md_file)
        if fm is None:
            warnings.append(f"WARN: could not parse frontmatter in {rel_path}")
            continue

        module_id = fm.get("module_id")
        if not module_id:
            warnings.append(f"WARN: missing module_id in {rel_path}")
            continue

        path = fm.get("path", "")
        layer = fm.get("layer")
        if not layer:
            warnings.append(f"WARN: missing layer in {rel_path} (module: {module_id})")

        forbidden = fm.get("forbidden", [])
        required = fm.get("required", [])

        module_map[module_id] = ModuleInfo(
            module_id=module_id,
            path=path,
            layer=layer if layer else None,
            forbidden=forbidden,
            required=required,
            doc_path=rel_path,
        )

    return module_map, warnings


def _parse_frontmatter(md_file: Path) -> Optional[dict]:
    """Parse relevant fields from dia.module.v1 YAML frontmatter without PyYAML."""
    try:
        text = md_file.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None

    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return None

    end = -1
    for i in range(1, len(lines)):
        if lines[i].strip() == "---":
            end = i
            break
    if end == -1:
        return None

    fm_lines = lines[1:end]
    result: dict = {"forbidden": [], "required": []}

    # State machine to track which list section we're in
    # Sections: "forbidden", "required", or None
    in_dependencies = False
    current_list: Optional[str] = None  # "forbidden" | "required" | None

    for line in fm_lines:
        stripped = line.strip()

        # Top-level scalar fields (handle multiple key aliases)
        if re.match(r"(module_id|id|module):", line) and not line.startswith("  "):
            result["module_id"] = _scalar(line)
            in_dependencies = False
            current_list = None
            continue
        if line.startswith("path:") and not line.startswith("  "):
            result["path"] = _scalar(line)
            in_dependencies = False
            current_list = None
            continue
        if line.startswith("layer:") and not line.startswith("  "):
            result["layer"] = _scalar(line)
            in_dependencies = False
            current_list = None
            continue

        # Enter dependencies: block
        if line.startswith("dependencies:") and not line.startswith("  "):
            in_dependencies = True
            current_list = None
            continue

        if in_dependencies:
            # Indented sub-keys inside dependencies:
            if re.match(r"  (required|forbidden|optional):", line):
                key = re.match(r"  (required|forbidden|optional):", line).group(1)
                rest = line.split(":", 1)[1].strip()
                if rest == "[]":
                    # empty inline list — already defaulted to []
                    current_list = None
                elif rest == "":
                    current_list = key
                else:
                    current_list = None
                continue

            # List items inside forbidden/required
            if current_list and re.match(r"    - ", line):
                value = line[6:].strip().strip('"').strip("'")
                if value:
                    result.setdefault(current_list, []).append(value)
                continue

            # Unindented line ends dependencies block
            if not line.startswith("  "):
                in_dependencies = False
                current_list = None

    return result


def _scalar(line: str) -> str:
    """Extract the value after the first colon on a YAML line."""
    return line.split(":", 1)[1].strip().strip('"').strip("'")
