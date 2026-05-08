from __future__ import annotations

import shutil
from typing import TYPE_CHECKING

from ..handler import AssetError, AssetHandler, DeployResult, TransformResult
from .._resolve import resolve_source

if TYPE_CHECKING:
    from ..context import BuildContext


class FolderHandler(AssetHandler):
    type_id = "folder"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        source_path = resolve_source(record, context)
        if not source_path.is_dir():
            return [AssetError(
                asset_id=record.get("id", ""),
                phase="validate",
                message=f"Folder asset source is not a directory: {source_path}",
            )]
        return []

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        source_path = resolve_source(record, context)
        return TransformResult(success=True, output_path=str(source_path))

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        from ..layout import resolve_deploy_path
        source_path = resolve_source(record, context)
        try:
            deploy_path = resolve_deploy_path(record, context)
            deploy_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(str(source_path), str(deploy_path), dirs_exist_ok=True)
            return DeployResult(success=True, deploy_path=str(deploy_path))
        except Exception as exc:
            return DeployResult(
                success=False,
                errors=[AssetError(asset_id=record.get("id", ""), phase="deploy", message=str(exc))],
            )
