from __future__ import annotations

from typing import TYPE_CHECKING

from ..handler import AssetError, AssetHandler, DeployResult, TransformResult
from .._resolve import resolve_source

if TYPE_CHECKING:
    from ..context import BuildContext


class DefaultAssetHandler(AssetHandler):
    """Shared validate / copy-as-is transform / layout-engine deploy."""

    file_pattern: str = ""  # e.g. "*.texture.png"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        errors: list[AssetError] = []
        asset_id = record.get("id", "")
        source_path = resolve_source(record, context)

        if not source_path.exists():
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"Source file not found: {source_path}",
            ))

        if self.file_pattern and not source_path.match(self.file_pattern):
            context.output.warn(
                "asset",
                f"[{asset_id}] File '{source_path.name}' does not match expected pattern"
                f" '{self.file_pattern}' — processing anyway",
            )

        return errors

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        source_path = resolve_source(record, context)
        return TransformResult(success=True, output_path=str(source_path))

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        from ..layout import copy_asset, resolve_deploy_path
        source_path = resolve_source(record, context)
        try:
            deploy_path = resolve_deploy_path(record, context)
            copy_asset(source_path, deploy_path)
            return DeployResult(success=True, deploy_path=str(deploy_path))
        except Exception as exc:
            return DeployResult(
                success=False,
                errors=[AssetError(asset_id=record.get("id", ""), phase="deploy", message=str(exc))],
            )
