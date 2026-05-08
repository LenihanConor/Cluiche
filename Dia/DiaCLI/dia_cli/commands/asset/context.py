from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Protocol, runtime_checkable


@runtime_checkable
class OutputProtocol(Protocol):
    def emit(self, payload: dict) -> None: ...
    def warn(self, system: str, message: str) -> None: ...
    def run_started(self, **kwargs) -> None: ...
    def run_completed(self, **kwargs) -> None: ...
    def run_failed(self, **kwargs) -> None: ...


@dataclass
class BuildContext:
    catalogue: dict               # parsed assets.catalogue.json
    config: str                   # "Debug" | "Release"
    platform: str                 # "x64"
    app_name: str                 # e.g. "CluicheTest"
    deploy_root: Path             # bin/<App>/<Config>/<Platform>/assets/
    asset_stages: list[str]       # Stage asset IDs from pipeline.toml target
    output: OutputProtocol        # OutputContext for NDJSON event logging
    source_root: Path = field(default_factory=Path)  # root for resolving relative source_path
