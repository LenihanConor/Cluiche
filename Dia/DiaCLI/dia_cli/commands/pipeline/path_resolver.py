"""$(OutDir) / $(Configuration) / $(Platform) resolution."""
from pathlib import Path
from typing import Optional


def resolve_out_dir(repo_root: Path, config: str, platform: str, app_name: Optional[str] = None) -> Path:
    if app_name:
        return repo_root / "Cluiche" / "bin" / app_name / config / platform
    return repo_root / "Cluiche" / "bin" / "sharedlibs" / config / platform


def resolve_variables(path: str, config: str, platform: str, repo_root: Path, app_name: Optional[str] = None) -> str:
    out_dir = str(resolve_out_dir(repo_root, config, platform, app_name)).replace("\\", "/") + "/"
    return (
        path
        .replace("$(OutDir)", out_dir)
        .replace("$(Configuration)", config)
        .replace("$(Platform)", platform)
    )
