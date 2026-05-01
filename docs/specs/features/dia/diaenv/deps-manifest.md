# Feature Spec: deps-manifest

## Parent System
@docs/specs/systems/dia/diaenv.md

## Status
`Done`

## Summary

Define and implement `deps.json` — a machine-readable, version-pinned, SHA-256-verified manifest of every binary SDK that lives in `External/` and is gitignored. Implement `deps_restore.py`, a new DiaCLI module that reads the manifest and downloads, verifies, and unpacks each entry to its correct `External/` location. Vendor URLs are used as primary sources; local file paths (`file:///C:/...`) are supported as fallback mirrors for SDKs where vendor URLs are unavailable or slow. Source-only dependencies (googletest, pybind11, websocketpp, asio, jsoncpp-master) are explicitly excluded — those belong in the `submodule-migration` feature.

## Problem

There is currently no machine-readable record of which binary SDKs belong in `External/`, what versions are in use, or where to download them. A fresh `git clone` produces a source tree that cannot build because `External/` is gitignored and the setup ritual is undocumented. This feature is also a hard dependency of `docker-build-env` — the container's `deps` stage cannot function without `deps.json`.

## Goals

- Define `deps.json` schema at repo root covering all binary SDK entries in `External/`
- SHA-256 verification for every downloaded dep before unpacking
- Primary URL + Synology NAS mirror fallback per entry
- Support `zip` and `exe` install types (Awesomium requires silent EXE install)
- `dia env deps` CLI command: restore all deps, or a single named dep
- Sentinel file per dep to skip already-restored entries (fast on re-run)
- `dia env verify` integration: report which deps are present, missing, or hash-mismatched

## Non-Goals

- Source-only deps (googletest, pybind11, websocketpp, asio, jsoncpp-master) — handled by `submodule-migration`
- Toolchain packages (VS 2022, Python, Git) — handled by `winget-manifest`
- Synology NAS as a hosted mirror store — out of scope; local `file://` mirrors are sufficient for now
- Uploading or publishing deps to any remote store (DiaEnv only downloads)
- Automatic version bump / dep update workflow
- `dia env docker deps` container variant — this feature provides the underlying logic; `docker-build-env` calls it

## Binary SDK Manifest

### Deps in scope for `deps.json`

| ID | Location | Install type | Primary source | Mirror |
|----|----------|-------------|----------------|--------|
| `sfml` | `External/SFML/Current-x64` | zip | SFML official GitHub releases | `file://` local path |
| `ultralight` | `External/Ultralight` | zip | Ultralight vendor CDN | `file://` local path |
| `cef` | `External/CEF` | zip | CEF official builds (spotify/cef) | `file://` local path |
| `python311` | `External/Python311` | zip | python.org embeddable package | `file://` local path |
| `visjs` | `External/VisJS` | zip | GitHub releases / CDN | `file://` local path |
| `webix-5.2.1` | `External/Webix/5.2.1` | zip | Webix CDN | `file://` local path |
| `webix-2.4.7` | `External/Webix/2.4.7` | zip | Webix CDN | `file://` local path |
| `protobuf` | `External/protobuf/protobuf-25.6` | zip | GitHub releases | `file://` local path |

**Excluded:** `External/Python` (unreferenced by any `.vcxproj`), `External/Webix/3.1.2` (unreferenced), `External/Cluiche/Tests` (test data, not a dep), `External/External/googletest` (duplicate — covered by submodule-migration).

### `deps.json` Schema

```json
{
  "schema": "diaenv.deps.v1",
  "deps": [
    {
      "id": "sfml",
      "version": "2.6.1",
      "url": "https://github.com/SFML/SFML/releases/download/2.6.1/SFML-2.6.1-windows-vc17-64-bit.zip",
      "mirrors": [
        "\\\\synology\\cluiche-deps\\sfml-2.6.1.zip"
      ],
      "sha256": "<hash>",
      "install_type": "zip",
      "unzip_to": "External/SFML",
      "strip_root": true
    },
    {
      "id": "webix-5.2.1",
      "version": "5.2.1",
      "url": "https://cdn.webix.com/packages/webix-gpl-5.2.1.zip",
      "mirrors": [
        "file:///C:/cluiche-deps/webix-5.2.1.zip"
      ],
      "sha256": "<hash>",
      "install_type": "zip",
      "unzip_to": "External/Webix/5.2.1",
      "strip_root": true
    }
  ]
}
```

**Field reference:**

| Field | Required | Description |
|-------|----------|-------------|
| `id` | Yes | Unique identifier; used as sentinel file name and CLI argument |
| `version` | Yes | Pinned version string |
| `url` | No | Primary download URL; `null` for dead CDNs |
| `mirrors` | No | Ordered fallback list; tried in sequence after `url` fails |
| `sha256` | Yes | SHA-256 hex digest of the downloaded archive/installer before unpacking |
| `install_type` | Yes | `zip` or `exe` |
| `unzip_to` | zip only | Destination path relative to repo root |
| `strip_root` | zip only | Strip single top-level directory from archive before extracting |
| `exe_args` | exe only | Silent-install arguments passed to the installer EXE |
| `install_to` | exe only | Expected output directory (used for sentinel + verify) |

## CLI Interface

```bash
# Restore all deps listed in deps.json
dia env deps

# Restore a single named dep
dia env deps --dep sfml

# Force re-download even if sentinel present
dia env deps --force
dia env deps --dep sfml --force

# Verify dep presence and hashes (read-only, no download)
dia env verify --deps
```

## Implementation

### New module: `deps_restore.py`

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── env/
    │       └── deps_cmd.py          # Click command: dia env deps
    ├── commands/
    │   └── env/
    │       └── deps_restore_cmd.py  # Orchestration logic
    └── utils/
        └── deps_restore.py          # Core: reads deps.json, download, verify, unpack
```

`deps_restore.py` responsibilities:
- Load and validate `deps.json` against the schema
- For each entry: check sentinel → skip if present (unless `--force`)
- Download from `url`, fall back to `mirrors` in order on 404 or connection error; mirrors support `file://` protocol (uses `shutil.copy`) and `http(s)://` (uses `requests`)
- SHA-256 verify downloaded file; abort with exit code 1 on mismatch
- Dispatch to `_install_zip()` or `_install_exe()` based on `install_type`
- Write sentinel file to `.diaenv/deps/<id>.restored` on success
- Emit DiaCLI events: `OnDepDownloadStarted`, `OnDepDownloadCompleted`, `OnDepDownloadFailed`

`software_installer.py` remains as a low-level HTTP streaming utility; `deps_restore.py` calls it internally for the download step.

### Sentinel files

```
.diaenv/
└── deps/
    ├── sfml.restored
    ├── ultralight.restored
    └── ...
```

`.diaenv/` is gitignored. Sentinel file content: `{"id": "sfml", "version": "2.6.1", "sha256": "<hash>", "restored_at": "2026-04-25T10:00:00"}`.

### `dia env verify` integration

`dia env verify` inspects each dep entry in `deps.json` and reports:
- `PASS` — sentinel present and version matches
- `WARN` — directory exists but no sentinel (manually placed, unverified)
- `FAIL` — sentinel absent and directory missing

## Files Introduced / Modified

| File | Change |
|------|--------|
| `deps.json` (repo root) | New — binary SDK manifest |
| `.gitignore` | Add `.diaenv/` sentinel directory |
| `Dia/DiaCLI/dia_cli/utils/deps_restore.py` | New — core manifest reader + download/verify/unpack |
| `Dia/DiaCLI/dia_cli/commands/env/deps_restore_cmd.py` | New — orchestration |
| `Dia/DiaCLI/dia_cli/cli/env/deps_cmd.py` | New — Click command surface |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `software_installer.py` | Internal | Used for HTTP streaming download |
| `requests` + `requests-toolbelt` | Python package | Already in DiaCLI dependencies |
| `hashlib` (stdlib) | Python stdlib | SHA-256 verification |
| `zipfile` / `shutil` (stdlib) | Python stdlib | Archive extraction |
| Synology NAS (network) | Runtime | Must be reachable for dead-CDN deps (Awesomium); optional for others |

## Acceptance Criteria

1. `deps.json` exists at repo root and validates against the `diaenv.deps.v1` schema for all 9 binary SDK entries
2. `dia env deps` downloads, SHA-256 verifies, and unpacks all deps to their correct `External/` locations; exits 0 on success
3. `dia env deps --dep sfml` restores only the named dep; exits 1 if the ID is not found in `deps.json`
4. Re-running `dia env deps` when all sentinels are present completes immediately (no network I/O) — no-op fast path
5. `dia env deps --force` re-downloads and re-verifies even when sentinel is present
6. SHA-256 mismatch aborts the install for that dep, deletes the partial download, and exits 1 with a clear error naming the dep and expected vs actual hash
7. If `url` returns 404, each mirror is tried in order; if all fail, exit 1 naming all attempted URLs
8. `dia env verify` reports PASS/WARN/FAIL per dep entry, using sentinel files as the source of truth
9. `deps.json` is committed to the repo; `.diaenv/` sentinel directory is gitignored
10. A `file://` mirror path is tried via `shutil.copy` when the primary URL fails; `http(s)://` mirrors use `requests`
11. A transient network error retries up to 3 times with exponential backoff before attempting the next mirror
12. A partial download (`.tmp` file) left by an interrupted run is detected and cleaned up before re-downloading

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
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — development tooling; no application runtime components |
| PD-003 | Platform | Component-based entities | Compliant — development tooling; no entity system used |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only; no C++ public API surface |
| PD-005 | Platform | x64 Windows only | Compliant — all SDK downloads target Windows x64; no cross-platform paths |
| PD-006 | Platform | VS project files are source of truth | Compliant — `deps.json` feeds `External/`; does not modify any `.vcxproj` or `Directory.Build.props` |
| PD-007 | Platform | C++20 required | Compliant — tooling feature; no compiler configuration touched |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — deps are unpacked to `External/`; no build output paths modified |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling; no new C++ module introduced |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — `deps_cmd.py` follows the same two-file plugin pattern used by all DiaCLI commands |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — all implementation is Python |
| SD-CLI-003 | DiaCLI | Separate from C++ DiaAPI | Compliant — no C++ DiaAPI commands introduced |
| SD-CLI-006 | DiaCLI | Click framework for argument parsing | Compliant — `deps_cmd.py` uses Click decorators |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exits 0 on success, 1 on hard failure, 3 on manifest not found |
| SD-ENV-001 | DiaEnv | `deps.json` is single source of truth for binary SDKs | Compliant — this feature defines and owns `deps.json`; no parallel dep lists |
| SD-ENV-002 | DiaEnv | SHA-256 verification required for all downloaded deps | Compliant — `deps_restore.py` verifies SHA-256 before unpacking every entry; mismatch aborts |
| SD-ENV-003 | DiaEnv | `dia env verify` is read-only and CI-safe | Compliant — verify integration reads sentinels and filesystem only; no downloads triggered |
| SD-ENV-007 | DiaEnv | Source-only deps use git submodules; binary SDKs use deps.json | Compliant — source deps (googletest, pybind11, etc.) explicitly excluded from this feature |
| SD-ENV-010 | DiaEnv | Python 3.11 is the single Python version for all contexts | Compliant — `python311` entry in `deps.json` provides the embedded Python 3.11 runtime; host Python 3.11 is owned by `winget-manifest` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | deps.json | Should SHA-256 hashes be pre-populated in the committed `deps.json`, or computed on first run and written back? | Pre-populated — hashes must be manually computed and committed. Computed-on-first-run would defeat the integrity guarantee (TOFU attack vector). |
| 2 | mirrors | How are local file mirrors specified? | `file://` URI (`file:///C:/cluiche-deps/sfml.zip`). `deps_restore.py` detects the protocol and dispatches to `shutil.copy` for `file://` and `requests` for `http(s)://`. |
| 3 | mirrors | Is Synology NAS mirror support in scope? | No — out of scope for this feature. Local `file://` mirrors are sufficient. NAS support deferred to a future feature. |
| 4 | exe install type | Should `exe` installs be verified post-install in addition to pre-install SHA-256? | Yes — optional `verify_file` field in the schema: a relative path inside `install_to` that must exist after install. Sentinel only written if check passes. |
| 5 | strip_root | Is `strip_root: true` needed for all zip deps, or only some? | Per-dep — must be verified against each SDK's archive structure when populating `deps.json`. Default `false`; set `true` only where archive has a single root directory wrapper. |
| 6 | Python entries | Are both `External/Python` and `External/Python311` needed? | Resolved — only `python311` is referenced in `.vcxproj` files. `External/Python` is excluded from `deps.json`. |
| 7 | Webix versions | Which Webix versions are active? | Resolved — `5.2.1` and `2.4.7` both referenced in HTML files; `3.1.2` unreferenced and excluded. Two separate deps.json entries. |
| 8 | Network errors | Should transient errors retry before falling back to mirrors? | Yes — retry up to 3 times with exponential backoff before trying the next mirror. Implemented in `deps_restore.py`. |
| 9 | Partial downloads | Is a partial download cleaned up if the process is killed mid-run? | Yes — write to `.tmp` file, rename atomically on completion. `.tmp` present at startup = interrupted run; delete and re-download. |
