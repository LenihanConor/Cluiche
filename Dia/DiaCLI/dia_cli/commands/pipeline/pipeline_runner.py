"""Stage orchestration loop with OutputContext event emission."""
from pathlib import Path
from typing import Optional

from loguru import logger

from .pipeline_config import PipelineConfig, VALID_STAGES
from .stages import (
    compile_code_stage,
    asset_build_stage,
    package_stage,
)

_STAGE_ORDER = ["compile-code", "build-assets", "deploy"]

def _get_handler(stage_name):
    return {
        "compile-code": compile_code_stage.run,
        "build-assets": asset_build_stage.run,
        "deploy": package_stage.run,
    }[stage_name]


def run_pipeline(
    config: PipelineConfig,
    target: str,
    stages: list[str],
    build_config: str,
    force: bool,
    output,
    repo_root: Path,
) -> int:
    system = "pipeline"
    output.run_started(
        system=system,
        target=target,
        config=build_config,
        stages=stages,
    )

    pass_count = 0
    fail_count = 0

    for stage_name in _STAGE_ORDER:
        if stage_name not in stages:
            output.stage_skipped(system=system, stage=stage_name, reason="not in active stage list")
            continue

        output.stage_started(system=system, stage=stage_name)
        try:
            handler = _get_handler(stage_name)
            exit_code = handler(
                config=config,
                target=target,
                build_config=build_config,
                force=force,
                repo_root=repo_root,
            )
        except Exception as e:
            output.stage_failed(system=system, stage=stage_name, error=str(e))
            fail_count += 1
            break

        if exit_code == 0:
            output.stage_completed(system=system, stage=stage_name)
            pass_count += 1
        else:
            output.stage_failed(system=system, stage=stage_name, error=f"exit {exit_code}")
            fail_count += 1
            break

    if fail_count == 0:
        output.run_completed(system=system, pass_count=pass_count, fail_count=0)
        return 0
    else:
        output.run_failed(system=system, pass_count=pass_count, fail_count=fail_count)
        return 1
