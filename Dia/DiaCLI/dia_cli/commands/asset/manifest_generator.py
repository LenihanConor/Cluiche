from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .context import BuildContext


@dataclass
class RuntimeAssetEntry:
    id: str           # type.name composite
    scope: str        # "global" | "stage"
    deploy_path: str  # relative to deploy_root


@dataclass
class RuntimeStageEntry:
    id: str
    assets: list[str] = field(default_factory=list)


class RuntimeManifestGenerator:
    def __init__(self, context: "BuildContext") -> None:
        self._context = context
        self._assets: list[RuntimeAssetEntry] = []
        self._stages: list[RuntimeStageEntry] = []

    def add_asset(self, asset_id: str, scope: str, deploy_path: Path, is_folder: bool = False) -> None:
        """Record a deployed asset. deploy_path is relative to deploy_root.

        Folder assets get a trailing '/' in the manifest deploy_path so
        DiaAssetRuntime knows to use directory-based loading (SD-CAT-013).
        """
        rel = str(deploy_path.relative_to(self._context.deploy_root)).replace("\\", "/")
        if is_folder and not rel.endswith("/"):
            rel += "/"
        self._assets.append(RuntimeAssetEntry(id=asset_id, scope=scope, deploy_path=rel))

    def build_stages(self) -> None:
        """Build stage entries from `contains` edges in the catalogue."""
        deployed_ids = {e.id for e in self._assets}
        for record in self._context.catalogue.get("assets", []):
            asset_type = record.get("type", "")
            if asset_type != "stage":
                continue
            stage_id = record.get("id", "")
            members: list[str] = []
            for ref in record.get("references", []):
                if ref.get("type") == "contains":
                    target = ref.get("target", "")
                    if target in deployed_ids:
                        members.append(target)
            if stage_id:
                self._stages.append(RuntimeStageEntry(id=stage_id, assets=members))

    def generate(self) -> Path:
        """Write assets.runtime.json to deploy_root. Returns the manifest path."""
        manifest = {
            "version": 1,
            "app_name": self._context.app_name,
            "assets": [
                {"id": a.id, "scope": a.scope, "deploy_path": a.deploy_path}
                for a in self._assets
            ],
            "stages": [
                {"id": s.id, "assets": s.assets}
                for s in self._stages
            ],
        }
        out_path = self._context.deploy_root / "assets.runtime.json"
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        return out_path
