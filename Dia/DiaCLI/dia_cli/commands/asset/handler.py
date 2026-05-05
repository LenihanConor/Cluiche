from __future__ import annotations

from dataclasses import dataclass, field
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .context import BuildContext


@dataclass
class AssetError:
    asset_id: str
    phase: str   # "validate" | "transform" | "deploy"
    message: str


@dataclass
class TransformResult:
    success: bool
    output_path: str | None = None
    errors: list[AssetError] = field(default_factory=list)


@dataclass
class DeployResult:
    success: bool
    deploy_path: str | None = None
    errors: list[AssetError] = field(default_factory=list)


class AssetHandler:
    type_id: str = ""

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        return []

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        return TransformResult(success=True)

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        return DeployResult(success=True)
