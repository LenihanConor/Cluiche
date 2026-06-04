**Spec:** @docs/specs/systems/dia/diaarchitecture.md
**Status:** In Progress

---

## Implementation Phases

### Phase 1 — Layer Field Updates (documentation only, no code changes)

All tasks are independent — can be dispatched in parallel.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Set `layer: foundation/core` on DiaCore, DiaSerializer, DiaThreading, DiaMailbox, DiaStateMachine, DiaProtobuf, DiaPicking | grep confirms value | Done | haiku | 29 DiaCore sub-module docs + 4 siblings |
| 2 | Set `layer: foundation/maths` on DiaMaths, DiaGeometry2D, DiaGeometry3D, DiaGeometryBridge, DiaGeometry2DPicking | grep confirms value | Done | haiku | 9 files; DiaGeometryBridge/DiaGeometry2DPicking have no module.md |
| 3 | Set `layer: foundation/services` on DiaObservation, DiaMetrics, DiaDebugProtocol, DiaWebSocket, DiaAPI, DiaDebugServer, DiaEditor, DiaPython, DiaImGui | grep confirms value | Done | haiku | 6 files; DiaMetrics/DiaAPI/DiaEditor/DiaImGui have no module.md |
| 4 | Set `layer: foundation/platform` on DiaWindow, DiaInput, DiaSDL | grep confirms value | Done | haiku | 4 files (incl. DiaWindow/Interface) |
| 5 | Set `layer: foundation/application` on DiaApplicationFlow, DiaAutomation, DiaGame | grep confirms value | Done | haiku | 2 files; DiaGame has no module.md |
| 6 | Set `layer: assets/core` on DiaEntity, DiaAsset, DiaAssetCatalogue, DiaAssetRuntime, DiaMesh3D | grep confirms value | Done | haiku | 3 files; DiaAsset/DiaMesh3D have no module.md |
| 7 | Set `layer: assets/tools` on DiaAssetCatalogueEditor, DiaEntityInspector, DiaBlueprintEditor, DiaPipelineEditor, DiaApplicationEditor, DiaAssetRuntimeInspector, DiaEntityVisualDebugger, DiaAssetRuntimeVisualDebugger | grep confirms value | Done | haiku | 7 files; DiaPipelineEditor/DiaAssetRuntimeVisualDebugger have no module.md |
| 8 | Set `layer: domain/visual/*` on Visual domain modules | grep confirms value | Done | haiku | 13 files; DiaGraphics3D/DiaBgfx3D/DiaScene3D/DiaVisualDebuggerConsole have no module.md |
| 9 | Set `layer: domain/physics/*` on Physics domain modules | grep confirms value | Done | haiku | 4 files |
| 10 | Set `layer: domain/animation/*` on Animation domain modules | grep confirms value | Done | haiku | 6 files; DiaRig3D/DiaAnimation3D/DiaSkinning3D have no module.md |
| 11 | Verify: grep for module.md files missing `layer:` or with values not in reference table | grep returns empty | Done | haiku | 76 files updated; all valid; dia.root set to foundation/core |

**Commit after Phase 1 complete.**

---

### Phase 2 — Refactoring: DiaCore Splits (sequential — R2 depends on R1)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| R1 | Split DiaFileIO from DiaCore | `msbuild DiaFileIO.vcxproj` + `msbuild DiaCore.vcxproj` both succeed; `dia run googletest` passes | Done | sonnet | Files stay in DiaCore/FilePath/; DiaFileIO compiles them. 5 modules updated with explicit dep. 5858 tests pass. |
| R2 | Split DiaJson from DiaCore | `msbuild DiaJson.vcxproj` + `msbuild DiaCore.vcxproj` both succeed; `dia run googletest` passes | TODO | sonnet | Create DiaJson/ with vcxproj, module.md (`foundation/core`). Move: Json/ directory. DiaJson depends on DiaCore. Modules using JSON add DiaJson ProjectReference. DiaFileIO may depend on DiaJson (verify). |

**Commit after each R-task (R1 then R2).**

---

### Phase 3 — Refactoring: Independent Actions (can run in parallel)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| R3 | Split DiaStreams from DiaApplicationFlow | `msbuild DiaStreams.vcxproj` + `msbuild DiaApplicationFlow.vcxproj` succeed; `dia run googletest` passes | Deferred | sonnet | 50+ game module files need include path changes (scriptable). DiaDebugServer dep now resolved via IStreamTapTarget — R3 is aspirational cleanup. |
| R4 | Move TextureHandler to DiaBgfx | `msbuild DiaAssetRuntime.vcxproj` succeeds without DiaBgfx ref; `dia run googletest` passes | Done | sonnet | TextureHandler in DiaBgfx/Handlers/. DiaAssetRuntime drops DiaBgfx dep. AssetServiceModule.cpp updated. 5858 tests pass. |
| R5 | Invert DiaDebugServer → DiaApplicationFlow dep | `msbuild DiaDebugServer.vcxproj` succeeds without DiaApplicationFlow ref; `dia run googletest` passes | Deferred | sonnet | DebugServer uses LifecycleEvent (concrete type from DiaApplicationFlow core, not just Streams). Needs LifecycleEvent decoupled from Module.h first. Tackle with R3. |
| R6 | Split DiaDebugDraw from DiaVisualDebugger | `msbuild DiaDebugDraw.vcxproj` + all domain VDs build; `dia run googletest` passes | Done | opus | DiaDebugDraw at foundation/services. 9 domain VDs now depend on DiaDebugDraw, not DiaVisualDebugger. Cross-domain exception eliminated. 5890 tests pass. |

**Commit after each R-task. R3/R4/R5 can be parallel. R6 is independent but higher complexity.**

---

### Phase 4 — Architecture Audit Tool (C1)

Tasks 13–16 are independent; task 17 depends on all of them.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 12 | Add `arch` subcommand to `dia check` group in `cli_check.py` — `--module`/`--summary` flags | `dia check arch --help` shows flags | Done | sonnet | Added to cli_check.py group (not check.py — that's a library). `dia_cli/commands/check/` package created. |
| 13 | Implement YAML module map builder (`dia_cli/commands/check/arch_module_map.py`) — reads all `dia.*.architecture.module.md`, extracts module_id, path, layer, dependencies | `dia check arch --module dia.core` returns 0 violations | Done | sonnet | No PyYAML in venv — hand-rolled parser. Handles `module_id:`, `id:`, `module:` key aliases. Fixed missing `---` closer in dia.core.reflect. |
| 14 | Implement include parser (`dia_cli/commands/check/arch_include_parser.py`) — walks .cpp/.h, extracts `#include <...>`, resolves to module_id via longest-prefix path match | | Done | sonnet | Excludes: External/, bgfx/, bare headers (no /), system prefixes |
| 15 | Implement forbidden-dep checker — cross-references includes vs `dependencies.forbidden` | | Done | sonnet | In arch_checker.py; skips layer check if forbidden to avoid double-report |
| 16 | Implement layer ordering checker (`dia_cli/commands/check/arch_layer_rules.py`) — level ordering, sub-level ordering, same-sub-level-different-group, cross-domain | | Done | sonnet | `check_layer_violation(from, to)` → violation string or None |
| 17 | Wire output: violations → `Cluiche/out/check/arch-violations.txt` + console; `--summary` prints count; `--module <id>` scopes to one module; exit 0/1 | `dia check arch --summary` → 1975 violations, exit 1; `dia check arch --module dia.core` → 0, exit 0 | Done | sonnet | Full report always written to file; `--summary` affects console only |
| 18 | Add `dia check --tool=arch` to CI pipeline | Pipeline stage exits 1 on violations | TODO | sonnet | Commit separately after all refactoring merged + clean |

**Commit: task 12 alone, then 13–16 together, then 17, then 18.**

---

### Phase 5 — SLN Layer Sync

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 19 | Implement `dia_cli/commands/check/sln_sync.py` — reads `layer:` from all module docs, rewrites .sln solution folders to numbered names | Before/after .sln diff shows correct folders | TODO | sonnet | Folder names: `1.0-Core`, `1.1-Maths`, `1.1-Services`, `1.2-Platform`, `1.2-Application`, `2.0-Assets`, `2.1-Assets-Tools`, `3.0-Visual`, `3.1-Visual-Tools`, `3.0-Physics`, `3.1-Physics-Tools`, `3.0-Animation`, `3.1-Animation-Tools`. GUID regeneration for new folders. vcxproj files unchanged. |
| 20 | Add `--tool=sln-sync` branch to `dia check`; `--dry-run` mode prints planned changes | `dia check --tool=sln-sync --dry-run` outputs folder assignments | TODO | sonnet | |
| 21 | Add sln-sync post-step to `dia scaffold module` — auto-run after creating new module | `dia scaffold module DiaCore Foo` → project appears under `1.0-Core` in VS | TODO | sonnet | Reads `layer:` from newly written module doc |

**Commit: 19+20 together, then 21.**

---

## Dependency Graph

```
Phase 1 (layer fields)
    │
    ├──→ Phase 2 (R1 → R2, sequential)
    │        │
    │        └──→ Phase 3 (R3, R4, R5, R6 — parallel)
    │                 │
    │                 └──→ Phase 4 (audit tool — needs valid layers + clean deps)
    │                          │
    │                          └──→ Phase 5 (SLN sync — needs audit passing)
    │
    └──→ Phase 4 tasks 12-16 can start in parallel with Phase 2/3
         (they don't need refactoring done, only layer fields)
```

**Critical path:** Phase 1 → Phase 2 (R1→R2) → Phase 3 (R6 is longest) → Phase 4 task 17 (integration) → Phase 5

---

## Estimated Effort

| Phase | Tasks | Effort | Parallelism |
|-------|-------|--------|-------------|
| Phase 1 | 11 | S (1 session) | Full parallel — all independent file edits |
| Phase 2 | 2 | M (1-2 sessions) | Sequential — R2 depends on R1 outcome |
| Phase 3 | 4 | L (2-3 sessions) | R3/R4/R5 parallel; R6 largest single task |
| Phase 4 | 7 | M (1-2 sessions) | 13–16 parallel; 17 sequential after |
| Phase 5 | 3 | S (1 session) | 19+20 parallel; 21 after |

**Total estimate: 6-9 sessions**

---

## Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| R1/R2 (DiaCore splits) create widespread include path changes | High churn, many files touched | Grep all `#include <DiaCore/FilePath/...>` and `#include <DiaCore/Json/...>` before starting to map blast radius |
| R6 (DiaDebugDraw) touches every visual debugger in the codebase (~12 modules) | Broad but mechanical | Each VD just swaps its DiaVisualDebugger dep for DiaDebugDraw — pattern is identical |
| DiaStreams (R3) may have circular dep with DiaObservation | Blocks R3 | Verify before starting: if circular, DiaStreams moves to `foundation/core` (1.0) instead |
| Audit tool (Phase 4) may reveal additional violations beyond R1–R6 | Additional refactoring work | Accept as findings; track as separate tasks, don't block Phase 5 |
| TextureHandler move (R4) requires app-level registration wiring | May affect CluicheTest/CluicheEditor startup | Small: one line in each app's init code to register the handler |
