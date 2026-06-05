"""Stage orchestration loop with OutputContext event emission."""
import json
from pathlib import Path
from typing import Optional

from loguru import logger
from rich.style import Style
from rich.text import Text

from .pipeline_config import PipelineConfig, VALID_STAGES
from .stages import (
    compile_code_stage,
    reflect_stage,
    asset_build_stage,
    package_stage,
    static_analysis_stage,
)

_STYLE_HINT  = Style(color="yellow")
_STYLE_ERROR = Style(color="red")
_STYLE_DIM   = Style(dim=True)

# Sub-systems that each pipeline stage writes logs to.
_STAGE_SUBSYSTEMS = {
    "build-assets": "asset-pipeline",
}

_TAIL_LINES = 5


def _print_failure_hint(stage_name: str, output) -> None:
    """After a stage failure, inline the last error lines and log path."""
    subsystem = _STAGE_SUBSYSTEMS.get(stage_name)
    if subsystem is None:
        return

    log_path = output.log_path_for(subsystem)

    # Collect last N lines and any error-level events from the log.
    errors: list[str] = []
    tail: list[str] = []
    if log_path.exists():
        try:
            lines = log_path.read_text(encoding="utf-8").splitlines()
            tail = lines[-_TAIL_LINES:]
            for raw in lines:
                try:
                    ev = json.loads(raw)
                    if ev.get("event") == "OnAssetFailed":
                        asset_id = ev.get("assetId", "?")
                        phase = ev.get("phase", "?")
                        msgs = [e.get("message", "") for e in ev.get("errors", [])]
                        errors.append(f"  {asset_id} [{phase}]: {'; '.join(msgs)}")
                    elif ev.get("level") == "error":
                        errors.append(f"  {ev.get('message', '')}")
                except (json.JSONDecodeError, AttributeError):
                    pass
        except OSError:
            pass

    console = output._console
    console.print(Text("  ── build-assets detail ──────────────────────", style=_STYLE_DIM))
    if errors:
        for line in errors:
            console.print(Text(line, style=_STYLE_ERROR))
    elif tail:
        for raw in tail:
            try:
                ev = json.loads(raw)
                msg = ev.get("message") or ev.get("event", raw)
            except (json.JSONDecodeError, AttributeError):
                msg = raw
            console.print(Text(f"  {msg}", style=_STYLE_DIM))
    console.print(Text(f"  Full log: {log_path}", style=_STYLE_HINT))

_STAGE_ORDER = ["compile-code", "reflect", "build-assets", "deploy", "static-analysis"]

def _get_handler(stage_name):
    return {
        "compile-code": compile_code_stage.run,
        "reflect":      reflect_stage.run,
        "build-assets": asset_build_stage.run,
        "deploy":       package_stage.run,
        "static-analysis": static_analysis_stage.run,
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

    try:
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
                    output=output,
                    system=system,
                )
            except Exception as e:
                output.stage_failed(system=system, stage=stage_name, error=str(e))
                _print_failure_hint(stage_name, output)
                fail_count += 1
                break

            if exit_code == 0:
                output.stage_completed(system=system, stage=stage_name)
                pass_count += 1
            else:
                output.stage_failed(system=system, stage=stage_name, error=f"exit {exit_code}")
                _print_failure_hint(stage_name, output)
                fail_count += 1
                break
    except KeyboardInterrupt:
        output.run_failed(system=system, pass_count=pass_count, fail_count=1)
        raise

    if fail_count == 0:
        output.run_completed(system=system, pass_count=pass_count, fail_count=0)
        return 0
    else:
        output.run_failed(system=system, pass_count=pass_count, fail_count=fail_count)
        return 1
