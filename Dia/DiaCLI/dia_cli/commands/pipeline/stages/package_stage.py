"""package stage: copies runtime files per pipeline.toml rules."""
import glob
import shutil
from pathlib import Path

from loguru import logger

from ..pipeline_config import PipelineConfig
from ..path_resolver import resolve_variables


def _is_staged(rules, config: str, platform: str, repo_root: Path) -> bool:
    for rule in rules:
        src_pat = resolve_variables(rule.src, config, platform, repo_root)
        dest_pat = resolve_variables(rule.dest, config, platform, repo_root)
        matched = glob.glob(str(repo_root / src_pat), recursive=True)
        for src_file in matched:
            src_path = Path(src_file)
            if src_path.is_dir():
                continue
            dest_file = Path(dest_pat) / src_path.name
            if not dest_file.exists() or src_path.stat().st_mtime > dest_file.stat().st_mtime:
                return False
    return True


def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path) -> int:
    rules = config.targets[target].package.files
    platform = config.global_cfg.default_platform

    if not force and _is_staged(rules, build_config, platform, repo_root):
        logger.info("package: already staged (use --force to re-copy)")
        return 0

    try:
        for rule in rules:
            src_pat = resolve_variables(rule.src, build_config, platform, repo_root)
            dest_pat = resolve_variables(rule.dest, build_config, platform, repo_root)
            matched = glob.glob(str(repo_root / src_pat), recursive=True)
            if not matched:
                logger.warning(f"package: no files matched {src_pat}")
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
        logger.error(f"package: copy failed: {e}")
        return 1

    return 0
