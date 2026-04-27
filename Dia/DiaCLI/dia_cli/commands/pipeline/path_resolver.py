"""$(OutDir) / $(Configuration) / $(Platform) resolution."""
from pathlib import Path


def resolve_out_dir(repo_root: Path, config: str, platform: str) -> Path:
    return repo_root / "Cluiche" / "bin" / config / platform


def resolve_variables(path: str, config: str, platform: str, repo_root: Path) -> str:
    out_dir = str(resolve_out_dir(repo_root, config, platform)).replace("\\", "/") + "/"
    return (
        path
        .replace("$(OutDir)", out_dir)
        .replace("$(Configuration)", config)
        .replace("$(Platform)", platform)
    )
