"""deploy stage: runs ui_builds then copies runtime files per pipeline.toml rules."""
import glob
import json
import shutil
import subprocess
from pathlib import Path
from typing import Optional

from loguru import logger

from ..pipeline_config import PipelineConfig, DeployFile
from ..path_resolver import resolve_variables


def _app_name_for_target(config: PipelineConfig, target: str) -> str:
    """Derive the MSBuild project name from the target's vcxproj filename."""
    return Path(config.targets[target].project).stem


def _copy_files(
    rules: list[DeployFile],
    build_config: str,
    platform: str,
    out_dir: Optional[Path],
    force: bool,
    repo_root: Path,
    app_name: Optional[str] = None,
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
                dest_pat = resolve_variables(rule.dest, build_config, platform, repo_root, app_name)
            matched = glob.glob(str(repo_root / src_pat), recursive=True)
            if not matched:
                logger.warning(f"deploy: no files matched {src_pat}")
                continue
            dest_dir = Path(dest_pat)
            dest_dir.mkdir(parents=True, exist_ok=True)
            # For ** patterns, preserve relative paths from the glob base directory.
            # For * patterns, copy flat (file name only) into dest_dir.
            preserve_rel = "**" in src_pat
            if preserve_rel:
                src_pat_str = str(repo_root / src_pat)
                wildcard_pos = next((i for i, c in enumerate(src_pat_str) if c in "*?"), len(src_pat_str))
                glob_base = Path(src_pat_str[:wildcard_pos].rstrip("/\\"))
            for src_file in matched:
                src_path = Path(src_file)
                if src_path.is_dir():
                    continue
                if preserve_rel:
                    try:
                        rel = src_path.relative_to(glob_base)
                        dest_file = dest_dir / rel
                    except ValueError:
                        dest_file = dest_dir / src_path.name
                else:
                    dest_file = dest_dir / src_path.name
                dest_file.parent.mkdir(parents=True, exist_ok=True)
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
        output.step_completed(system=system, stage=stage, step="copy-files")
    return 0


def _build_env_with_node() -> dict:
    """Return a copy of os.environ with node's directory on PATH if needed."""
    import os
    import shutil
    if shutil.which("node"):
        return None  # already on PATH, use inherited env
    from dia_cli.utils.node_resolve import node_dir
    ndir = node_dir()
    if ndir is None:
        return None
    env = os.environ.copy()
    env["PATH"] = str(ndir) + os.pathsep + env.get("PATH", "")
    return env


def _run_ui_builds(ui_builds, repo_root: Path, output=None, system: str = "pipeline", stage: str = "deploy") -> int:
    if output:
        output.step_started(system=system, stage=stage, step="ui-builds")
    env = _build_env_with_node()
    for entry in ui_builds:
        cwd = repo_root / entry.cwd
        logger.info(f"deploy: ui_build in {entry.cwd}: {entry.cmd}")
        result = subprocess.run(entry.cmd, cwd=str(cwd), shell=True, env=env)
        if result.returncode != 0:
            err = f"ui_build failed (exit {result.returncode}): {entry.cmd}"
            logger.error(f"deploy: {err}")
            if output:
                output.step_failed(system=system, stage=stage, step="ui-builds", error=err)
            return result.returncode
    if output:
        output.step_completed(system=system, stage=stage, step="ui-builds")
    return 0


def _is_staged(rules, build_config: str, platform: str, out_dir: Optional[Path], repo_root: Path, app_name: Optional[str] = None) -> bool:
    for rule in rules:
        src_pat = resolve_variables(rule.src, build_config, platform, repo_root)
        if out_dir is not None:
            dest_pat = str(out_dir / resolve_variables(
                rule.dest.replace("$(OutDir)", ""), build_config, platform, repo_root
            ).lstrip("/\\"))
        else:
            dest_pat = resolve_variables(rule.dest, build_config, platform, repo_root, app_name)
        matched = glob.glob(str(repo_root / src_pat), recursive=True)
        for src_file in matched:
            src_path = Path(src_file)
            if src_path.is_dir():
                continue
            dest_file = Path(dest_pat) / src_path.name
            if not dest_file.exists() or src_path.stat().st_mtime > dest_file.stat().st_mtime:
                return False
    return True


def _derive_diastage_deploy_rules(deploy_files: list[DeployFile], repo_root: Path) -> list[DeployFile]:
    """Auto-generate DeployFile rules for .diastage files by reading the .diagame imports.

    Finds the .diagame file in the existing deploy rules, reads its imports, and returns
    a DeployFile for each import with type "stage" that is not already covered by an
    explicit rule.

    Import paths in .diagame are relative to the deployed assets root, e.g.:
      "stages/AssetRuntimeTestStage/asset_runtime_stage.diastage"
    maps to:
      src  = "Cluiche/Assets/Stages/AssetRuntimeTestStage/asset_runtime_stage.diastage"
      dest = "$(OutDir)assets/stages/AssetRuntimeTestStage/"
    """
    diagame_src = None
    for rule in deploy_files:
        if rule.src.endswith(".diagame"):
            diagame_src = rule.src
            break
    if diagame_src is None:
        return []

    diagame_path = repo_root / diagame_src
    if not diagame_path.exists():
        return []

    try:
        diagame = json.loads(diagame_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return []

    rules = []
    for imp in diagame.get("imports", []):
        if imp.get("type") != "stage":
            continue
        # path like "stages/AssetRuntimeTestStage/asset_runtime_stage.diastage"
        path = imp.get("path", "")
        parts = path.split("/")
        if len(parts) < 3 or not parts[-1].endswith(".diastage"):
            continue
        stage_dir = parts[1]          # e.g. "AssetRuntimeTestStage"
        diastage_file = parts[-1]     # e.g. "asset_runtime_stage.diastage"
        src = f"Cluiche/Assets/Stages/{stage_dir}/{diastage_file}"
        dest = f"$(OutDir)assets/stages/{stage_dir}/"
        # Only add if not already covered by an explicit rule
        if not any(r.src == src for r in deploy_files):
            rules.append(DeployFile(src=src, dest=dest))

    return rules


def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path, output=None, system: str = "pipeline") -> int:
    """Called by the pipeline runner (uses path_resolver for $(OutDir))."""
    stage = "deploy"
    deploy = config.targets[target].deploy
    platform = config.global_cfg.default_platform
    app_name = _app_name_for_target(config, target)

    if deploy.ui_builds:
        rc = _run_ui_builds(deploy.ui_builds, repo_root, output=output, system=system, stage=stage)
        if rc != 0:
            return rc

    # Auto-derive .diastage deploy rules from .diagame imports
    all_files = list(deploy.files) + _derive_diastage_deploy_rules(deploy.files, repo_root)

    if not force and _is_staged(all_files, build_config, platform, None, repo_root, app_name):
        logger.info("deploy: already staged (use --force to re-copy)")
        return 0

    return _copy_files(all_files, build_config, platform, None, force, repo_root, app_name, output=output, system=system, stage=stage)


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
