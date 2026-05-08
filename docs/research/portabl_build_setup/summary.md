# Research Summary — Portable Build Setup & Development Environment

**Session folder:** docs/research/portabl_build_setup/
**Date:** 2026-04-24

## One-Line Answer

Build a `DiaEnv` system inside DiaCLI that provisions a fresh Windows machine — toolchain, binary SDKs, source deps, and AI context — from a single command after `git clone`.

## Journey

1. **Explored:** Fresh clone of Cluiche is unbuildable without manual SDK hunting, VS workload installation, and Python setup; the `.claude/` directory already travels with the repo but memory and `settings.local.json` have gaps. Docker was considered but ruled out (Windows-only project, SFML needs a display).
2. **Ideated:** 9 candidates generated — ranging from pure data files (deps.json, winget.json) to CLI commands (setup, verify, doctor, export), a build-system integration (MSBuild RestoreExternals), git hygiene (submodule migration), and AI context portability hardening.
3. **Evaluated:** deps.json manifest scored highest (3.75) as standalone, confirming it as the foundation; MSBuild RestoreExternals was highest on game value; submodule migration offers zero-maintenance wins for source-only deps.
4. **Chose:** The full `DiaEnv` system — all candidates sequenced as ordered feature specs — because the user's goal (clone → build works → AI engagement continues) requires the complete stack.

## Chosen Work Item

**Name:** DiaEnv — Portable Development Environment System
**Home module:** DiaCLI (`Dia/DiaCLI/`)
**Suggested spec type:** System (with child feature specs per candidate)
**Estimated size:** M overall (each feature is S; sequenced over several sessions)

## Key Insights from Exploration

- `.claude/` is already in git — AI context portability is ~80% solved for free; only `settings.local.json` (template needed) and the user-profile memory path (symlink needed) require work
- DiaCLI's `software_installer` already has download+unzip primitives — the manifest is the only missing piece before `dia env setup --deps` can work
- `External/` splits cleanly into two categories: source/header-only deps (googletest, pybind11, websocketpp, asio) → git submodules; binary SDKs (Ultralight, Python311, Protobuf, CEF) → manifest download
- `vswhere.exe` (ships with VS) is the reliable way to locate MSBuild and detect VS workloads — use it in `dia env verify`
- The winget toolchain manifest needs elevation and VS must be closed during install — `dia env setup --toolchain` must check for this and guide the user
- SDK URL stability is the main ongoing risk — SHA-256 hashing in deps.json catches corruption but not upstream URL changes; consider a fallback mirror field in the schema

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Docker | PD-005 Windows-only; SFML needs display; Windows containers are 20-30 GB with poor dev ergonomics |
| `dia env doctor` (standalone) | Premature until setup + verify exist; valuable later as the project grows contributors |
| `dia env export` (standalone) | Third-tier convenience; only valuable after setup works reliably |

## References

- docs/research/portabl_build_setup/explore.md
- docs/research/portabl_build_setup/ideate.md
- docs/research/portabl_build_setup/evaluate.md
- docs/research/portabl_build_setup/choose.md
