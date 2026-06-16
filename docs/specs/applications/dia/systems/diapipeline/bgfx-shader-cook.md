# Feature Spec: bgfx-shader-cook

## Parent System
@docs/specs/applications/dia/systems/diapipeline/diapipeline.md

**Cross-cutting system:** @docs/specs/applications/dia/systems/render-backend/render-backend.md (RB-011 — bgfx `shaderc` cook step is a first-class pipeline concern)

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Done`

**Plan:** @docs/specs/applications/dia/systems/diapipeline/bgfx-shader-cook.plan.md

## Summary

Add `bgfx_shaders` as a new `build_deps` sub-step of `compile-code` in DiaPipeline, mirroring the existing `protobuf` and `cef_wrapper` sub-steps. The sub-step invokes the bgfx `shaderc.exe` (staged into `External/bgfx/tools/shaderc.exe` by `bgfx-env-setup`) on every `.sc` shader file under `Dia/DiaBgfx/Shaders/` and writes per-backend cooked binaries to `Cluiche/out/<AppName>/shaders/<backend>/<shader>.bin`. Sentinel-driven incremental cook (skip unchanged files); `--force` forces a full re-cook.

This feature defines the cook step but **does not author any shaders** — `Dia/DiaBgfx/Shaders/` does not exist yet. The first `.sc` shader files arrive in the `diabgfx-canvas-parity` feature (sprite, debug-line, debug-rect, UI-overlay shaders). This feature is intentionally specified ahead of `diabgfx-canvas-parity` so that when the bgfx canvas implementer needs a shader pipeline, the pipeline already exists.

## Problem

bgfx ships its own shader cross-compiler tool, `shaderc`, that takes a `.sc` source file plus a `varying.def.sc` definition and produces per-backend cooked binaries (one for each enabled bgfx backend: D3D11 / D3D12 / Vulkan / GL on Windows). Without a cook step:

- Shaders cannot be loaded at runtime — `bgfx::createShader` requires backend-specific bytecode, not source
- Every developer would manually invoke `shaderc.exe` per shader per backend (16+ commands for a 4-shader baseline × 4 backends)
- Hot-reload, CI, and Docker-pipeline runs cannot cook shaders without scripted automation

The existing DiaPipeline `compile-code` stage already cooks two non-shader artifacts (protobuf C++ via `protoc`, CEF wrapper via cmake). Bgfx shaders fit the same pattern: a pre-MSBuild build_dep that is per-target, sentinel-skippable, and `pipeline.toml`-configurable. Per RB-011, this must be a first-class concern, not ad-hoc per-developer scripting.

## Goals

- Add `build_deps.bgfx_shaders = true` to `pipeline.toml` schema; available on any target
- Implement a new build_deps sub-step in `compile-code` that, when triggered:
  - Discovers every `.sc` file under `Dia/DiaBgfx/Shaders/` (scanned recursively; vertex shaders prefixed `vs_`, fragment shaders prefixed `fs_`, compute shaders prefixed `cs_`)
  - For each `.sc`, runs `shaderc.exe` once per active bgfx backend (default: `dx11`, `dx12`, `vulkan`)
  - Writes cooked binaries to `Cluiche/out/<AppName>/shaders/<backend>/<shader>.bin` (per PD-009)
  - Maintains a per-shader sentinel file (`.diaenv/shaders/<backend>/<shader>.sentinel`) keyed by source-file SHA-256; skip-if-unchanged-and-not-force
- Active backend list configurable in `pipeline.toml` `[bgfx_shaders] backends = ["dx11", "dx12", "vulkan"]`; default applies if unset
- Locate `shaderc.exe` at `External/bgfx/tools/shaderc.exe`; fail with a clear error pointing at `dia env setup --dep bgfx` if missing
- `varying.def.sc` is per-shader-folder (one per directory; the convention bgfx uses); cook discovers it implicitly per directory
- Stream `shaderc.exe` output to stdout; exit 1 on any cook failure; propagate the failure path/message
- `--force` re-cooks every shader regardless of sentinels
- Sub-step runs **before** MSBuild so `DiaBgfx.vcxproj` (which loads cooked shader binaries at startup or runtime) is built against fresh cooked output
- `dia pipeline --target cluichetest --stage compile-code` runs the cook automatically when `bgfx_shaders = true` is set on cluichetest

## Non-Goals

- **Authoring shaders** — `Dia/DiaBgfx/Shaders/` is created in `diabgfx-canvas-parity`; this feature only defines the cook contract
- **Runtime shader hot-reload** — the cook step writes to disk; runtime hot-reload (file watcher → reload bgfx shader handle) is out of scope and may be added later as part of a debug feature
- **Cooking shaders for non-bgfx backends** — only bgfx-shaderc-style `.sc` files are handled; other shader systems (e.g. SFML's `.frag` files like `ui.frag`) are out of scope
- **Shader source generation / template expansion** — `.sc` files are author-written; no codegen step
- **Shader binary distribution** — cooked binaries are gitignored under `Cluiche/out/` (PD-009); developers re-cook locally
- **Cross-platform cook** — Windows-only (PD-005); `shaderc.exe` is the Windows binary built in `bgfx-env-setup`
- **Removing `xcopy` from .vcxproj** — out of scope per existing SD-PIPE-006
- **Running `shaderc` in parallel** — sequential per shader, parallel only across the implicit `--config Both` outer loop; parallelisation can be a later optimisation if cook time becomes load-bearing

## `pipeline.toml` schema extension

Add a new `[bgfx_shaders]` global section and a per-target `build_deps.bgfx_shaders` toggle:

```toml
# pipeline.toml (repo root)

[bgfx_shaders]
# Active backends to cook. Order is the priority order at runtime if a
# backend is unavailable. Default applied if section is absent.
backends = ["dx11", "dx12", "vulkan"]

# Source root scanned for .sc files (relative to repo root).
source_root = "Dia/DiaBgfx/Shaders"

# Output root (relative to repo root).
# Resolves $(AppName) at cook time per the active --target.
output_root = "Cluiche/out/$(AppName)/shaders"

# Path to shaderc.exe. Default points at bgfx-env-setup output.
shaderc_path = "External/bgfx/tools/shaderc.exe"

# ... existing sections ...

[targets.cluichetest]
project = "Cluiche/CluicheTest/CluicheTest.vcxproj"
stages = ["compile-code", "build-assets", "deploy"]

[targets.cluichetest.build_deps]
protobuf = true
bgfx_shaders = true   # NEW
```

When `bgfx_shaders = true` is set on a target and `compile-code` runs, the cook sub-step fires. When unset (or `false`), no cook runs for that target — useful for `googletest` (which builds DiaBgfx.lib but does not need cooked shaders to run unit tests).

## Cook flow

```
1. Sub-step entry (compile-code build_deps loop):
   - bgfx_shaders == true for active target?
     - no  -> skip; log "bgfx_shaders disabled for <target>"
     - yes -> continue
   - shaderc.exe path exists?
     - no  -> exit 1: "shaderc.exe missing — run `dia env setup --dep bgfx`"
   - source_root exists and contains at least one .sc?
     - no  -> log "no .sc files under <source_root>; skipping" (not an error — DiaBgfx may pre-date its first shader)
     - yes -> continue

2. Per-shader cook (recursive discovery):
   for each .sc under source_root:
     classify by filename prefix:
       vs_  -> --type vertex
       fs_  -> --type fragment
       cs_  -> --type compute
     locate adjacent varying.def.sc (same directory)
     for each backend in [bgfx_shaders.backends]:
       compute source_sha = sha256(shader.sc + varying.def.sc + shaderc binary version)
       sentinel_path = .diaenv/shaders/<backend>/<rel_shader_path>.sentinel
       if sentinel exists and sentinel.source_sha == source_sha and not --force:
         skip
         continue
       output_path = <output_root>/<backend>/<rel_shader_path>.bin
       run: shaderc.exe -f <shader.sc> -o <output_path>
                        --type <vertex|fragment|compute>
                        --varyingdef <varying.def.sc>
                        --platform <windows>
                        --profile <backend-specific>
                        -i <bgfx-include-paths>
                        -O 3
       on failure: exit 1, propagate stderr
       on success: write sentinel { source_sha, cooked_at }

3. Summary:
   log "cooked N shaders, skipped M (up-to-date)"
```

### Backend → shaderc profile mapping

| backend       | profile     | platform |
|---------------|-------------|----------|
| `dx11`        | `s_5_0`     | windows  |
| `dx12`        | `s_5_0`     | windows  |
| `vulkan`      | `spirv`     | windows  |
| `gl` (future) | `120`       | windows  |

The mapping is hardcoded in the cook script (small, stable table — bgfx's own `runtime_check.cpp` uses identical values).

### Output layout

```
Cluiche/out/cluichetest/shaders/
├── dx11/
│   ├── vs_sprite.bin
│   ├── fs_sprite.bin
│   ├── vs_debug.bin
│   ├── fs_debug.bin
│   ├── vs_ui_overlay.bin
│   └── fs_ui_overlay.bin
├── dx12/
│   └── (same)
└── vulkan/
    └── (same)
```

(Shader names are illustrative — actual files arrive in `diabgfx-canvas-parity`.)

At runtime, `DiaBgfx::Canvas::Initialize` selects the matching folder based on `bgfx::getRendererType()` and loads each `.bin` via `bgfx::createShader`.

### Sentinel format

```json
{
  "id": "vs_sprite.dx11",
  "source_sha": "<sha256 of shader.sc + varying.def.sc + shaderc binary version>",
  "cooked_at": "2026-05-17T14:23:00Z",
  "output_path": "Cluiche/out/cluichetest/shaders/dx11/vs_sprite.bin"
}
```

`source_sha` includes the shaderc binary version string so that bumping the bgfx pinned commit (which rebuilds shaderc) invalidates all sentinels and forces a re-cook. This avoids stale binaries cooked by an old shaderc continuing to be reused.

## CLI Interface

No new top-level CLI surface. Existing commands extend transparently:

```bash
dia pipeline --target cluichetest                    # cooks shaders if bgfx_shaders=true
dia pipeline --target cluichetest --stage compile-code
dia pipeline --target cluichetest --force            # force re-cook all shaders + rebuild
```

A `--skip-shaders` flag is intentionally **not** added — disabling the cook for a target is done via `pipeline.toml` (`bgfx_shaders = false`), keeping CLI surface minimal.

## Implementation

### Files modified

```
Dia/DiaCLI/dia_cli/commands/pipeline/compile_code.py
   - Extend build_deps loop to handle "bgfx_shaders" sub-step
   - Add _cook_bgfx_shaders() function

Dia/DiaCLI/dia_cli/commands/pipeline/bgfx_shader_cook.py
   - NEW — cook orchestration:
     * Discover .sc files (Path.rglob)
     * Compute per-shader source SHA
     * Read/write sentinel files
     * Invoke shaderc.exe via subprocess
     * Stream output
   - Imports OutputContext for structured logging

Dia/DiaCLI/dia_cli/utils/pipeline_config.py
   - Extend pipeline.toml schema validator:
     * [bgfx_shaders] global section (optional)
     * [targets.<x>.build_deps] supports new "bgfx_shaders" boolean

pipeline.toml (repo root)
   - Add [bgfx_shaders] global section with default backends
   - Add bgfx_shaders = true to [targets.cluichetest.build_deps]
   - Add bgfx_shaders = false to [targets.googletest.build_deps] explicitly (future-proof; not strictly required since absent == false)

.gitignore
   - Confirm .diaenv/shaders/ is ignored (already covered by .diaenv/)
```

No `.vcxproj` changes; no DiaBgfx files (those arrive in `diabgfx-canvas-parity`).

### Events emitted

Reuses existing `OutputContext` channels — no new event types:

- `OnStageStarted("compile-code", target, config)` — already emitted
- Per-shader log lines: `"cooking vs_sprite.sc -> dx11/vs_sprite.bin"` and `"vs_sprite.sc dx11 up-to-date (sentinel)"`
- `OnStageFailed("compile-code", target, config, "<shader> cook failed: <stderr>")` — emitted when shaderc returns non-zero

### Cook performance budget

- Cooking 6 shaders × 3 backends = 18 invocations.
- Each shaderc.exe invocation is ~200ms cold-start + ~100ms compile = ~300ms.
- Full cold cook: ~5–6 seconds.
- Warm re-run (all sentinels valid): <100ms (sentinel reads only).
- This is comparable to the existing protobuf cook (one protoc invocation, ~500ms).

If cook time grows beyond ~30 seconds for a typical project as more shaders are added, parallelisation across `(.sc × backend)` pairs becomes worthwhile and is captured as a follow-up note. Not in scope here.

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaCLI/dia_cli/commands/pipeline/bgfx_shader_cook.py` | NEW — cook orchestration |
| `Dia/DiaCLI/dia_cli/commands/pipeline/compile_code.py` | Extend build_deps loop |
| `Dia/DiaCLI/dia_cli/utils/pipeline_config.py` | Schema validator for `bgfx_shaders` |
| `pipeline.toml` (repo root) | Add `[bgfx_shaders]` section + per-target toggle |
| `.diaenv/shaders/` | New sentinel directory (gitignored via `.diaenv/`) |
| `Cluiche/out/<target>/shaders/` | New cook output directory (gitignored via `Cluiche/out/`) |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `bgfx-env-setup` | Hard | Provides `External/bgfx/tools/shaderc.exe`; cook fails with a clear error if missing |
| `pipeline-config` (Done) | Hard | This feature extends `pipeline.toml` schema |
| `compile-code` (Done) | Hard | This feature adds a new build_deps sub-step inside compile-code |
| `cli-output` (Done) | Soft | Per-shader log lines reuse existing `OutputContext` channels |
| `diabgfx-canvas-parity` | Reverse | The first `.sc` files land in that feature; this feature's cook is unused until then but harmless (cook runs over zero files = no-op log) |

## Acceptance Criteria

1. `pipeline.toml` accepts a `[bgfx_shaders]` global section with `backends`, `source_root`, `output_root`, `shaderc_path` fields; defaults applied when section absent
2. `pipeline.toml` accepts `build_deps.bgfx_shaders = true|false` per-target; absent treated as false
3. `dia pipeline --target cluichetest --stage compile-code` invokes the bgfx shader cook before MSBuild when `bgfx_shaders = true`
4. Cook discovers `.sc` files recursively under `source_root` (verified with a synthetic `Dia/DiaBgfx/Shaders/test/vs_dummy.sc` fixture deleted after the test)
5. Cook produces per-backend `.bin` files under `Cluiche/out/<target>/shaders/<backend>/` matching the active backends list
6. Cook writes a sentinel `.diaenv/shaders/<backend>/<rel_path>.sentinel` per `(shader, backend)` pair containing source SHA + cooked timestamp + output path
7. Re-running the cook with no source changes completes in <500 ms total (all sentinels valid; no shaderc invocations)
8. Modifying a `.sc` source file invalidates its sentinels (across all backends) and triggers re-cook for that shader only
9. Bumping the pinned bgfx commit (which rebuilds shaderc) invalidates all sentinels (because `source_sha` includes shaderc version) and triggers a full re-cook
10. `--force` re-cooks all shaders regardless of sentinels
11. Missing `shaderc.exe` exits 1 with the message: `"shaderc.exe missing at <path> — run \`dia env setup --dep bgfx\`"`
12. shaderc cook failure exits 1, propagates the failure path and stderr to terminal output, and does **not** invoke MSBuild
13. `dia pipeline --target googletest` does **not** invoke the cook (build_deps.bgfx_shaders not set / false) — confirmed by absence of cook log lines
14. NDJSON event log records `OnStageStarted("compile-code", ...)` and any `OnStageFailed` with the shader path; no new event types added
15. Per PD-009, all cooked output lands under `Cluiche/out/<AppName>/shaders/`; nothing written to repo root or `Dia/DiaBgfx/`
16. With zero `.sc` files in `Dia/DiaBgfx/Shaders/` (the state at this feature's merge time), the cook logs `"no .sc files under <source_root>; skipping"` and exits 0 (not an error)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System (primary) | DiaPipeline | @docs/specs/applications/dia/systems/diapipeline/diapipeline.md |
| System (cross-cutting) | RenderBackend | @docs/specs/applications/dia/systems/render-backend/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | N/A — Python tooling only; no C++ identifier surface |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | N/A — development tooling |
| PD-003 | Platform | Component-based entities | N/A |
| PD-004 | Platform | No STL containers in public APIs | N/A — Python only |
| PD-005 | Platform | x64 Windows only | Compliant — shaderc.exe is Windows x64; bgfx backends scoped to Windows-supported set (dx11/dx12/vulkan/gl) |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` modified; cook is an MSBuild pre-step orchestrated from Python |
| PD-007 | Platform | C++20 required | N/A — tooling feature |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — cook output uses Cluiche/out/, not the binary OutDir |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | **Compliant — this feature's outputs are the canonical example: `Cluiche/out/<target>/shaders/<backend>/<shader>.bin`** |
| PD-010 | Platform | `.diagame` typed imports | N/A |
| AD-001 | Dia App | Module YAML frontmatter | N/A — Python tooling, no new C++ module |
| AD-002 | Dia App | No STL in public APIs | N/A |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | N/A |
| AD-004 | Dia App | ProcessingUnit/Phase/Module | N/A |
| AD-005 | Dia App | Component-based entities | N/A |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — extends existing pipeline plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — all new code in Python |
| SD-CLI-006 | DiaCLI | Click framework | Compliant — uses existing `pipeline_cmd.py` Click surface unchanged |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exits 0 on success, 1 on cook failure or missing shaderc, 2 on bad config |
| SD-PIPE-001 | DiaPipeline | `pipeline.toml` is single source of truth | Compliant — schema extends `pipeline.toml`, no parallel config file |
| SD-PIPE-002 | DiaPipeline | Stage ordering: compile-code → build-assets → deploy | Compliant — cook is a build_dep sub-step *inside* compile-code (per SD-PIPE-007), not a new stage |
| SD-PIPE-003 | DiaPipeline | `$(OutDir)` and `$(Configuration)` resolved at runtime | Compliant — `$(AppName)` resolution mirrors the existing pattern |
| SD-PIPE-005 | DiaPipeline | build-assets stage is a no-op stub | Compliant — shader cook is a build_dep, not in build-assets |
| SD-PIPE-007 | DiaPipeline | Build deps are sub-steps of compile-code, not separate stages | **Compliant — this feature follows the protobuf/cef_wrapper precedent exactly** |
| SD-ENV-002 | DiaEnv | SHA-256 verification required for downloaded deps | Compliant transitively — `shaderc.exe` integrity is owned by `bgfx-env-setup`'s sentinel (which SHAs the staged binary) |
| RB-010 | RenderBackend | bgfx prebuilt via GENie/CMake; consumed via .vcxproj | Compliant — uses staged shaderc.exe from bgfx-env-setup |
| RB-011 | RenderBackend | bgfx shaderc invoked as a cook step in DiaPipeline | **Compliant — this feature is the implementation of RB-011** |
| RB-018 | RenderBackend | Phase 3 (DiaTerrain) out of scope | N/A — this feature is Phase 1 |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Schema location | Why `[bgfx_shaders]` global instead of per-target shader settings? | Global because the source root, output convention, and shaderc path are repo-wide concerns; per-target configurability is via the `build_deps.bgfx_shaders` boolean. If a future game needs per-target backend lists, the schema can grow `[targets.<x>.bgfx_shaders.backends]` overrides — not needed now. |
| 2 | Backends default | Why `["dx11", "dx12", "vulkan"]` and not `["dx11"]` only? | Per RB-012, the default runtime backend is deferred but bgfx supports D3D11/D3D12/Vulkan on Windows. Cooking all three at the cost of ~5 extra seconds is cheap insurance. If runtime perf testing favours one backend exclusively, the list shrinks. |
| 3 | varying.def.sc | Where does varying.def.sc live — one per shader, or one per directory? | bgfx convention is one per directory (or one per shader pair). The cook discovers it implicitly per shader's parent directory. If absent, shaderc fails with a clear error and the cook step propagates it. |
| 4 | Profile selection | Why hardcode `s_5_0` for both dx11 and dx12? | bgfx's own examples use s_5_0 for both. D3D12 backend in bgfx accepts SM5 bytecode and translates internally. SM6 is not yet first-class in bgfx as of the pinned commit. If/when bgfx upstream adds SM6, the mapping table updates. |
| 5 | Source SHA scope | Should the source SHA include shaderc include paths or other build flags? | Yes — implementation detail. The SHA covers: the .sc file, the adjacent varying.def.sc, the shaderc binary version, and the active backend's profile string. Anything that changes the cooked output should be in the SHA. Captured in implementation step. |
| 6 | Cooking on every dia run | `dia run cluichetest` doesn't currently invoke `dia pipeline`. Does the cook fire automatically? | No — `dia run` runs the existing executable assuming it was built. Developers run `dia pipeline --target cluichetest` to refresh both binary and shaders. This matches the existing pattern (protobuf cook also doesn't fire from `dia run`). A `dia pipeline && dia run` chain in DiaCLI's launch sequence is a separate UX consideration outside this feature. |
| 7 | DiaBgfx.vcxproj copy step | Does `DiaBgfx.vcxproj` need a post-build xcopy of cooked shaders to the OutDir? | No — cooked shaders live under `Cluiche/out/<App>/shaders/`. `DiaBgfx::Canvas` reads them from that fixed path at startup. No copy to per-binary OutDir is needed. This avoids the SD-PIPE-006 xcopy debt growing. |
| 8 | Hot-reload | The SFML UI shader (`ui.frag`) is loaded at canvas init. Should bgfx shaders be hot-reloadable? | Out of scope for this feature. A future debug feature could file-watch `Cluiche/out/<App>/shaders/` and call a `DiaBgfx::Canvas::ReloadShader(StringCRC)` API. Captured as a follow-up note in the system spec's RB-011 area, not implemented here. |
| 9 | CI cook caching | If CI runs `dia pipeline` from scratch every time, every CI run pays the full cook cost. Acceptable? | Yes, ~5–6s on cold cook is a small fraction of msbuild time (~minutes). CI optimisation is owned by the CI config (caching `.diaenv/shaders/` and `Cluiche/out/<App>/shaders/`), not this feature. |
| 10 | Profile fallback | What if a developer's `bgfx-env-setup` was run before D3D12 was added to backends? Does shaderc support all backends in one binary? | Yes — `shaderc.exe` is a cross-compiler that targets all backends from a single binary. Different `--profile` and `--platform` flags select the output. No per-backend shaderc binaries. Verified against bgfx upstream. |
| 11 | Empty source_root | This feature ships with no .sc files (those land in diabgfx-canvas-parity). Does the cook log a warning every pipeline run? | No — when `bgfx_shaders = true` but no .sc files exist, log at info level: `"no .sc files under <source_root>; skipping"`. Not a warning. The first .sc file checked in by `diabgfx-canvas-parity` flips it to actual cook output. |
| 12 | shaderc include paths | What -i include paths does shaderc need? | bgfx's own `bgfx_shader.sh` and the `src/` include directory must be on the include path. Both staged into `External/bgfx/include/` by `bgfx-env-setup`. The cook script passes `-i External/bgfx/src` and `-i External/bgfx/include` (and any per-shader-folder local includes). Captured in implementation step. |
| 13 | NDJSON noise | If 18 shaders cook and emit per-shader log lines, does the NDJSON event stream become noisy? | Per-shader log lines go to terminal output via OutputContext.info, not to the NDJSON event stream. NDJSON only sees `OnStageStarted` and (on failure) `OnStageFailed`. Matches the existing protobuf cook's behaviour. |

---
