from __future__ import annotations

import shutil
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .context import BuildContext

# Ordered category resolution — first matching tag wins (SD-CAT-007)
CATEGORY_TAGS = [
    "Presentation/UI",
    "Presentation",
    "misc/ApplicationFlow",
    "characters",
    "environments",
    "gameplay",
    "misc",
]


def resolve_category(tags: list[str]) -> str:
    """Return the first tag matching CATEGORY_TAGS, or 'misc'."""
    for tag in tags:
        if tag in CATEGORY_TAGS:
            return tag
    return "misc"


def compute_world_subfolder(source_path: str) -> str:
    """Return folders between the 'Global' part and the filename ('' if none/absent)."""
    parts = Path(source_path).parts
    if "Global" not in parts:
        return ""
    inner = parts[parts.index("Global") + 1:-1]
    return "/".join(inner)


def world_record_outside_global(record: dict) -> bool:
    """True if a 'world'-tagged record's source_path has no 'Global' folder."""
    return "world" in record.get("tags", []) and "Global" not in Path(record.get("source_path", "")).parts


def resolve_deploy_path(record: dict, context: "BuildContext") -> Path:
    """Resolve the full output path for an asset from its scope and category tag.

    Scope rule:
        global -> <deploy_root>/global/<category>/
        stage  -> <deploy_root>/stages/<stage_name>/<category>/
        global + 'world' tag -> <deploy_root>/global/world/<subfolder under Global/>/
        (stage scope wins over the world tag)

    Folder assets (type 'folder') get a trailing slash appended to signal
    directory-based loading to DiaAssetRuntime (SD-CAT-013).
    """
    scope = record.get("scope", "global")
    tags: list[str] = record.get("tags", [])
    category = resolve_category(tags)

    filename = Path(record.get("source_path", "")).name

    if scope == "stage":
        stage_name = record.get("stage_name", "")
        base = context.deploy_root / "stages" / stage_name / category
    elif "world" in tags:
        source_path = record.get("source_path", "")
        if world_record_outside_global(record):
            context.output.warn(
                "asset",
                f"[{record.get('id', '')}] 'world' asset source_path '{source_path}' has no"
                " 'Global/' folder - deploying flat to global/world/;"
                " move it under a 'Global/<subfolder>/' path to preserve structure",
            )
        sub = compute_world_subfolder(source_path)
        base = context.deploy_root / "global" / "world"
        if sub:
            base = base / sub
    else:
        base = context.deploy_root / "global" / category

    return base / filename


def copy_asset(source: str | Path, dest: Path) -> None:
    """Copy a single file to dest, creating parent directories. Overwrites existing."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(str(source), str(dest))


def copy_asset_directory(source: str | Path, dest: Path) -> None:
    """Recursively copy a directory tree to dest (SD-CAT-013)."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(str(source), str(dest), dirs_exist_ok=True)
