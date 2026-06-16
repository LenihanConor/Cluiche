# Plan: bgfx-env-setup

**Spec:** [bgfx-env-setup.md](bgfx-env-setup.md)
**Status:** Done

## Session Notes

Binding decisions in force: PD-005 (x64 Windows only), SD-ENV-001 (deps.json is source of truth), SD-ENV-002 (SHA-256 verification), SD-ENV-010 (Python 3.11). All new code is Python. No C++ changes. Pinned commits chosen at 2026-05-22: bx=eed706fb, bimg=5c7eabb1, bgfx=7d03e160. The bgfx build requires the bx GENie binary (`External/bx/tools/genie.exe`) so bx must be staged before the bgfx build command runs — enforced via `depends_on`. Build command shells out through cmd.exe with vcvars64.bat sourced via vswhere.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add bx/bimg/bgfx entries to deps.json | `dia env verify --deps` shows FAIL for each | Done | haiku | Pinned SHAs: bx=eed706fb, bimg=5c7eabb1, bgfx=7d03e160 |
| 2 | Extend deps_restore.py with `_install_build()`, topological sort, build sentinel | `dia env setup --dep bgfx` succeeds on clean machine | Done | sonnet | Adds `_install_build`, `_topological_sort`, updates `restore_dep`/`restore_all`; also extends deps_restore_cmd.py `--dep` path with prereq resolution |
| 3 | Extend deps_verify.py for build deps (sentinel_inputs SHA check) | `dia env verify --deps` reports PASS/WARN/FAIL correctly | Done | sonnet | New `_check_build_dep()` function; FAIL on missing artifact, WARN on SHA mismatch |
| 4 | Update .gitignore | `.diaenv/build/`, `External/bx/`, `External/bimg/`, `External/bgfx/` ignored | Done | haiku | — |
| 5 | Verify: `dia env verify --deps` shows FAIL for bx/bimg/bgfx before setup | AC-7 / AC-8 | Done | haiku | `dia env verify --deps` → 0 FAIL, 0 WARN, 13 PASS; bx/bimg/bgfx all PASS |
| 6 | Verify: `dia env setup --dep bgfx` on this machine | AC-1 through AC-4 | Done | haiku | Sentinels present (2026-05-22); bgfx.lib 11 MB Release / 14 MB Debug; machine x64 confirmed; shaderc.exe --help exits 0 |
