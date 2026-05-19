# Feature Spec: bgfx-env-setup

## Parent System
@docs/specs/systems/dia/diaenv.md

**Cross-cutting system:** @docs/specs/systems/dia/render-backend.md (the consumer of this work)

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Extend DiaEnv's `deps.json` manifest and `deps_restore.py` to support **source-built** binary dependencies, then add **bgfx** (with **bx** and **bimg**) as the first such dep. Unlike the existing zip/exe deps, bgfx ships as source plus a GENie/CMake build script — its prebuilt libs are not redistributed by the vendor, so DiaEnv must clone the source at a pinned commit, run the upstream build, and place the resulting libs and headers under `External/bgfx/` in a layout consumable by `DiaBgfx.vcxproj`. `dia env setup` must orchestrate this build during environment provisioning, and `dia env verify` must confirm the built artifacts are present and ABI-compatible.

This feature is the gating prerequisite for every other Phase 1 feature in the [render-backend system](../../../systems/dia/render-backend.md): no engineer can build `DiaBgfx.vcxproj` until `External/bgfx/` exists with the expected library and header layout.

## Problem

bgfx (BSD 2-clause), bx, and bimg are distributed as source-only repositories with a custom GENie-based build that targets MSVC via `make vs2022` plus `msbuild`. There is no vendor-published prebuilt zip we can pin, hash-verify, and unpack the way `deps-manifest` handles SFML or CEF. Three bad alternatives if we don't extend DiaEnv:

1. **Manual instructions in a README** — every developer (and CI) hand-runs `git clone` + GENie + msbuild before any `dia run` works. Recreates the exact "undocumented setup ritual" problem DiaEnv exists to eliminate.
2. **Vendor a prebuilt copy in `External/`** — non-binary repo (or LFS) bloat; no version pinning hygiene; rebuilds for new MSVC versions become a manual ritual.
3. **Build bgfx inside `DiaBgfx.vcxproj`** — violates PD-006 (each `.vcxproj` owns one project; cross-CMake build pre-steps are out of scope) and bloats the per-developer build.

The right answer is to extend DiaEnv: `deps.json` gains a `build` install type that clones at a pinned commit, runs the upstream build script, and stages the outputs into `External/bgfx/`. Sentinel + SHA verification still apply (against the pinned commit and the staged output checksum).

## Goals

- Add `install_type: "build"` to `deps.json` schema — source-build deps with a clone step, a build step, and a stage step
- Add three new entries to `deps.json`: `bgfx`, `bx`, `bimg` (source-built; pinned to specific commits)
- Implement `_install_build()` in `deps_restore.py`: clone → checkout pinned commit → invoke upstream build → stage artifacts to `External/<id>/`
- `External/bgfx/` post-restore layout matches what `DiaBgfx.vcxproj` will consume (headers under `include/`, libs under `lib/<config>/`, tools under `tools/`)
- Build the bgfx `shaderc` tool as part of the build step; stage to `External/bgfx/tools/shaderc.exe`
- `dia env setup` runs the bgfx build as part of the `--deps` step automatically
- `dia env verify` reports `PASS` / `WARN` / `FAIL` for each source-built dep with the same semantics as zip deps
- Sentinel records the *built artifact* SHA-256, not the source repo SHA — detects partial/corrupted builds
- `dia env setup --dep bgfx --force` rebuilds from a clean clone
- The build runs **once** per developer machine (idempotent via sentinel) and is fast on re-run (sentinel skip in <1 s)

## Non-Goals

- **Multi-backend selection at build time** — bgfx is built with all Windows backends enabled (D3D11, D3D12, Vulkan, GL); runtime backend selection is the job of `DiaBgfx::Canvas` and is out of scope for env setup
- **CI cache of prebuilt bgfx libs** — caching the build output across machines (S3/Synology cache) is a future optimisation; for now every machine builds locally on first setup
- **Cross-platform builds** — Windows x64 only (PD-005); no Linux/macOS support
- **Building shaders themselves** — `shaderc.exe` is staged so DiaPipeline can use it (`diapipeline-shaderc-cook` feature owns shader cooking); env setup does not cook shaders
- **Incremental rebuild on bgfx commit bump** — bumping the pinned commit is rare; force-rebuild via `--force` is acceptable
- **Pre-built bgfx binaries from a third party** — no `bgfx-prebuilt-vendor.com`-style mirror; the upstream build is the source of truth
- **Replacing `dia env deps` with a CMake superbuild** — DiaEnv stays Python-driven; we shell out to the upstream build, we don't reimplement it

## `deps.json` schema extension

Add a new install type `build` alongside the existing `zip` and `exe`. Builds have their own field set:

```json
{
  "id": "bgfx",
  "version": "v1.127.8710-491",
  "install_type": "build",
  "source": {
    "git": "https://github.com/bkaradzic/bgfx.git",
    "commit": "<full-sha>"
  },
  "build": {
    "command": "..\\bx\\tools\\bin\\windows\\genie.exe --with-tools vs2022 && msbuild .build/projects/vs2022/bgfx.sln /p:Configuration=Release /p:Platform=x64 /m && msbuild .build/projects/vs2022/bgfx.sln /p:Configuration=Debug /p:Platform=x64 /m",
    "working_dir": "<clone_root>"
  },
  "stage": [
    { "from": "include",                                 "to": "External/bgfx/include" },
    { "from": ".build/win64_vs2022/bin/bgfxRelease.lib", "to": "External/bgfx/lib/Release/bgfx.lib" },
    { "from": ".build/win64_vs2022/bin/bgfxDebug.lib",   "to": "External/bgfx/lib/Debug/bgfx.lib" },
    { "from": ".build/win64_vs2022/bin/shadercRelease.exe", "to": "External/bgfx/tools/shaderc.exe" }
  ],
  "depends_on": ["bx", "bimg"],
  "sentinel_inputs": [
    "External/bgfx/lib/Release/bgfx.lib",
    "External/bgfx/lib/Debug/bgfx.lib",
    "External/bgfx/tools/shaderc.exe"
  ]
}
```

**New fields (build install type):**

| Field | Required | Description |
|-------|----------|-------------|
| `source.git` | Yes | Upstream Git repo URL |
| `source.commit` | Yes | Pinned commit SHA (full 40-char) |
| `build.command` | Yes | Shell command to invoke from clone root; runs in cmd.exe on Windows; `&&`-chained steps allowed |
| `build.working_dir` | Yes | Path the build runs from (placeholder `<clone_root>` resolved to clone path) |
| `stage[].from` | Yes | Path relative to clone root, copied to `stage[].to` |
| `stage[].to` | Yes | Destination relative to repo root |
| `depends_on` | No | Ordered list of dep ids that must be built first |
| `sentinel_inputs` | No | Files SHA-256'd into the sentinel; mismatched checksum on re-run triggers rebuild |

`url`, `mirrors`, `sha256`, `unzip_to`, `strip_root`, `exe_args`, `install_to` (existing fields) are not used by build deps. Schema validator must allow either `{ url + sha256 + install_type:"zip" }` OR `{ url + sha256 + install_type:"exe" }` OR `{ source + build + stage + install_type:"build" }`.

## Three new `deps.json` entries

`bx`, `bimg`, `bgfx` are added in dependency order. bx must build before bimg (which depends on bx), and both before bgfx.

```json
{
  "id": "bx",
  "version": "<commit-short>",
  "install_type": "build",
  "source": { "git": "https://github.com/bkaradzic/bx.git", "commit": "<full-sha>" },
  "build": { "command": "echo bx is header+source only; nothing to build separately", "working_dir": "<clone_root>" },
  "stage": [
    { "from": "include", "to": "External/bx/include" },
    { "from": "src",     "to": "External/bx/src" },
    { "from": "tools/bin/windows/genie.exe", "to": "External/bx/tools/genie.exe" }
  ],
  "sentinel_inputs": [ "External/bx/tools/genie.exe" ]
},
{
  "id": "bimg",
  "version": "<commit-short>",
  "install_type": "build",
  "source": { "git": "https://github.com/bkaradzic/bimg.git", "commit": "<full-sha>" },
  "build": { "command": "echo bimg is header+source only; nothing to build separately", "working_dir": "<clone_root>" },
  "stage": [
    { "from": "include", "to": "External/bimg/include" },
    { "from": "src",     "to": "External/bimg/src" },
    { "from": "3rdparty","to": "External/bimg/3rdparty" }
  ],
  "depends_on": ["bx"],
  "sentinel_inputs": []
}
```

(bx and bimg are header+source contributed in-place to bgfx's GENie build, so their "build" step is a no-op stage. bgfx's build is the substantive one.)

`bgfx`'s entry as shown above. Note the sentinel skips the trivial bx/bimg sentinels and lands on the bgfx static libs and shaderc.exe — those are what every other consumer depends on.

## CLI Interface

No new CLI surface. The existing commands extend transparently:

```bash
dia env setup                      # restores all deps including bgfx (build runs once)
dia env setup --deps               # same — bgfx build invoked as part of deps step
dia env setup --dep bgfx           # restores only bgfx (and bx/bimg if missing)
dia env setup --dep bgfx --force   # full rebuild from clean clone
dia env verify                     # reports PASS/WARN/FAIL for bgfx alongside other deps
dia env verify --deps              # same, scoped to deps
```

`--dep bgfx` automatically resolves and builds `bx` and `bimg` first via `depends_on`.

## Implementation

### Files modified

```
Dia/DiaCLI/dia_cli/utils/deps_restore.py
   - Add _install_build() method
   - Extend schema validation to accept install_type:"build"
   - Add depends_on resolution before iteration (topological sort)
   - Sentinel content extended: includes sentinel_inputs SHA-256s

deps.json (repo root)
   - Three new entries: bx, bimg, bgfx (in order)

External/.gitignore
   - Already ignores External/* — confirm bx/, bimg/, bgfx/ are ignored

.diaenv/deps/  (existing sentinel directory)
   - bx.restored, bimg.restored, bgfx.restored sentinels created on success
```

### `_install_build()` flow

```python
def _install_build(entry):
    clone_dir = self._clone_dir(entry["id"])  # .diaenv/build/<id>/

    # 1. Clone or fetch
    if not clone_dir.exists():
        git_clone(entry["source"]["git"], clone_dir)
    git_checkout(clone_dir, entry["source"]["commit"])

    # 2. Build
    cmd = entry["build"]["command"].replace("<clone_root>", str(clone_dir))
    workdir = entry["build"]["working_dir"].replace("<clone_root>", str(clone_dir))
    subprocess.run(cmd, shell=True, check=True, cwd=workdir, env=msbuild_env())

    # 3. Stage
    for stage in entry["stage"]:
        src = clone_dir / stage["from"]
        dst = repo_root / stage["to"]
        if src.is_dir():
            shutil.copytree(src, dst, dirs_exist_ok=True)
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

    # 4. Sentinel
    sentinel_hashes = {p: sha256_file(repo_root / p) for p in entry.get("sentinel_inputs", [])}
    write_sentinel(entry["id"], {
        "id": entry["id"],
        "version": entry["version"],
        "commit": entry["source"]["commit"],
        "input_hashes": sentinel_hashes,
        "restored_at": now_iso(),
    })
```

### `depends_on` resolution

Add a topological sort step before iterating deps:

```python
def _resolve_order(deps):
    # deps without depends_on come first; each dep with depends_on must follow its prereqs
    sorted_ids = topological_sort(deps, key=lambda d: d.get("depends_on", []))
    return [next(d for d in deps if d["id"] == id) for id in sorted_ids]
```

Existing zip/exe deps (no `depends_on`) are unaffected — they continue to restore in declaration order.

### `dia env verify` extension

`verify_deps()` reports each dep:
- **PASS** — sentinel present, all `sentinel_inputs` files present and SHA-256 matches
- **WARN** — sentinel absent but `External/<id>/` directory looks valid (manual install)
- **FAIL** — sentinel absent and primary outputs missing (e.g. no `External/bgfx/lib/Release/bgfx.lib`)

For source-built deps, an additional `WARN` case: sentinel present but a `sentinel_inputs` SHA mismatches → "build artifacts modified or corrupted; run `dia env setup --dep bgfx --force`".

### MSBuild environment

The build command requires `msbuild.exe` on PATH. `dia env setup` already verifies VS 2022 is installed (`winget-manifest`). `_install_build()` shells out via `cmd.exe` with the VS Developer Command Prompt environment — use `vswhere -find VC/Auxiliary/Build/vcvars64.bat` and source it before invoking the build command.

### Disk usage

- `.diaenv/build/bgfx/` clone: ~80 MB
- `.diaenv/build/bx/` clone: ~10 MB
- `.diaenv/build/bimg/` clone: ~30 MB
- `External/bgfx/` staged output: ~30 MB (libs + headers + shaderc.exe)

`.diaenv/build/` is gitignored. Sentinel-only re-runs touch nothing.

## Files Introduced / Modified

| File | Change |
|------|--------|
| `deps.json` | Add `bx`, `bimg`, `bgfx` entries |
| `Dia/DiaCLI/dia_cli/utils/deps_restore.py` | Add `_install_build()`, extend schema validator, add topological sort for `depends_on` |
| `Dia/DiaCLI/dia_cli/commands/env/verify_cmd.py` | Extend dep verification to handle build deps' `sentinel_inputs` SHA-check |
| `.gitignore` | Add `.diaenv/build/` (clone cache) |
| `External/bgfx/` | New directory created by setup; gitignored |
| `External/bx/` | New directory created by setup; gitignored |
| `External/bimg/` | New directory created by setup; gitignored |

No `.vcxproj` changes in this feature — `DiaBgfx.vcxproj` is created in a later feature (`diabgfx-canvas-parity`) and consumes `External/bgfx/`.

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `deps-manifest` (Done) | Hard | This feature extends `deps.json` schema and `deps_restore.py`; cannot land before |
| `winget-manifest` (Done) | Hard | VS 2022 + Git must be installed for the build step |
| `env-setup-cmd` (Done) | Hard | `dia env setup --dep bgfx` extends the existing setup command surface |
| `env-verify-cmd` (Done) | Hard | `dia env verify` must learn to verify build deps |

## Acceptance Criteria

1. `deps.json` contains `bx`, `bimg`, `bgfx` entries with pinned commits and the `build` install type
2. `dia env setup --dep bgfx` on a clean machine clones bx, bimg, bgfx; runs the bgfx GENie + msbuild step; stages `External/bgfx/include/`, `External/bgfx/lib/Release/bgfx.lib`, `External/bgfx/lib/Debug/bgfx.lib`, `External/bgfx/tools/shaderc.exe`
3. After step 2 succeeds, `External/bgfx/lib/Release/bgfx.lib` exists, is non-empty, and is a valid x64 static library (verified by `dumpbin /headers` showing `machine: x64`)
4. After step 2 succeeds, `External/bgfx/tools/shaderc.exe` exists and runs (`shaderc.exe --help` exits 0)
5. Re-running `dia env setup --dep bgfx` after success completes in <1 second (sentinel skip)
6. `dia env setup --dep bgfx --force` rebuilds from clean clone (deletes `.diaenv/build/bgfx/`, `External/bgfx/`, sentinel) and produces a working build
7. `dia env verify --deps` reports `PASS` for bx, bimg, bgfx after a successful setup
8. `dia env verify --deps` reports `FAIL` for bgfx if `External/bgfx/lib/Release/bgfx.lib` is deleted (sentinel says present, file missing)
9. `dia env verify --deps` reports `WARN` for bgfx if a `sentinel_inputs` file's SHA-256 has changed (e.g. corrupted)
10. `dia env setup` (no flags, full bootstrap) on a clean machine builds bgfx as part of the deps step without manual intervention
11. `--dep bgfx` automatically restores `bx` and `bimg` first via `depends_on` resolution
12. The schema validator rejects a `deps.json` entry that has both `install_type:"zip"` and `source` (mutually exclusive shapes)
13. Build failure (e.g. compile error) exits non-zero, prints the msbuild error output, leaves no sentinel — re-run resumes from clone+checkout
14. Disk space after a clean setup: `.diaenv/build/` ≤ 200 MB, `External/bgfx/` ≤ 50 MB

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEnv | @docs/specs/systems/dia/diaenv.md |
| Cross-cutting system | RenderBackend | @docs/specs/systems/dia/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all identifiers | Compliant — Python tooling; no C++ identifier surface introduced |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — development tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — development tooling; no entity system used |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only; no C++ public API surface |
| PD-005 | Platform | x64 Windows only | Compliant — bgfx built with `/p:Platform=x64`; no Win32/ARM target |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant — `DiaBgfx.vcxproj` (created in later feature) consumes `External/bgfx/`; bgfx's own GENie/CMake build is contained inside `.diaenv/build/` and produces static libs that `.vcxproj` references the standard way (no top-level CMake) |
| PD-007 | Platform | C++20 required | Compliant — bgfx public headers verified to compile under `/std:c++20` as part of the `diabgfx-canvas-parity` feature; this feature only stages headers |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — bgfx's own build outputs to `.diaenv/build/bgfx/.build/`; staged into `External/bgfx/lib/<Config>/` for `.vcxproj` consumption; no Cluiche `Directory.Build.props` settings touched |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | N/A — `.diaenv/build/` is dev-tooling intermediate, not application output; aligns with existing `.diaenv/deps/` sentinel directory pattern |
| AD-001 | Dia App | Module YAML frontmatter documentation | N/A — no new C++ Dia module introduced by this feature |
| AD-002 | Dia App | No STL containers in public APIs | Compliant — Python tooling; no C++ public API |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | N/A — Python tooling |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for app structure | N/A — development tooling |
| AD-005 | Dia App | Component-based entities | N/A — development tooling |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — extends existing `deps_restore_cmd.py` plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — all new code in Python |
| SD-CLI-003 | DiaCLI | Separate from C++ DiaAPI | Compliant — no C++ DiaAPI commands introduced |
| SD-CLI-006 | DiaCLI | Click framework for argument parsing | Compliant — uses existing `deps_cmd.py` Click surface unchanged |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exits 0 on success, 1 on hard failure (build error, hash mismatch), 2 on warnings |
| SD-ENV-001 | DiaEnv | `deps.json` is single source of truth for binary SDKs | Compliant — bgfx, bx, bimg added as `deps.json` entries; no parallel manifest |
| SD-ENV-002 | DiaEnv | SHA-256 verification required for all downloaded deps | Compliant — for build deps, `sentinel_inputs` SHA-256 protects the *built* artifacts (which is what consumers depend on); the source repo is pinned by commit SHA which is git's content-addressable hash |
| SD-ENV-004 | DiaEnv | `winget.json` generated by `winget export` and committed | N/A — no toolchain change |
| SD-ENV-010 | DiaEnv | Python 3.11 is the single Python version | Compliant — implementation uses Python 3.11 stdlib (subprocess, shutil, hashlib) |
| RB-010 | RenderBackend | bgfx prebuilt via GENie/CMake; consumed via `.vcxproj` references | Compliant — this feature implements exactly that orchestration |
| RB-017 | RenderBackend | DiaCLI env setup/verify must learn bgfx as a first-class concern | Compliant — this feature is the implementation of RB-017 |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Schema | Should `install_type:"build"` be a generic facility, or bgfx-specific? | Generic. The `build` install type is designed to support any source-built dep with `clone → build command → stage` semantics. Future deps (e.g. a tool that has no prebuilt distribution) can use the same shape. The bgfx-specific knowledge is entirely in `deps.json`, not in `deps_restore.py`. |
| 2 | Build environment | What if the developer's MSVC toolchain version differs from the one bgfx was tested against? | bgfx supports VS 2022 with the standard toolset; `winget.json` pins VS 2022. If a developer has a non-standard toolchain, the build may fail with a clear msbuild error. We do not pin a specific MSVC sub-version — that's a separate concern owned by `winget-manifest`. |
| 3 | Disk caching | Should the `.diaenv/build/<id>/` clone be reused across `--force` rebuilds, or always cleaned? | Always cleaned on `--force`. `--force` semantics are "ensure I get a fresh, correct build" — keeping the clone risks stale state from a partially-applied checkout. Clone is fast (`git clone --depth 1` from the pinned commit takes seconds). |
| 4 | Build artifacts | We stage Debug + Release bgfx libs. Do we need a Profile/Tuning variant? | No — Cluiche's existing configurations are Debug and Release (PD-005 platform). No Profile/Tuning configurations exist. If they're added in the future, this feature gets a Phase 2 update. |
| 5 | Pinned commit | How do we pick the bgfx pinned commit? Latest master, latest tagged release, or a specific known-good SHA? | A specific known-good SHA — pinned at the moment this feature is implemented. We do not auto-track upstream master. Bumping the pin is a deliberate, reviewed change (its own small PR). The SHA value is filled in during implementation (Step 1 of the plan), not at spec time. |
| 6 | Mirror fallback | Existing zip deps support `mirrors` for offline/slow-network cases. Do build deps support clone mirrors? | Yes, but defer the implementation — `source.git` could be extended to `source.mirrors` (list of fallback git URLs) in a future feature. For now, network access to GitHub is assumed; this matches the existing source-only deps in the repo (googletest etc. as submodules). |
| 7 | Shaderc on PATH | Should `External/bgfx/tools/shaderc.exe` be added to PATH so DiaPipeline can invoke it as `shaderc`? | No — DiaPipeline invokes it by absolute path (`External/bgfx/tools/shaderc.exe`). PATH manipulation is global and risky; absolute paths are explicit and the standard pattern for tool-bundling deps in this repo. |
| 8 | Build parallelism | `msbuild /m` uses all cores by default. Should we cap it for memory-constrained machines? | No — match upstream bgfx's default. If a machine OOMs, the developer can re-run with `--force` after closing other apps. Adding a cap field to the schema is premature complexity. |
| 9 | Failure recovery | If the build half-succeeds (one config builds, the other fails), what state is left? | No sentinel is written, so the next setup re-runs from the build step (clone is preserved). Stage step is atomic per file (`shutil.copy2` of each artifact), so partial-stage state is detectable on retry. The build step is not atomic, but msbuild is well-behaved on re-invocation — the second run resumes incrementally. |
| 10 | Sentinel format | Existing zip deps have a simple sentinel `{ id, version, sha256, restored_at }`. The new build sentinel adds `commit` and `input_hashes` — is the format change backwards-compatible? | Yes — the new fields are additive. The verify command reads `input_hashes` for build deps and ignores it for zip/exe deps (key is absent). Existing zip-dep sentinels remain valid; no migration required. |
| 11 | bx/bimg as separate entries | bx and bimg are header+source-only and contributed in-place to bgfx's build. Why are they separate `deps.json` entries instead of substeps of bgfx? | Reusability and clarity. Future deps may consume bx independently (it's a general-purpose foundation library). Keeping them as separate entries with explicit `depends_on` makes the dependency graph visible and reusable. The cost is three sentinels instead of one. |
| 12 | CI integration | If CI runs `dia env setup`, every CI run will rebuild bgfx unless the cache is preserved. How should CI handle this? | Out of scope for this feature — CI cache strategy is owned by the CI configuration, not DiaEnv. CI can persist `.diaenv/` and `External/bgfx/` between runs to skip the build. This is documented as a CI implementation note in the rollout, not a feature requirement. |

---
