"""reflect stage: run --dump-schema on the game binary and write registeredtypes.diaschema."""
from pathlib import Path

from loguru import logger

from ..pipeline_config import PipelineConfig
from ..path_resolver import resolve_out_dir
from ...reflect.reflect_handler import handle_reflect


def run(config: PipelineConfig, target: str, build_config: str, force: bool,
        repo_root: Path, output=None, system: str = "pipeline") -> int:
    stage = "reflect"
    target_cfg = config.targets.get(target)
    if target_cfg is None:
        err = f"unknown target '{target}'"
        logger.error(f"reflect: {err}")
        if output:
            output.stage_failed(system=system, stage=stage, error=err)
        return 1

    if output:
        output.step_started(system=system, stage=stage, step="dump-schema",
                            detail=f"{target} [{build_config}]")

    logger.info(f"reflect: generating schema for target '{target}' [{build_config}]")

    rc = handle_reflect(
        repo_root=repo_root,
        target_name=target,
        config=build_config,
        is_breaking=False,
    )

    if rc != 0:
        err = f"dia reflect exited {rc}"
        logger.error(f"reflect: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="dump-schema", error=err)
        return rc

    if output:
        output.step_completed(system=system, stage=stage, step="dump-schema")
    return 0
