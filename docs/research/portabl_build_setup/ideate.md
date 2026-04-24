# Research: Ideate — Portable Build Setup & Development Environment

**Input:** docs/research/portabl_build_setup/explore.md

## Candidates

### Candidate 1: `dia env setup` — Full Environment Bootstrap Command
**Home module/system:** DiaCLI (`Dia/DiaCLI/`)
**Size:** M (2–3 weeks)
**Description:** A single DiaCLI command — `dia env setup` — that provisions a fresh machine end-to-end. It reads a `deps.json` manifest to download and unzip binary SDKs into `External/`, invokes `winget import` from a committed `winget.json` to install VS 2022 + Python 3.11 + Git, checks PATH and environment variables, and prints a final health summary. Sub-commands (`dia env setup --deps-only`, `--toolchain-only`) allow partial runs. The manifest lives in the repo root, is version-pinned with SHA-256 hashes, and is the single source of truth for what a working environment looks like.
**Primary value:** A developer (or the same developer on a new machine) can go from `git clone` to successful build with one command and no documentation lookup.

---

### Candidate 2: `dia env verify` — Environment Health Checker
**Home module/system:** DiaCLI (`Dia/DiaCLI/`)
**Size:** S (3–5 days)
**Description:** A read-only `dia env verify` command that inspects the current machine and reports what is missing or mismatched: VS 2022 installed? Correct toolset version? Python 3.11 on PATH? MSBuild locatable? Each entry in `External/` that should exist per the manifest — does it? Outputs a colour-coded checklist (pass / warn / fail) and exits non-zero on any hard failure. Does not install anything. Useful as a CI pre-flight and as a day-to-day sanity check after pulling changes that add a new dependency.
**Primary value:** Turns "why won't this build on my machine" from a debugging session into a 5-second diagnosis.

---

### Candidate 3: SDK Dependency Manifest (`deps.json`)
**Home module/system:** Repo root config file + DiaCLI utilities
**Size:** S (2–4 days)
**Description:** A `deps.json` (or `dia_deps.json`) file at the repo root that declaratively lists every binary SDK currently gitignored from `External/`: Ultralight, Python311, pre-built Protobuf, CEF. Each entry has a name, target path (relative to repo root), download URL, version string, and SHA-256 hash for integrity verification. No new commands needed at first — DiaCLI's existing `software_installer` can consume it with minimal extension. This is the foundational data layer that Candidates 1, 2, and 4 all depend on.
**Primary value:** Replaces tribal knowledge ("go download Ultralight 1.3 from their site") with a versioned, machine-readable contract that anyone can follow.

---

### Candidate 4: winget Toolchain Manifest (`winget.json`)
**Home module/system:** Repo root config file + DiaCLI `setup` command
**Size:** S (1–2 days)
**Description:** A `winget.json` (generated via `winget export`) committed to the repo root that captures the exact versions of VS 2022 (with C++ Desktop + Windows 11 SDK workloads), Python 3.11, and Git. Paired with a one-liner bootstrap: `winget import winget.json --accept-package-agreements`. DiaCLI's `dia env setup --toolchain` sub-command wraps this call, checks for elevation, and handles the known quirk that VS installs require the installer to be closed first. The manifest is re-exported and committed whenever a toolchain version changes.
**Primary value:** Toolchain setup becomes a single command rather than a 20-step VS installer walkthrough.

---

### Candidate 5: `dia env export` — Environment Snapshot for Transfer
**Home module/system:** DiaCLI (`Dia/DiaCLI/`)
**Size:** S (3–5 days)
**Description:** A `dia env export` command that produces a portable zip containing: (1) the full `External/` tree (or a manifest pointing to cached copies), (2) the `.claude/` directory minus `settings.local.json`, (3) the `winget.json`, and (4) a `dia env import` script. Intended for the "same developer, new machine" case where internet access to re-download SDKs may be slow or unavailable. The zip is not committed to git — it's a point-in-time transfer artefact. `dia env import <zip>` unpacks it and runs verify to confirm the machine is ready.
**Primary value:** Enables offline or low-bandwidth machine transfers without re-downloading large SDKs from scratch.

---

### Candidate 6: `External/` Partial Git Submodule Migration
**Home module/system:** Repo root (`.gitmodules`) + DiaCLI
**Size:** S (2–3 days)
**Description:** Convert the source-only, header-only deps already in `External/` (googletest, pybind11, websocketpp, asio) to proper git submodules pinned at exact commits. They are already gitignored inconsistently — making them submodules means `git clone --recurse-submodules` populates them automatically with no script needed. Binary SDKs (Ultralight, Python311, Protobuf) remain as manifest-downloaded deps (Candidate 3). This splits `External/` into two clean categories: submodules (source deps, auto-populated) and downloaded (binary SDKs, populated by `dia env setup`).
**Primary value:** Eliminates one whole class of "missing External" failures for source-only deps with zero ongoing maintenance — git handles it.

---

### Candidate 7: `.claude/` AI Context Portability Hardening
**Home module/system:** `.claude/` directory structure + DiaCLI
**Size:** S (2–3 days)
**Description:** The `.claude/` tree already travels with the repo for most content. The gaps are: (1) `settings.local.json` is gitignored and machine-specific, (2) the memory directory lives at a user-profile path (`C:\Users\<user>\.claude\projects\...`) that is not in the repo. This candidate adds a `dia env claude-setup` command that: generates a correct `settings.local.json` from a committed template (`settings.local.template.json`), and symlinks or copies the in-repo `memory/` directory to the expected user-profile path. Result: a new machine immediately has Claude Code configured with full project memory and skills — the AI engagement continues without a cold start.
**Primary value:** The AI assistant "knows who you are" from the first session on a new machine.

---

### Candidate 8: `dia env doctor` — Continuous Environment Monitoring
**Home module/system:** DiaCLI (`Dia/DiaCLI/`)
**Size:** M (1–2 weeks)
**Description:** Extends `dia env verify` (Candidate 2) with a watch mode and automatic remediation suggestions. Runs as a background task or as a pre-build hook (MSBuild `BeforeBuild` target) that checks environment freshness: are all manifest entries still at the pinned versions? Has a new dep been added to `deps.json` that hasn't been downloaded yet? Outputs actionable `dia env setup --<component>` commands for anything out of sync. Can be wired into DiaCLI's event system to fire on `git pull` via a git hook.
**Primary value:** Prevents "it built yesterday, why not today" after a dep manifest update — environment drift is caught immediately, not at compile time.

---

### Candidate 9: MSBuild `RestoreExternals` Target
**Home module/system:** `Directory.Build.props` / `Directory.Build.targets`
**Size:** S (2–3 days)
**Description:** Add a `RestoreExternals` MSBuild target (similar to NuGet's `Restore`) that reads `deps.json` and downloads any missing `External/` entries before compilation begins. Triggered automatically by MSBuild when building the solution — no separate CLI step required. Implemented as a PowerShell inline task in `Directory.Build.targets`. Works whether the developer builds from VS or from the command line. Does not replace DiaCLI but provides a safety net for developers who forget to run `dia env setup`.
**Primary value:** The build system self-heals missing deps — a developer who forgets the setup step gets a working build anyway, not a cryptic linker error.

---

## Coverage Map

The nine candidates span the full range of design axes from explore.md:

- **Foundational data** (no logic): Candidate 3 (deps.json), Candidate 4 (winget.json)
- **Automation commands — setup**: Candidate 1 (full bootstrap), Candidate 9 (MSBuild auto-restore)
- **Automation commands — verify/diagnose**: Candidate 2 (verify), Candidate 8 (doctor/watch)
- **Transfer / portability**: Candidate 5 (export/import zip)
- **Source control hygiene**: Candidate 6 (submodule migration)
- **AI context continuity**: Candidate 7 (claude-setup)

Size range: 5× Small (days), 3× Medium (weeks), 0× Large/XL — this problem is solvable incrementally without a big-bang rewrite. Candidates 3 and 4 are prerequisites for Candidate 1; Candidate 2 is a natural companion to Candidate 1. Most candidates can be built independently and shipped in sequence.
