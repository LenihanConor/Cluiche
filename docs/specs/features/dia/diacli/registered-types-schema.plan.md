**Spec:** @docs/specs/features/dia/diacli/registered-types-schema.md
**Status:** Done

## Implementation Patterns

### Schema dump (C++ side)
- Add `--dump-schema` flag check early in `main()` before window/graphics init
- Walk `Dia::Entity::ComponentRegistry::Get()` and `Dia::ApplicationFlow::TypeRegistry::Global()`
- Write JSON to stdout via `Json::StreamWriterBuilder`, then `exit(0)`
- Touch point: `Cluiche/CluicheTest/main.cpp` (or equivalent entry point)

### Generation script (Python side)
- New command: `Dia/DiaCLI/dia_cli/commands/reflect/` following existing `asset/` pattern
- `group.py` registers `dia reflect` group; `reflect_cmd.py` is the subcommand
- Runs game binary via `subprocess.run([binary_path, "--dump-schema"], capture_output=True)`
- Diffs previous schema (if any) to determine version bump
- Writes file only if content changed (avoids spurious git diffs)

### `.diagame` schema key
- Schema key added to manifest JSON: `"schema": "registeredtypes.diaschema"`
- `dia validate manifest` checks the referenced file exists
- Touch point: `cluichetest.diagame` + manifest validator in DiaCLI

### Blueprint editor integration (C++ side)
- `DiaEntityTemplateEditorPlugin::OnProjectChanged` loads schema from diagame dir
- New `SchemaReader` class in `Dia/DiaEntityTemplateEditor/` — reads `registeredtypes.diaschema`, exposes component list
- `BlueprintPropertyController::BuildAvailableComponentsJson` uses `SchemaReader` instead of `ComponentRegistry::Get()`

### TypeDiscoveryService migration
- `TypeDiscoveryService::LoadFromFile` already reads `modules` + `processing_units` keys — format is compatible
- Change load path from hardcoded `types.json` to schema path derived from `ProjectContext`

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **`--dump-schema` flag in CluicheTest binary.** Add early exit in `main()` that walks `ComponentRegistry` + `TypeRegistry`, writes JSON to stdout, exits 0. | `CluicheTest.exe --dump-schema` outputs valid JSON with `components`, `modules`, `processing_units` arrays | Done | sonnet | Entry point is `main()`. PUs are empty array (TypeRegistry doesn't store PU types — T2 populates from manifest). 5 components, 39 modules on test run. |
| 2 | **`dia reflect` CLI command.** New `Dia/DiaCLI/dia_cli/commands/reflect/` group. `reflect_cmd.py` subcommand: resolve binary path from `pipeline.toml` target, run with `--dump-schema`, parse stdout, write `registeredtypes.diaschema` alongside `.diagame`. | `dia reflect --target cluichetest` writes file; second run with no changes doesn't rewrite (version unchanged) | Done | sonnet | 5 files: cli/reflect.py, commands/reflect/{__init__,group,reflect_cmd,reflect_handler}.py. Binary resolved via path_resolver.resolve_out_dir. |
| 3 | **Versioning logic.** Script diffs previous schema vs new dump. Minor bump if type set changed (added fields/types). Major bump only with `--breaking` flag. No write if identical. | Run with new component → minor bumps. Run again unchanged → file untouched. `--breaking` → major bumps. | Done | sonnet | Implemented in reflect_handler._compute_version(). Diffs type_id sets + fields-per-type. Warns on removal even without --breaking. |
| 4 | **`.diagame` schema reference.** Add `"schema": "registeredtypes.diaschema"` to `cluichetest.diagame`. Update `dia validate manifest` to check referenced schema file exists when key present. | `dia validate manifest` passes with schema present; warns when schema key present but file missing | Done | haiku | WARN (not ERR) on missing schema; exit 0 always. Validator rewritten as cli_validate.py. |
| 5 | **`SchemaReader` in DiaEntityTemplateEditor.** New `Dia/DiaEntityTemplateEditor/SchemaReader.h/cpp`. Reads `registeredtypes.diaschema`, exposes `GetComponents()` returning array of `{typeId, debugName, fields[]}`. Loaded in `DiaEntityTemplateEditorPlugin::OnProjectChanged` from diagame dir. | Unit test: load valid schema → correct component list; missing file → empty list (no crash) | Done | sonnet | SchemaReader.h/cpp created. LoadFromFile silently no-ops on missing/malformed. Major version change triggers DIA_LOG_WARNING. vcxproj updated. |
| 6 | **Blueprint editor uses `SchemaReader` for component dropdown.** Replace `ComponentRegistry::Get()` call in `BlueprintPropertyController::BuildAvailableComponentsJson` with `SchemaReader` data. Pass `SchemaReader` ref into `BuildAvailableComponentsJson`. Status message when schema absent. | Run editor → load `.diagame` → Add Component dropdown shows `TransformComponent`, `VisualTestRenderComponent`, `PickableCircleComponent` | Done | sonnet | BuildAvailableComponentsJson now takes const SchemaReader& schema. Returns statusMessage entry when schema absent. OnProjectChanged derives path and loads. |
| 7 | **`TypeDiscoveryService` uses schema path.** Change load path from hardcoded `types.json` to `registeredtypes.diaschema` derived from `ProjectContext.diagamePath`. Remove old `types.json` reference. | DiaApplicationFlowEditor module dropdown still populated after path change | Done | haiku | HandleTypesRefresh now derives path from mModel->GetDiagameProject().diagamePath. LoadFromRegistry() fallback preserved. |
| 8 | **GoogleTests.** `SchemaReader`: load valid schema, missing file, malformed JSON, version mismatch warning. `BlueprintPropertyController`: `BuildAvailableComponentsJson` with schema data. | `dia run googletest --filter="SchemaReader*"` all pass | Done | sonnet | 14 SchemaReader tests + 3 new BlueprintPropertyController tests. Fixed old 2-param calls to use 3-param signature. 17/17 pass. |

## Implementation Order

```
T1 (binary flag) → T2 (CLI command) → T3 (versioning) → T4 (diagame ref)
T5 (SchemaReader) → T6 (blueprint editor) → T7 (TypeDiscoveryService)
T8 (tests) after T5+T6
```

T1-T4 are the generation pipeline (sequential). T5-T7 are the consumption side (T5 before T6, T7 independent). T8 after T5+T6.

## Verification

- `dia reflect --target cluichetest` generates `Cluiche/Assets/CluicheTest/registeredtypes.diaschema`
- Open CluicheEditor → load `cluichetest.diagame` → Blueprint Editor → select `blargh.diaentitytemplate` → "Add Component" dropdown shows all 3 CluicheTest components
- DiaApplicationFlowEditor module dropdown still works
- `dia validate manifest` passes
