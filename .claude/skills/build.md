---
name: build
description: Build Cluiche/Dia projects with dependency management
tags: [build, msbuild, cpp, dia]
user_invocable: true
agent_invocable: true
---

# Cluiche Build Skill

Build individual projects or the entire Cluiche solution with proper dependency handling.

## Usage

```
/build [target] [options]
```

## Targets

- **No target**: Build entire solution (Cluiche.sln)
- **googletest**: GoogleTests C++ test suite
- **cluichetest**: Demo game and engine testbed
- **cluicheeditor**: Editor application
- **DiaCore**: Foundation library (must build first, no dependencies)
- **DiaMaths**: Math library (depends on DiaCore)
- **DiaGraphics**: Graphics layer (depends on DiaCore, DiaMaths, DiaUI)
- **DiaApplicationFlow**: Application framework
- **DiaWindow**: Window management (depends on DiaGraphics)
- **DiaInput**: Input handling
- **DiaUI**: Base UI system (depends on DiaCore, DiaInput)
- **DiaSFML**: SFML integration (depends on DiaCore, DiaMaths, DiaGraphics, DiaWindow)
- **DiaAPI**: Plugin-based CLI (depends on DiaCore)

## Options

- `--config=<Debug|Release>`: Build configuration (default: Debug)
- `--platform=<x64>`: Target platform (default: x64)
- `--clean`: Clean before building
- `--rebuild`: Force full rebuild (clean + build)
- `--verbose`: Show detailed MSBuild output
- `--deps`: Also build dependencies (auto-enabled for targets with deps)

## Examples

```bash
# Build and run GoogleTests
/build googletest

# Build CluicheTest
/build cluichetest

# Build in Release mode
/build --config=Release

# Build DiaCore only (raw vcxproj)
/build DiaCore

# Full solution rebuild
/build --rebuild
```

## Instructions for Claude

When this skill is invoked:

### 1. Parse Arguments

Extract target project and options. Normalize project names (case-insensitive).

### 2. Choose Build Strategy

**For pipeline targets (googletest, cluichetest, cluicheeditor):**

Use DiaCLI — this handles MSBuild discovery, dependency building, and deploy:
```bash
cd Dia/DiaCLI && DIA_CLI_CONFIG="C:/GitHub/Cluiche/Dia/DiaCLI/dia_cli_prime_config.json" .venv/Scripts/python.exe -m dia_cli run <target> --build-only --config <config>
```

Or if `dia` is on PATH:
```bash
dia run <target> --build-only --config <config>
```

With `--force` to force rebuild:
```bash
dia run <target> --build-only --force --config <config>
```

**For individual modules or full solution (not a pipeline target):**

Fall back to MSBuild:
```bash
msbuild <path>.vcxproj /p:Configuration=<config> /p:Platform=x64 /nologo /v:minimal
```

Or for full solution:
```bash
msbuild Cluiche/Cluiche.sln /p:Configuration=<config> /p:Platform=x64 /nologo /v:minimal
```

### 3. Project Paths (for raw MSBuild fallback)

- Dia modules: `Dia/<ModuleName>/<ModuleName>.vcxproj`
- CluicheTest: `Cluiche/CluicheTest/CluicheTest.vcxproj`
- GoogleTests: `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj`
- CluicheEditor: `Cluiche/CluicheEditor/CluicheEditor.vcxproj`

### 4. Parse Build Output

- **Success**: Report time taken, show binary output path
- **Failure**: Extract and highlight actual error:
  - Compilation errors (file:line:column)
  - Linker errors (unresolved symbols, missing libs)
  - Missing include paths

### 5. Output Format

**Success:**
```
Built googletest (Debug|x64) in 4.5s
   Output: Cluiche/bin/GoogleTests/Debug/x64/GoogleTests.exe
```

**Failure:**
```
Build failed: DiaCore

Error in Containers/Arrays/DynamicArray.cpp:45:10
  'size_t' was not declared in this scope

Suggestion: Add #include <cstddef> or check for missing namespace
```

### Error Handling

1. **MSBuild not found**: DiaCLI handles this via vswhere.exe. If using raw MSBuild, suggest `dia run` instead.
2. **Dependency failed**: Show which dependency failed and why
3. **Linker errors**: Likely means dependency wasn't built or is out of date — suggest `--force`

### DiaCLI Commands Reference

```bash
# Pipeline targets (compile + deploy):
dia run googletest --build-only        # Build GoogleTests
dia run cluichetest --build-only       # Build CluicheTest
dia run cluicheeditor --build-only     # Build CluicheEditor

# Full pipeline + launch:
dia run googletest                     # Build + run tests
dia run cluichetest                    # Build + launch game
dia run cluicheeditor                  # Build + launch editor

# Just launch (already built):
dia launch googletest
dia launch cluichetest
dia launch cluicheeditor

# Pipeline with stage control:
dia pipeline --target googletest --stage compile-code
dia pipeline --target cluichetest --config Release
```

### Smart Defaults

- Default to Debug|x64 (primary development config)
- Use DiaCLI for pipeline targets — it handles MSBuild discovery automatically
- Show relative paths in errors (from repo root)
- For build errors, check if DiaCore or other dependencies need rebuilding
