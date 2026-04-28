"""build-assets stage: no-op stub."""
from pathlib import Path

from loguru import logger

from ..pipeline_config import PipelineConfig


def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path, output=None, system: str = "pipeline") -> int:
    logger.info("build-assets: skipped (not yet implemented)")
    return 0
