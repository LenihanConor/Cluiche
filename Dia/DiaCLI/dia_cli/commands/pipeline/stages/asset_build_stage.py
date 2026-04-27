"""asset-build stage: no-op stub."""
from pathlib import Path

from loguru import logger

from ..pipeline_config import PipelineConfig


def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path) -> int:
    logger.info("asset-build: skipped (not yet implemented)")
    return 0
