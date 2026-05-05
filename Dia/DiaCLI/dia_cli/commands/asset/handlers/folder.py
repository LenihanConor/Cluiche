from __future__ import annotations

import shutil
from pathlib import Path
from typing import TYPE_CHECKING

from ..handler import AssetError, AssetHandler, DeployResult, TransformResult

if TYPE_CHECKING:
    from ..context import BuildContext


class FolderHandler(AssetHandler):
    type_id = "folder"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        source_path = Path(record.get("source_path", ""))
        if not source_path.is_dir():
            return [AssetError(
                asset_id=record.get("id", ""),
                phase="validate",
                message=f"Folder asset source is not a directory: {source_path}",
            )]
        return []

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        return TransformResult(success=True, output_path=record.get("source_path"))

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        from ..layout import resolve_deploy_path
        try:
            deploy_path = resolve_deploy_path(record, context)
            deploy_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(record["source_path"], str(deploy_path), dirs_exist_ok=True)
            return DeployResult(success=True, deploy_path=str(deploy_path))
        except Exception as exc:
            return DeployResult(
                success=False,
                errors=[AssetError(asset_id=record.get("id", ""), phase="deploy", message=str(exc))],
            )
