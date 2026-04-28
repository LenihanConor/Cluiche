"""deploy stage: runs ui_builds then copies runtime files per pipeline.toml rules."""
import glob
import shutil
import subprocess
from pathlib import Path
from typing import Optional

from loguru import logger

from ..pipeline_config import PipelineConfig, DeployFile
from ..path_resolver import resolve_variables


def _copy_files(
    rules: list[DeployFile],
    build_config: str,
    platform: str,
    out_dir: Optional[Path],
    force: bool,
    repo_root: Path,
    output=None,
    system: str = "pipeline",
    stage: str = "deploy",
) -> int:
    if output:
        output.step_started(system=system, stage=stage, step="copy-files")
    try:
        for rule in rules:
            src_pat = resolve_variables(rule.src, build_config, platform, repo_root)
            if out_dir is not None:
                dest_pat = str(out_dir / resolve_variables(
                    rule.dest.replace("$(OutDir)", ""), build_config, platform, repo_root
                ).lstrip("/\\"))
            else:
                dest_pat = resolve_variables(rule.dest, build_config, platform, repo_root)
            matched = glob.glob(str(repo_root / src_pat), recursive=True)
            if not matched:
                logger.warning(f"deploy: no files matched {src_pat}")
                continue
            dest_dir = Path(dest_pat)
            dest_dir.mkdir(parents=True, exist_ok=True)
            for src_file in matched:
                src_path = Path(src_file)
                if src_path.is_dir():
                    continue
                dest_file = dest_dir / src_path.name
                if force or not dest_file.exists() or src_path.stat().st_mtime > dest_file.stat().st_mtime:
                    shutil.copy2(src_path, dest_file)
                    logger.info(f"  copied {src_path.name} -> {dest_file}")
                else:
                    logger.debug(f"  skip (up to date) {src_path.name}")
    except OSError as e:
        err = f"copy failed: {e}"
        logger.error(f"deploy: {err}")
        if output:
            output.step_failed(system=system, stage=stage, step="copy-files", error=err)
        return 1
    if output:
        output.log(system=system, level="info", message=f"copied {len(rules)} deploy rules", stage=stage)
        output.step_completed(system=system, stage=stage, step="copy-files")
    return 0


def _run_ui_builds(ui_builds, repo_root: Path, output=None, system: str = "pipeline", stage: str = "deploy") -> int:
    if output:
        output.step_started(system=system, stage=stage, step="ui-builds")
    for entry in ui_builds:
        cwd = repo_root / entry.cwd
        logger.info(f"deploy: ui_build in {entry.cwd}: {entry.cmd}")
        if output:
            output.log(system=system, level="info", message=f"ui_build: {entry.cwd}", stage=stage)
        result = subprocess.run(entry.cmd, cwd=str(cwd), shell=True)
        if result.returncode != 0:
            err = f"ui_build failed (exit {result.returncode}): {entry.cmd}"
            logger.error(f"deploy: {err}")
            if output:
                output.step_failed(system=system, stage=stage, step="ui-builds", error=err)
            return result.returncode
    if output:
        output.step_completed(system=system, stage=stage, step="ui-builds")
    return 0


def _is_staged(rules, build_config: str, platform: str, out_dir: Optional[Path], repo_root: Path) -> bool:
    for rule in rules:
        src_pat = resolve_variables(rule.src, build_config, platform, repo_root)
        if out_dir is not None:
            dest_pat = str(out_dir / resolve_variables(
                rule.dest.replace("$(OutDir)", ""), build_config, platform, repo_root
            ).lstrip("/\\"))
        else:
            dest_pat = resolve_variables(rule.dest, build_config, platform, repo_root)
        matched = glob.glob(str(repo_root / src_pat), recursive=True)
        for src_file in matched:
            src_path = Path(src_file)
            if src_path.is_dir():
                continue
            dest_file = Path(dest_pat) / src_path.name
            if not dest_file.exists() or src_path.stat().st_mtime > dest_file.stat().st_mtime:
                return False
    return True


def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path, output=None, system: str = "pipeline") -> int:
    """Called by the pipeline runner (uses path_resolver for $(OutDir))."""
    stage = "deploy"
    deploy = config.targets[target].deploy
    platform = config.global_cfg.default_platform

    if deploy.ui_builds:
        rc = _run_ui_builds(deploy.ui_builds, repo_root, output=output, system=system, stage=stage)
        if rc != 0:
            return rc

    if not force and _is_staged(deploy.files, build_config, platform, None, repo_root):
        logger.info("deploy: already staged (use --force to re-copy)")
        return 0

    return _copy_files(deploy.files, build_config, platform, None, force, repo_root, output=output, system=system, stage=stage)


def run_deploy(
    config: PipelineConfig,
    target: str,
    build_config: str,
    force: bool,
    out_dir: Path,
    repo_root: Path,
) -> int:
    """Called by `dia pipeline deploy <target>` — out_dir supplied by MSBuild $(TargetDir)."""
    stage = "deploy"
    deploy = config.targets[target].deploy
    platform = config.global_cfg.default_platform

    if deploy.ui_builds:
        rc = _run_ui_builds(deploy.ui_builds, repo_root)
        if rc != 0:
            return rc

    if not force and _is_staged(deploy.files, build_config, platform, out_dir, repo_root):
        logger.info("deploy: already staged (use --force to re-copy)")
        return 0

    return _copy_files(deploy.files, build_config, platform, out_dir, force, repo_root)
