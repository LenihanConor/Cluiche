from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .context import BuildContext


def resolve_source(record: dict, context: "BuildContext") -> Path:
    """Resolve source_path against context.source_root if relative."""
    raw = Path(record.get("source_path", ""))
    if raw.is_absolute():
        return raw
    return context.source_root / raw
