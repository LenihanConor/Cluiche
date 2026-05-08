"""Load asset-pipeline target configuration from pipeline.toml."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

import toml


class AssetConfigError(Exception):
    pass


@dataclass
class AssetTargetConfig:
    app_name: str
    catalogue_manifest: str    # relative path to assets.catalogue.json
    asset_stages: list[str] = field(default_factory=list)


def load_asset_target_config(repo_root: Path, target: str) -> AssetTargetConfig:
    """Read pipeline.toml and return the asset-pipeline config for target.

    Raises AssetConfigError with a descriptive message for:
    - missing pipeline.toml
    - parse error
    - unknown target
    - missing catalogue_manifest field
    """
    toml_path = repo_root / "pipeline.toml"
    if not toml_path.exists():
        raise AssetConfigError(f"pipeline.toml not found at {toml_path}")

    try:
        raw = toml.load(str(toml_path))
    except toml.TomlDecodeError as exc:
        raise AssetConfigError(f"pipeline.toml parse error: {exc}") from exc

    targets = raw.get("targets", {})
    if target not in targets:
        known = ", ".join(sorted(targets)) or "(none)"
        raise AssetConfigError(
            f"Target '{target}' not found in pipeline.toml  (known: {known})"
        )

    t = targets[target]
    catalogue_manifest = t.get("catalogue_manifest")
    if not catalogue_manifest:
        raise AssetConfigError(
            f"Target '{target}' in pipeline.toml is missing required field 'catalogue_manifest'"
        )

    return AssetTargetConfig(
        app_name=t.get("app_name", target),
        catalogue_manifest=catalogue_manifest,
        asset_stages=t.get("asset_stages", []),
    )
