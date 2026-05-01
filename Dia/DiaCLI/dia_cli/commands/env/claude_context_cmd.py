"""Template generation + memory symlink creation for dia env claude-setup."""
import json
import os
import shutil
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_REPO_ROOT = find_repo_root(__file__)


def run(repo_root: Optional[Path], force: bool) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT
    exit_code = 0

    # Step 1: Generate settings.local.json from template
    template = root / ".claude" / "settings.local.template.json"
    target = root / ".claude" / "settings.local.json"

    if not template.exists():
        print(f"ERROR: template not found at {template}")
        return 1

    if target.exists() and not force:
        print(f"settings.local.json already exists (use --force to overwrite)")
    else:
        shutil.copy2(str(template), str(target))
        print(f"settings.local.json generated from template")

    # Step 2: Create memory symlink
    slug = str(root).replace("\\", "-").replace(":", "-").replace("/", "-")
    profile_memory = Path(os.environ.get("USERPROFILE", "C:/Users/user")) / ".claude" / "projects" / slug / "memory"
    repo_memory = root / ".claude" / "projects" / slug / "memory"

    # Ensure repo memory dir exists
    repo_memory.mkdir(parents=True, exist_ok=True)

    if profile_memory.is_symlink():
        current_target = Path(os.readlink(str(profile_memory)))
        if current_target.resolve() == repo_memory.resolve():
            print("Memory symlink already configured correctly")
            return exit_code
        elif force:
            profile_memory.unlink()
        else:
            print(f"WARN: memory symlink points to different target: {current_target}")
            print(f"Use --force to re-create. Target should be: {repo_memory}")
            return 2

    if profile_memory.exists() and not profile_memory.is_symlink():
        print(f"WARN: {profile_memory} is a directory, not a symlink — will not overwrite")
        return 2

    profile_memory.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.symlink(str(repo_memory), str(profile_memory))
        print(f"Memory symlink created: {profile_memory} -> {repo_memory}")
    except OSError as e:
        print(f"ERROR: could not create symlink: {e}")
        print("Enable Developer Mode or run as Administrator to create symlinks on Windows")
        exit_code = 1

    return exit_code
