# Feature Spec: dev-source-alias

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diacli/diacli.md | **dev-source-alias** |

**Origin:** CoW F-3 World Assets (@docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md) needs testbeds to write authored content into the source tree. Logged as Dia need #16 in `docs/specs/applications/cow/dia-needs.md`.  
**Status:** `Approved`

---

## Problem Statement

Testbed `save_as` commands write authored assets to the source tree (`Cluiche/Assets/<App>/Global/`) so they become source-controlled game content. At line 173 of `f3-worldassets.md`, F-3 expects a `source` path alias pointing at the app's source assets root. No mechanism exists today to inject dev-only aliases that are absent in packaged builds.

Path aliases are registered from `.diagame`/`.diastage` `path_aliases` config at lines 47-73 of `Dia/DiaCLI/dia_cli/templates/app/Main.cpp.j2` (`RegisterPathAliases`). Every alias in the manifest is baked into deployed builds. CoW needs `source` present only when launched by DiaCLI, not in shipped packages.

---

## Solution Overview

`dia launch` and `dia run` pass `--dev-alias source=<path>` to the game executable. The generated `Main.cpp` parses `--dev-alias` arguments before loading the manifest and registers them to `PathStore` before manifest aliases. These aliases exist only when the CLI launches the game; packaged executables launched directly never see them.

### Injection mechanism

Command-line arguments parsed by `Main.cpp`. Chosen over environment variables (less explicit, harder to test, platform-dependent) and generated overlay files (adds a deployment artifact, race conditions if multiple launches). Arguments are visible in process listings and debugger attach, which aids troubleshooting.

The argument format is `--dev-alias <name>=<path>`, repeatable. Paths are absolute (the CLI resolves them before passing). The runtime registers them before the manifest loads. **`source` is a reserved alias name** (amended 2026-10-20, approval): only `--dev-alias` may set it. `dia validate manifest` rejects any `.diagame` or `.diastage` that declares `source` in `path_aliases`, and the runtime ignores a manifest `source` with an Error log. This way `source` can never exist in a packaged build. For any other dev alias added later, the manifest wins.

---

## Design

### CLI changes

**File:** `Dia/DiaCLI/dia_cli/cli/launch.py`

New function `_resolve_dev_aliases(target: str, repo_root: Path) -> list[str]`:
- If `target` has `app_name` in its `pipeline.toml` entry, compute `source` alias: `repo_root / "Cluiche" / "Assets" / app_name`.
- Return `["--dev-alias", f"source={source_path}"]` if the directory exists, else `[]`.
- googletest and other non-app targets return `[]`.

Function `launch_target` line 94: insert dev alias args after `cmd = [str(exe_path)]` and before any target-specific arguments.

### Runtime changes

**File:** `Dia/DiaCLI/dia_cli/templates/app/Main.cpp.j2`

New function before `RegisterPathAliases`:

```cpp
// Parse and register --dev-alias arguments (dev launches only).
// Format: --dev-alias <name>=<path> (repeatable, absolute paths)
void RegisterDevAliases(int argc, const char* argv[])
{
    for (int i = 1; i < argc - 1; ++i)
    {
        if (strcmp(argv[i], "--dev-alias") == 0 && i + 1 < argc)
        {
            const char* pair = argv[++i];
            const char* equals = strchr(pair, '=');
            if (!equals) continue;
            
            Dia::Core::Containers::String64 name;
            unsigned int nameLen = static_cast<unsigned int>(equals - pair);
            if (nameLen >= name.Capacity()) continue;
            for (unsigned int j = 0; j < nameLen; ++j)
                name[j] = pair[j];
            name[nameLen] = '\0';
            
            Dia::Core::Path::Alias alias(name.AsCStr());
            Dia::Core::Path::String pathStr(equals + 1);
            Dia::Core::PathStore::RegisterToStore(alias, pathStr);
        }
    }
}
```

Call it at line 79 after the `kDiagamePath` declaration: `RegisterDevAliases(argc, argv);`.

**Existing `Main.cpp` files:** `Cluiche/CluicheTest/Main.cpp`, `Cluiche/CoW/Main.cpp` get the same changes (not via template; direct edits).

### Validation

No `dia validate manifest` changes. Dev aliases are invisible to the manifest validator (they are not in the file). If a module resolves `source` and it is absent, `PathStore::ResolvePathToCString` returns an empty string, which the module checks.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `dia launch cluichetest` passes `--dev-alias source=C:\GitHub\Cluiche\Cluiche\Assets\CluicheTest` to the executable | Add a debug log in `RegisterDevAliases` and verify output |
| AC2 | `source` alias resolves to the correct path inside a launched CluicheTest module | pytest: call `PathStore::ResolvePathToCString("source")` via Python bindings after launch |
| AC3 | `dia launch googletest` passes no dev alias arguments (target has no `app_name`) | Manual: check process args |
| AC4 | Launching the executable directly (not via DiaCLI) does not register `source` | Manual: run `CluicheTest.exe` from its output directory; check alias is absent |
| AC5 | `source` is reserved: `dia validate manifest` fails on a manifest declaring `source`; at runtime a manifest `source` is ignored with an Error and the CLI value stays | pytest: validator rejects `.diagame` with `"source"`; GoogleTest: manifest `source` does not override the `--dev-alias` value |
| AC6 | Multiple `--dev-alias` arguments are all registered | pytest: pass two aliases, verify both resolve |

---

## Test Strategy

**pytest** (`Dia/DiaCLI/tests/commands/test_launch_dev_aliases.py`):
- Mock `launch_target` calls and verify `--dev-alias` in the command for `cluichetest` and `cow`.
- Verify no `--dev-alias` for `googletest`.
- Test that `source` is reserved (validator rejection and runtime ignore) with a temporary `.diagame`.

**Manual:**
- Generate and launch `ScratchApp` with `dia scaffold app` (uses the template).
- Verify `source` resolves inside a module via a DiaAPI `path.resolve source` command.

---

## Binding Decisions

| Source | Constraint |
|--------|------------|
| PD-009 | Authored files are source assets in `Assets/`, not generated output in `out/`. `source` points at the assets root. |
| AD-001 CoW sign-off | This change (Dia-tier, signed off 2026-10-19) enables CoW F-3 `save_as` commands. |
| F-3 Resolved Decision 1 | The `source` alias is dev-only and absent in packaged builds. |

## Resolved Decisions (approved 2026-10-20)

1. **Command-line arguments, not environment variables.** They are visible in process listings and easy to test. Packaged builds are never launched through DiaCLI.
2. **`source` is reserved.** No manifest may declare it; validate rejects it, and the runtime ignores it with an Error. For other dev aliases the manifest wins. This replaces the draft's "manifest wins for everything", which would have let a manifest put `source` into a packaged build.
3. **Absolute paths only in v1.** Relative dev aliases wait for a real need.

---

## Status

`Approved` (2026-10-20). No plan yet. Build sign-off (AD-001) given by the user 2026-10-20; not started (implementation deferred by the user). dia-needs #16 signed off under CoW AD-001 on 2026-10-19.
