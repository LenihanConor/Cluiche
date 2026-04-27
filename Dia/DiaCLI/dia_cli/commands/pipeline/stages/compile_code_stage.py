"""compile-code stage: runs msbuild on the target's .vcxproj."""
import subprocess
import sys
from pathlib import Path

from loguru import logger

from ..pipeline_config import PipelineConfig

_VSWHERE = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")


def _find_msbuild(toml_override: str | None) -> Path | None:
    if toml_override:
        p = Path(toml_override)
        return p if p.exists() else None

    if not _VSWHERE.exists():
        return None

    result = subprocess.run(
        [str(_VSWHERE), "-latest", "-requires", "Microsoft.Component.MSBuild",
         "-find", r"MSBuild\**\Bin\MSBuild.exe"],
        capture_output=True, text=True,
    )
    if result.returncode == 0:
        line = result.stdout.strip().splitlines()[0] if result.stdout.strip() else ""
        if line:
            p = Path(line)
            return p if p.exists() else None
    return None


def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path) -> int:
    msbuild = _find_msbuild(config.global_cfg.msbuild_path)
    if msbuild is None:
        logger.error("msbuild not found — run `dia env verify` to check your VS 2022 installation")
        return 1

    project_rel = config.targets[target].project
    project_path = repo_root / project_rel
    if not project_path.exists():
        logger.error(f"Project not found: {project_path}")
        return 1

    platform = config.global_cfg.default_platform
    cmd = [
        str(msbuild),
        str(project_path),
        f"/p:Configuration={build_config}",
        f"/p:Platform={platform}",
        "/m",
        "/v:minimal",
        "/nologo",
    ]

    logger.info(f"compile-code: msbuild {project_rel} [{build_config}|{platform}]")
    try:
        result = subprocess.run(cmd, stdout=sys.stdout, stderr=sys.stderr, timeout=600)
        return result.returncode
    except FileNotFoundError:
        logger.error(f"msbuild not found")
        return 1
    except subprocess.TimeoutExpired:
        logger.error("msbuild timed out after 10 minutes")
        return 1
