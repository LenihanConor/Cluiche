from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from ..handler import AssetError, AssetHandler, DeployResult, TransformResult

if TYPE_CHECKING:
    from ..context import BuildContext


class DefaultAssetHandler(AssetHandler):
    """Shared validate / copy-as-is transform / layout-engine deploy."""

    file_pattern: str = ""  # e.g. "*.texture.png"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        errors: list[AssetError] = []
        asset_id = record.get("id", "")
        source_path = Path(record.get("source_path", ""))

        if not source_path.exists():
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"Source file not found: {source_path}",
            ))

        # Pattern mismatch is a warning, not an error (AI Q4)
        if self.file_pattern and not source_path.match(self.file_pattern):
            context.output.warn(
                "asset",
                f"[{asset_id}] File '{source_path.name}' does not match expected pattern"
                f" '{self.file_pattern}' — processing anyway",
            )

        return errors

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        return TransformResult(success=True, output_path=record.get("source_path"))

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        from ..layout import copy_asset, resolve_deploy_path
        source = record.get("source_path", "")
        try:
            deploy_path = resolve_deploy_path(record, context)
            copy_asset(source, deploy_path)
            return DeployResult(success=True, deploy_path=str(deploy_path))
        except Exception as exc:
            return DeployResult(
                success=False,
                errors=[AssetError(asset_id=record.get("id", ""), phase="deploy", message=str(exc))],
            )
