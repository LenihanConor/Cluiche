"""Orchestration for dia env verify command."""
import json
import sys
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_REPO_ROOT = find_repo_root(__file__)

_STATUS_SYMBOLS = {"pass": "PASS", "warn": "WARN", "fail": "FAIL"}
_STATUS_COLORS = {"pass": "\033[32m", "warn": "\033[33m", "fail": "\033[31m"}
_RESET = "\033[0m"


def _color(status: str, text: str, use_color: bool) -> str:
    if not use_color:
        return text
    return f"{_STATUS_COLORS.get(status, '')}{text}{_RESET}"


def run(
    repo_root: Optional[Path],
    toolchain: bool,
    deps_only: bool,
    submodules: bool,
    docker_only: bool,
    claude: bool,
    output_json: bool,
    quiet: bool,
) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT
    use_color = sys.stdout.isatty() and not output_json

    run_all = not any([toolchain, deps_only, submodules, docker_only, claude])
    checks = []

    if run_all or toolchain or docker_only:
        from utils.toolchain_verify import check_all_toolchain, check_docker
        if docker_only and not toolchain:
            checks.extend(check_docker())
        else:
            checks.extend(check_all_toolchain())

    if run_all or deps_only:
        from utils.deps_verify import check_deps
        dep_results = check_deps(root)
        # Exit 3 if deps.json missing entirely
        if len(dep_results) == 1 and dep_results[0].name == "deps.json" and dep_results[0].status == "fail":
            print(f"ERROR: {dep_results[0].detail}")
            return 3
        checks.extend(dep_results)

    if run_all or submodules:
        from utils.submodule_verify import check_submodules
        checks.extend(check_submodules(root))

    if run_all or claude:
        checks.extend(_check_claude(root))

    pass_count = sum(1 for c in checks if c.status == "pass")
    warn_count = sum(1 for c in checks if c.status == "warn")
    fail_count = sum(1 for c in checks if c.status == "fail")

    if output_json:
        output = {
            "result": "fail" if fail_count else ("warn" if warn_count else "pass"),
            "pass": pass_count,
            "warn": warn_count,
            "fail": fail_count,
            "checks": [
                {
                    "category": c.category,
                    "name": c.name,
                    "status": c.status,
                    "detail": c.detail,
                    **({"fix": c.fix} if c.fix else {}),
                }
                for c in checks
            ],
        }
        print(json.dumps(output, indent=2))
    else:
        if not quiet:
            print("[dia env verify]\n")
        # Group by category
        categories = {}
        for c in checks:
            categories.setdefault(c.category, []).append(c)
        for cat, items in categories.items():
            if not quiet:
                print(f"{cat.capitalize()}")
            for item in items:
                sym = _STATUS_SYMBOLS[item.status]
                colored_sym = _color(item.status, sym, use_color)
                if quiet and item.status == "pass":
                    continue
                line = f"  {item.name:<30} {colored_sym}"
                if item.detail:
                    line += f"  {item.detail}"
                print(line)
                if item.fix and item.status != "pass":
                    print(f"{'':34} Fix: {item.fix}")
            if not quiet:
                print()

        result_sym = "FAIL" if fail_count else ("WARN" if warn_count else "OK")
        result_col = _color("fail" if fail_count else ("warn" if warn_count else "pass"), result_sym, use_color)
        print(f"Result: {fail_count} FAIL, {warn_count} WARN, {pass_count} PASS  ({result_col})")

    if fail_count:
        return 1
    if warn_count:
        return 2
    return 0


def _check_claude(repo_root: Path) -> list:
    from utils.check_result import CheckResult
    results = []
    settings = repo_root / ".claude" / "settings.local.json"
    template = repo_root / ".claude" / "settings.local.template.json"

    if not settings.exists():
        results.append(CheckResult("settings.local.json", "claude", "fail",
                                   "missing", "dia env claude-setup"))
    else:
        if template.exists():
            try:
                import json as _json
                tmpl_keys = set(_json.loads(template.read_text()).keys())
                local_keys = set(_json.loads(settings.read_text()).keys())
                missing = tmpl_keys - local_keys
                if missing:
                    results.append(CheckResult("settings.local.json", "claude", "warn",
                                               f"missing keys from template: {missing}",
                                               "dia env claude-setup --force"))
                else:
                    results.append(CheckResult("settings.local.json", "claude", "pass"))
            except Exception:
                results.append(CheckResult("settings.local.json", "claude", "warn", "unreadable"))
        else:
            results.append(CheckResult("settings.local.json", "claude", "pass"))

    # Check symlink
    slug = str(repo_root).replace("\\", "-").replace(":", "-").replace("/", "-")
    import os
    profile_memory = Path(os.environ.get("USERPROFILE", "C:/Users/user")) / ".claude" / "projects" / slug / "memory"
    repo_memory = repo_root / ".claude" / "projects" / slug / "memory"

    if profile_memory.is_symlink():
        target = Path(os.readlink(str(profile_memory)))
        if target == repo_memory or target.resolve() == repo_memory.resolve():
            results.append(CheckResult("memory symlink", "claude", "pass"))
        else:
            results.append(CheckResult("memory symlink", "claude", "warn",
                                       f"symlink target mismatch: {target}",
                                       "dia env claude-setup --force"))
    elif profile_memory.exists():
        results.append(CheckResult("memory symlink", "claude", "warn",
                                   "directory exists but is not a symlink",
                                   "dia env claude-setup"))
    else:
        results.append(CheckResult("memory symlink", "claude", "fail",
                                   "not configured",
                                   "dia env claude-setup"))
    return results
