from __future__ import annotations

import json
from pathlib import Path
from typing import TYPE_CHECKING

from ..handler import AssetError
from .._resolve import resolve_source
from .default import DefaultAssetHandler

if TYPE_CHECKING:
    from ..context import BuildContext


class StageHandler(DefaultAssetHandler):
    type_id = "stage"
    file_pattern = "*.stage.json"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        errors = super().validate(record, context)

        source_path = resolve_source(record, context)
        if not source_path.exists():
            return errors  # already caught by super

        try:
            stage_data = json.loads(source_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError) as exc:
            errors.append(AssetError(
                asset_id=record.get("id", ""),
                phase="validate",
                message=f"Could not read stage JSON: {exc}",
            ))
            return errors

        name = stage_data.get("name")
        if not name or not isinstance(name, str):
            errors.append(AssetError(
                asset_id=record.get("id", ""),
                phase="validate",
                message="Stage JSON missing required 'name' field (must be a non-empty string)",
            ))

        return errors
