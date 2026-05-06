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


def resolve_deploy_path(record: dict, context: "BuildContext") -> Path:
    """Resolve the full output path for an asset from its scope and category tag.

    Scope rule:
        global -> <deploy_root>/global/<category>/
        stage  -> <deploy_root>/stages/<stage_name>/<category>/

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
