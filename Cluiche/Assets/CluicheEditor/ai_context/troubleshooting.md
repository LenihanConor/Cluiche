# Troubleshooting & DiaCLI Reference

## DiaCLI Command Reference

DiaCLI is the primary interface for building, running, and managing Cluiche projects. **Never call executables directly** — always use `dia run` or `dia launch`.

### Core Commands

| Command | Description |
|---------|-------------|
| `dia run <target>` | Build + run in one step (tests, game, editor) |
| `dia launch <target>` | Run only — skip build (use when already built) |
| `dia pipeline --target <target>` | Full pipeline: compile → assets → deploy |
| `dia test googletest` | Run C++ unit test suite |
| `dia test editor-ui` | Run editor UI Vitest suite |
| `dia test cli` | Run DiaCLI pytest suite |

### Scaffold Commands

| Command | Description |
|---------|-------------|
| `dia scaffold module <Parent> <Name>` | Create new Dia engine module |
| `dia scaffold plugin <Name>` | Create new editor plugin |
| `dia scaffold stage <Name>` | Create new CluicheTest test stage |

### Validation & Checks

| Command | Description |
|---------|-------------|
| `dia check deps` | Cross-check module dependencies vs #include usage |
| `dia check arch` | Audit module layer dependencies |
| `dia check sln-sync` | Rewrite solution folders |
| `dia validate manifest` | Validate .diaapp/.diagame/.diastage files |
| `dia validate manifest --path <file>` | Validate specific manifest |

### Documentation Commands

| Command | Description |
|---------|-------------|
| `dia docs plan <path> <#> --status <S>` | Update plan task row |
| `dia docs registry` | Regenerate module-registry.md |
| `dia docs spec-done <spec.md>` | Mark spec Done + update plan |
| `dia docs vcxproj-add <Project> <File>` | Add file to vcxproj + filters |
| `dia docs backlog move <name>` | Move backlog entry to history |
| `dia docs test-scaffold <header.h>` | Generate test file from header |
| `dia docs precommit` | Run pre-commit checks |

### Environment Commands

| Command | Description |
|---------|-------------|
| `dia env setup` | Provision fresh developer machine |
| `dia env verify` | Check environment health (read-only) |
| `dia env deps` | Restore binary SDK dependencies |

### Build Targets

| Target | Output |
|--------|--------|
| `googletest` | `Cluiche/bin/GoogleTests/{config}/x64/GoogleTests.exe` |
| `cluichetest` | `Cluiche/bin/CluicheTest/{config}/x64/CluicheTest.exe` |
| `cluicheeditor` | `Cluiche/bin/CluicheEditor/{config}/x64/CluicheEditor.exe` |

### Build Configurations

| Config | Use |
|--------|-----|
| `Debug` | Primary development (default) |
| `Release` | Optimized release |
| `Debug-Asan` | Address sanitizer enabled |
| `Debug-Ubsan` | Undefined behavior sanitizer |

### Global Options

```
--no-color      Disable ANSI colour output
--quiet         Suppress terminal output; still writes JSON log
--log-json PATH Override NDJSON log file path
```

### Filtering Tests

```bash
dia run googletest --filter="FixedDrawLayer*"
dia run googletest --filter="SomeSuite.SomeTest"
dia launch googletest --filter="Entity*"
```

---

## Common Failure Modes

### Component Registration Failures

**Symptom:** Entity instantiation crashes or component not found at runtime.

**Causes:**
- Missing `DIA_COMPONENT_REGISTER()` macro in the .cpp file
- Component type string doesn't match what's referenced in templates
- .cpp file not added to vcxproj (so it never compiles and the static registrar never runs)

**Fix:**
1. Verify the component has `DIA_COMPONENT()` in its header and `DIA_COMPONENT_REGISTER()` in its .cpp
2. Check the type string matches exactly (e.g., `"render.sprite"` not `"render_sprite"`)
3. Run `dia docs vcxproj-add <Project> <file>` if the .cpp is missing from the build

### Asset Not Found at Runtime

**Symptom:** `"asset not found"` or `"Failed to load asset"` in logs.

**Causes:**
- Asset not deployed (pipeline not run after adding new assets)
- Path mismatch between manifest reference and actual file location
- Asset handler not registered for the file type

**Fix:**
1. Run `dia pipeline --target <target>` to rebuild and deploy assets
2. Check the manifest references use correct relative paths from app root
3. Verify the asset type has a registered handler (texture, sprite, audio, config, entity, stage, ui, folder)

### Phase Transition Deadlocks

**Symptom:** Application hangs, no progress after a phase completes.

**Causes:**
- `TransitionPhase()` called from wrong thread (use `QueuePhaseTransition()` for thread-safe transitions)
- Missing transition in phase state machine definition
- Circular phase dependencies

**Fix:**
1. Use `QueuePhaseTransition()` when crossing thread boundaries
2. Check phase state machine defines a valid path from current to target phase
3. Review module dependencies for cycles (use `dia check deps`)

### Include Path / Compilation Errors

**Symptom:** `error C2065` (undeclared identifier), `error C1083` (cannot open include file).

**Causes:**
- Include path not configured in vcxproj AdditionalIncludeDirectories
- Wrong include style (should use `<DiaCore/Module/Header.h>` not `"relative/path.h"`)
- Circular dependency between modules

**Fix:**
1. Include from module root: `#include <DiaCore/Containers/Arrays/Array.h>`
2. Check vcxproj has the correct `AdditionalIncludeDirectories` entry
3. Run `dia check deps` to detect circular dependencies
4. Run `dia check arch` to verify layer violations

### Linker Errors (LNK2019 / LNK2001)

**Symptom:** `error LNK2019: unresolved external symbol`.

**Causes:**
- Implementation .cpp not added to vcxproj
- Missing library reference in vcxproj
- Template method defined in .cpp instead of header

**Fix:**
1. Run `dia docs vcxproj-add <Project> <file.cpp>` to add missing source
2. Check project references in vcxproj for missing dependencies
3. For template classes, ensure implementation is in the header

### Manifest Validation Failures

**Symptom:** `dia validate manifest` reports errors.

**Common errors:**
- `"missing required key: {key}"` — required field not present
- `"{key} should be {type}"` — wrong data type for field
- `"invalid JSON"` — malformed YAML/JSON syntax

**Fix:**
1. Check manifest against schema (version field, required keys)
2. Ensure all referenced stages/entities exist as files
3. Use `dia validate manifest --path <file>` for targeted validation

### Build Pipeline Failures

**Symptom:** `dia pipeline` or `dia run` fails with stage errors.

**Pipeline events to watch:**
- `OnAssetFailed` — validation, transform, or deploy stage failed for an asset
- `OnStageFailed` — a pipeline stage (compile/link/asset/deploy) failed
- `OnRunFailed` — the overall run failed

**Fix:**
1. Check the specific error message — usually points to the failing file
2. For compile errors: fix C++ source, check includes
3. For asset errors: verify source file exists and passes validation
4. For deploy errors: check output directory permissions

### Environment Issues

**Symptom:** Tools missing or commands fail before building.

**Common messages:**
- `"not found in PATH"` — tool not installed or PATH not set
- `"Node.js not found"` — needed for UI builds
- `"deps.json not found"` — dependencies not configured

**Fix:**
1. Run `dia env verify` to check environment health
2. Run `dia env setup` to provision missing tools
3. Run `dia env deps` to restore binary SDK dependencies

---

## Diagnostic Commands

Quick checks when something goes wrong:

```bash
# Is my environment healthy?
dia env verify

# Are my module deps valid?
dia check deps --verbose

# Is my manifest well-formed?
dia validate manifest

# Rebuild everything from scratch
dia pipeline --target <target> --config Debug

# Run specific failing test in isolation
dia run googletest --filter="FailingSuite.FailingTest"
```
