# Feature Spec: submodule-migration

## Parent System
@docs/specs/systems/dia/diaenv.md

## Status
`Done`

## Summary

Resolve the partially-complete and inconsistent submodule state for the five source-only dependencies in `External/`. Three deps (`pybind11`, `asio`, `websocketpp`) are already tracked as git submodule commits but have incomplete or missing `.gitmodules` entries. One dep (`googletest`) has a `.git` directory but is not registered as a submodule. One dep (`jsoncpp-master`) is committed as a plain file tree with no upstream remote and must be converted to a proper submodule. After migration, `git clone --recurse-submodules` and `dia env verify` will correctly initialise all five.

## Problem

The source-only deps in `External/` are in an inconsistent git state: some are partially registered submodules, one is unregistered, and one is a vendored file blob. This means `git clone --recurse-submodules` does not restore the full source tree, `git submodule status` errors out, and `dia env verify` cannot reliably check submodule health.

## Current State vs Target State

| Dep | Current state | Target state |
|-----|--------------|--------------|
| `pybind11` | Submodule commit tracked (160000) + `.gitmodules` entry ✓ | Verify URL and pinned commit; no change needed if correct |
| `asio` | Submodule commit tracked (160000) — **missing `.gitmodules` entry** | Add `.gitmodules` entry with URL + pinned commit |
| `websocketpp` | Submodule commit tracked (160000) — **missing `.gitmodules` entry** | Add `.gitmodules` entry with URL + pinned commit |
| `googletest` | Has `.git` dir, commit tracked (160000) — **not registered in `.gitmodules`** | Register as submodule in `.gitmodules` |
| `jsoncpp-master` | Committed as plain file tree; `.git` remote points at Cluiche repo itself | Remove vendored files, add as submodule pointing at `open-source-parsers/jsoncpp` |

## Known Pinned Commits

| Dep | Remote URL | Pinned commit |
|-----|-----------|--------------|
| `pybind11` | `https://github.com/pybind/pybind11.git` | `288913638bb2da563f1c39e7d07071c2f21bfb25` |
| `asio` | `https://github.com/chriskohlhoff/asio.git` | `a4d820dd69b37fb8daee275d20eb162054453414` |
| `websocketpp` | `https://github.com/zaphoyd/websocketpp.git` | `4dfe1be74e684acca19ac1cf96cce0df9eac2a2d` |
| `googletest` | `https://github.com/google/googletest.git` | `d72f9c8aea6817cdf1ca0ac10887f328de7f3da2` |
| `jsoncpp-master` | `https://github.com/open-source-parsers/jsoncpp.git` | TBD — vendored version is `1.6.5`; pin to tag `1.6.5` commit at implementation time |

## Goals

- `.gitmodules` has correct entries for all five deps
- `git submodule status` exits 0 with no errors
- `git clone --recurse-submodules` restores all five source deps correctly
- `dia env verify` reports submodule health per dep (PASS / WARN / FAIL)
- `jsoncpp-master` vendored files removed from git history and replaced with a submodule

## Non-Goals

- Binary SDK deps (`SFML`, `CEF`, `Ultralight`, etc.) — covered by `deps-manifest`
- Updating submodule versions — this feature only fixes the registration state; version bumps are a developer responsibility
- Automating submodule updates on `git pull` — `git submodule update --init --recursive` is the developer's responsibility; `dia env verify` will remind them

## CLI Interface

```bash
# Verify submodule health (read-only)
dia env verify
dia env verify --submodules   # submodule checks only
```

No new `dia env` command is introduced — the migration itself is a one-time git operation performed manually (or scripted as a migration script). `dia env verify` gains submodule health reporting.

## Migration Steps (implementation tasks)

### Task 1 — Fix `.gitmodules` for asio and websocketpp
Add missing entries to `.gitmodules`:
```ini
[submodule "External/asio"]
    path = External/asio
    url = https://github.com/chriskohlhoff/asio.git

[submodule "External/websocketpp"]
    path = External/websocketpp
    url = https://github.com/zaphoyd/websocketpp.git
```

### Task 2 — Register googletest in `.gitmodules`
```ini
[submodule "External/googletest"]
    path = External/googletest
    url = https://github.com/google/googletest.git
```

### Task 3 — Convert jsoncpp-master to submodule
1. Identify the jsoncpp commit that matches the vendored source (compare tag or file hashes against `open-source-parsers/jsoncpp` releases)
2. `git rm -r External/jsoncpp-master` — remove vendored files from index
3. `git submodule add https://github.com/open-source-parsers/jsoncpp.git External/jsoncpp-master`
4. `git submodule update --init External/jsoncpp-master`
5. Checkout the pinned commit matching the vendored version

### Task 4 — Verify pybind11
Confirm `.gitmodules` entry URL matches `https://github.com/pybind/pybind11.git` and pinned commit is correct. No change expected.

### Task 5 — `dia env verify` submodule integration
Extend `toolchain_verify.py` (or a new `submodule_verify.py`) to:
- Read `.gitmodules` and enumerate registered submodules
- For each: check if path exists and is initialised (`git submodule status`)
- Report PASS (initialised + correct commit) / WARN (initialised but detached/wrong commit) / FAIL (uninitialised or missing)
- Print actionable fix: `git submodule update --init --recursive`

## Files Modified

| File | Change |
|------|--------|
| `.gitmodules` | Add entries for asio, websocketpp, googletest; verify pybind11 |
| `External/jsoncpp-master/` | Remove vendored files; replace with submodule |
| `Dia/DiaCLI/dia_cli/utils/submodule_verify.py` | New — submodule health checks for `dia env verify` |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `env-verify-cmd` feature | Integration | `submodule_verify.py` consumed by `dia env verify` |
| Git (host) | Runtime | `git submodule` commands; no minimum version constraint found |

## Acceptance Criteria

1. `git submodule status` exits 0 with no errors after migration
2. `.gitmodules` contains correct entries for all five deps with matching remote URLs
3. `git clone --recurse-submodules <repo>` on a fresh machine initialises all five source deps without manual intervention
4. `dia env verify` reports PASS for all five submodules when correctly initialised
5. `dia env verify` reports FAIL with actionable fix message for any uninitialised submodule
6. `External/jsoncpp-master` is no longer a plain file tree in git history — it is a submodule commit pointer
7. All `.vcxproj` files that reference `External/jsoncpp-master/` continue to build after conversion (path unchanged)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEnv | @docs/specs/systems/dia/diaenv.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only; no C++ identifiers introduced |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — development tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — development tooling; no entity system used |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only; no C++ public API surface |
| PD-005 | Platform | x64 Windows only | Compliant — git submodules are platform-agnostic; source deps have no platform constraint |
| PD-006 | Platform | VS project files are source of truth | Compliant — all `External/jsoncpp-master/` include paths in `.vcxproj` files are preserved unchanged |
| PD-007 | Platform | C++20 required | Compliant — tooling feature; no compiler configuration touched |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — no build output paths modified |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling; no new C++ module introduced |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — `submodule_verify.py` is a utility consumed by the existing verify command |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — all new code is Python |
| SD-CLI-006 | DiaCLI | Click framework for argument parsing | Compliant — no new CLI commands; extends existing verify command |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — verify exits 0 on all-pass, non-zero on failure |
| SD-ENV-003 | DiaEnv | `dia env verify` is read-only and CI-safe | Compliant — submodule verify reads `.gitmodules` and `git submodule status` only; no writes |
| SD-ENV-007 | DiaEnv | Source-only deps use git submodules; binary SDKs use deps.json | Compliant — this feature implements the source-dep side of SD-ENV-007 |
| SD-ENV-010 | DiaEnv | Python 3.11 is the single Python version | Compliant — tooling feature; no Python version constraint introduced |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | jsoncpp-master | Which jsoncpp release/commit matches the currently vendored source? | Resolved — vendored version is `1.6.5` (confirmed via `External/jsoncpp-master/version`). Pin to tag `1.6.5` on `open-source-parsers/jsoncpp`; resolve exact commit hash at implementation time via `git rev-parse 1.6.5`. |
| 2 | jsoncpp-master | Does removing the vendored `jsoncpp-master` tree from git history require a rebase/filter-branch, or is a simple `git rm` + commit sufficient? | Simple `git rm` + commit is sufficient for forward cleanliness. History rewrite (filter-branch) would be needed to reduce repo size, but is disruptive on a shared branch. Recommend `git rm` + commit only. |
| 3 | googletest | googletest has a `.git` dir but is tracked as a 160000 commit — is it safe to just add the `.gitmodules` entry without re-running `git submodule add`? | Yes — the commit object is already in git's index at mode 160000. Adding the `.gitmodules` entry and running `git submodule sync` is sufficient to make it a properly registered submodule. |
| 4 | verify integration | Should `dia env verify --submodules` also run `git submodule update --init --recursive` automatically, or remain read-only? | Read-only per SD-ENV-003. Print the fix command but do not run it. |
| 5 | CI safety | `git submodule status` errors on the current repo due to missing `.gitmodules` entries — should this migration be done in a single commit or multiple? | Single commit for `.gitmodules` fixes (Tasks 1–4); separate commit for jsoncpp conversion (Task 3) to keep the history readable. Both must land before `dia env verify` submodule checks are enabled. |
