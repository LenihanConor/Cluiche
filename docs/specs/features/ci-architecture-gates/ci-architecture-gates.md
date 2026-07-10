# Feature Spec: CI Architecture Gates

## Parent System
@docs/specs/applications/dia/dia.md

**Status:** `Done`

---

## Summary

Wire the existing `dia check` tooling into a GitHub Actions CI pipeline so that architecture violations, undeclared dependencies, and manifest errors are caught automatically on every push and PR — not just when a developer remembers to run them locally. Also adds `dia check clones` (PMD CPD) to detect copy-paste duplication, and fixes two pre-existing tool gaps: `dia check deps` exits 0 on violation, and `dia check arch` has no runnable CLI entry point.

---

## Goals

1. Every push to `master` or `Development` (and every PR targeting those branches) triggers the architecture gate job.
2. `dia check deps` exits 1 when missing or stale dependency declarations are found.
3. `dia check arch` is reachable as a `dia check arch` CLI command and exits 1 on violations.
4. `dia check clones` wraps PMD CPD to detect copy-paste duplication across `Dia/` and `Cluiche/`; new duplicates over threshold fail CI; existing ones are baselined.
5. Hard-gate steps (`deps`, `arch`, `validate manifest`, `docs precommit`) block merge on failure.
6. Advisory steps (`cppcheck`, `clones`) run in a separate job with `continue-on-error: true` and report without blocking merge.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Fix `dia check deps` exit code | Returns 0 on violations today; must exit 1 when missing/stale count > 0 |
| 2 | Wire `dia check arch` Click command | `arch_checker.py` exists but is never registered in `cli/check.py` |
| 3 | Add `dia check clones` command | New `commands/check/clone_checker.py` + Click entry; wraps PMD CPD |
| 4 | Create `.github/workflows/ci.yml` | Hard-gate job + advisory job |

---

## Tool Behaviour Spec

### `dia check deps` (fix)
- After printing the summary line, call `ctx.exit(1)` when `len(missing) + len(stale) > 0`.
- `--fix` still auto-patches the module.md files; exit code reflects state _before_ fix is applied (non-zero if violations existed).

### `dia check arch` (new command)
- `@cli.command("arch")` in `cli/check.py`
- Options: `--module <id>` (filter to one module subtree), `--summary` (totals only, no per-file lines)
- Calls `run_arch_check(repo_root, module_filter, summary_only)` from `commands/check/arch_checker.py`
- Prints formatted output via `format_violations()`
- Exits via `ctx.exit(exit_code)` with the integer returned by `run_arch_check`

### `dia check clones` (new command)
- Requires Java + PMD CPD on PATH (`shutil.which("cpd")` or `shutil.which("pmd")`); exits 1 with install hint if absent
- Scans `Dia/` and `Cluiche/` for `*.cpp` and `*.h`; excludes `External/`, `.venv/`, `bin/`, `out/`
- Default `--minimum-tokens 100`; exposed as `--tokens N` option
- `--baseline` flag: writes current findings to `out/check/clones-baseline.xml` and exits 0
- Normal run: exits 1 only if new duplicates appear beyond the baseline (baseline-absent = no filter)
- Writes `out/check/clones.sarif` on every run; prints human summary (file pairs + token count)
- Mirrors the `_run_cppcheck` / baseline pattern from `check.py`

### GitHub Actions workflow
Two jobs in `.github/workflows/ci.yml`:

**`architecture-gates`** (hard gate, blocks merge):
```
dia check deps
dia check arch
dia validate manifest
dia docs precommit
```

**`advisory`** (warn only, `continue-on-error: true`):
```
dia check cppcheck  (skip if not on runner)
dia check clones    (skip if PMD not on runner)
```

Runner: `windows-latest` (matches MSBuild / vcxproj project files).
Python: 3.11, installed via `actions/setup-python`.
DiaCLI installed with `pip install -e Dia/DiaCLI`.
Java: `actions/setup-java` with Temurin 21 for CPD.

---

## Binding Decisions

No platform or application decisions constrain this feature — it is pure tooling.

---

## Open Design Questions

1. **Baseline committed or generated?** The `out/check/clones-baseline.xml` approach means committing generated output to track "allowed violations." Alternative: store baseline count in a small committed JSON and fail only when count increases. The file approach is more precise (exact duplicates, not just count) but adds churn to the baseline file over time.

2. **MSBuild compile step in CI?** A `dia run googletest` step on `windows-latest` requires VS Build Tools on the runner. The GitHub-hosted `windows-latest` runner includes MSVC, so it's technically feasible — but the build takes 5–10 minutes and is expensive on free-tier minutes. Deferred until the fast gate steps are proven; added as a blocked item below.

---

## Acceptance Criteria

- Pushing a branch with an undeclared `#include` causes `architecture-gates` to fail on `dia check deps`.
- Pushing a branch where a `foundation/core` module includes a `domain/` header causes `architecture-gates` to fail on `dia check arch`.
- `dia check clones` detects a 20+ line duplicated block and exits 1 (after baseline cleared).
- The workflow YAML is present at `.github/workflows/ci.yml` and appears in the GitHub Actions tab.
- Advisory job runs to completion without blocking the PR even when cppcheck or clones finds issues.

---

## Blocked / Deferred

| Item | Blocked by | Notes |
|------|-----------|-------|
| MSBuild compile + test in CI | Runner time cost | Add `dia run googletest` step after fast gates proven |
| TSan in CI | Linux/WSL2 runner | Consistent with existing backlog note |
