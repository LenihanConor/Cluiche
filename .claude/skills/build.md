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
- **DiaCore**: Foundation library (must build first, no dependencies)
- **DiaMaths**: Math library (depends on DiaCore)
- **DiaGraphics**: Graphics layer (depends on DiaCore, DiaMaths, DiaUI)
- **DiaApplication**: Application framework
- **DiaWindow**: Window management (depends on DiaGraphics)
- **DiaInput**: Input handling
- **DiaUI**: Base UI system (depends on DiaCore, DiaInput)
- **DiaUIAwesomium**: Awesomium UI layer (depends on DiaUI)
- **DiaSFML**: SFML integration (depends on DiaCore, DiaMaths, DiaGraphics, DiaWindow)
- **DiaPython**: Python bindings (depends on DiaCore)
- **DiaAPI**: Plugin-based CLI (depends on DiaCore)
- **CluicheKernel**: Core game kernel
- **CluicheTest**: Main executable (depends on most Dia modules)
- **GoogleTests**: Test suite (depends on DiaCore, DiaMaths, DiaGraphics)

## Options

- `--config=<Debug|Release>`: Build configuration (default: Debug)
- `--platform=<x64>`: Target platform (default: x64)
- `--clean`: Clean before building
- `--rebuild`: Force full rebuild (clean + build)
- `--verbose`: Show detailed MSBuild output
- `--deps`: Also build dependencies (auto-enabled for targets with deps)

## Examples

```bash
# Build entire solution
/build

# Build DiaCore only
/build DiaCore

# Rebuild CluicheTest (will rebuild dependencies if needed)
/build CluicheTest --rebuild

# Build in Release mode
/build --config=Release

# Clean build of GoogleTests
/build GoogleTests --clean

# Build with verbose output to diagnose issues
/build DiaGraphics --verbose
```

## Instructions for Claude

When this skill is invoked:

### 1. Parse Arguments

Extract target project and options. Normalize project names (case-insensitive).

### 2. Determine Dependency Chain

**Foundation (no dependencies):**
- DiaCore
- DiaApplication
- CluicheKernel

**Single-level dependencies:**
- DiaMaths → DiaCore
- DiaPython → DiaCore
- DiaAPI → DiaCore
- DiaInput → (none for library, but conceptually part of UI stack)

**Multi-level dependencies:**
- DiaUI → DiaCore, DiaInput
- DiaGraphics → DiaCore, DiaMaths, DiaUI
- DiaUIAwesomium → DiaUI
- DiaWindow → DiaGraphics
- DiaSFML → DiaCore, DiaMaths, DiaGraphics, DiaWindow
- CluicheTest → Most modules (DiaCore, DiaMaths, DiaGraphics, DiaWindow, DiaSFML, DiaInput, DiaUI, DiaUIAwesomium, DiaApplication, CluicheKernel)
- GoogleTests → DiaCore, DiaMaths, DiaGraphics

### 3. Build Strategy

**If building entire solution:**
```bash
msbuild Cluiche/Cluiche.sln /p:Configuration=<config> /p:Platform=<platform> /nologo /v:minimal
```

**If building specific project:**

a) **Determine project path:**
   - Dia modules: `Dia/<ModuleName>/<ModuleName>.vcxproj`
   - CluicheTest: `Cluiche/CluicheTest/CluicheTest.vcxproj`
   - GoogleTests: `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj`
   - CluicheKernel: `Cluiche/Cluiche/CluicheKernel/CluicheKernel.vcxproj`

b) **If --deps flag or target has dependencies:**
   - Build dependencies first in correct order
   - Stop if any dependency fails
   - Show which dependency failed

c) **Handle clean/rebuild:**
   - `--clean`: `/t:Clean;Build`
   - `--rebuild`: `/t:Rebuild`
   - Normal: `/t:Build`

d) **Build the project:**
```bash
msbuild <path>.vcxproj /t:<target> /p:Configuration=<config> /p:Platform=<platform> /nologo /v:minimal
```

### 4. Parse Build Output

- **Success**: Report time taken, show binary output path
- **Failure**: Extract and highlight actual error:
  - Compilation errors (file:line:column)
  - Linker errors (unresolved symbols, missing libs)
  - Missing include paths
  - Missing dependencies

### 5. Output Format

**Success:**
```
✅ Built DiaCore (Debug|x64) in 3.2s
   Output: Cluiche/bin/lib/Debug/DiaCore.lib
```

**Success with dependencies:**
```
Building DiaGraphics and dependencies...
  ✅ DiaCore (1.2s)
  ✅ DiaMaths (0.8s)
  ✅ DiaUI (1.5s)
  ✅ DiaGraphics (2.1s)

Total build time: 5.6s
```

**Failure:**
```
❌ Build failed: DiaCore

Error in Containers/Arrays/DynamicArray.cpp:45:10
  'size_t' was not declared in this scope
  
Suggestion: Add #include <cstddef> or check for missing namespace

Build command:
  msbuild Dia/DiaCore/DiaCore.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### Error Handling

**Common issues:**

1. **MSBuild not found**
   - Check if running in Developer Command Prompt
   - Suggest: `start Cluiche/Cluiche.sln` to build in Visual Studio

2. **Dependency failed**
   - Show which dependency failed and why
   - Suggest rebuilding dependencies: `/build <dep> --rebuild`

3. **Missing include paths**
   - Check .vcxproj configuration
   - Suggest verifying External/ dependencies (SFML, Awesomium)

4. **Linker errors (unresolved externals)**
   - Likely means dependency wasn't built or is out of date
   - Suggest: `/build <project> --deps --rebuild`

5. **Out of date dependencies**
   - If incremental build fails mysteriously, suggest `--rebuild`
   - DiaCore changes often require rebuilding dependents

### Smart Defaults

- Use `/nologo /v:minimal` for clean output (unless --verbose)
- Default to Debug|x64 (primary development config)
- Auto-enable --deps when building projects with dependencies
- Show relative paths in errors (from repo root)
- Estimate build time based on project size

### Build Optimization Tips

**When to rebuild:**
- Header changes in DiaCore → rebuild all dependents
- New source files added → rebuild that project only
- Linker errors → rebuild with --deps
- Strange behavior → full solution rebuild

**Incremental workflow:**
- Working on single module → `/build <module>`
- Testing changes → `/build <module> --no-deps` (skip deps if unchanged)
- Before commit → `/build` (full solution)
- After merge → `/build --rebuild` (clean slate)

### Verbosity Levels

**Minimal (default):**
- Only show project name, result, time
- Hide compiler/linker command lines
- Show errors only

**Verbose (--verbose):**
- Show full MSBuild output with `/v:normal`
- Useful for diagnosing include path issues
- Show all warnings

### Special Cases

**DiaCLI (Python-only):**
DiaCLI is a Python project (not a C++ vcxproj), so it doesn't need building via MSBuild. If user tries to build it, explain:
```
ℹ️  DiaCLI is a Python project (Poetry-based)
   No MSBuild compilation needed
   To set up: cd Dia/DiaCLI && poetry install
```

**External dependencies:**
If build fails with missing external headers/libs:
- Check `External/SFML/` exists
- Check `External/Awesomium/` exists
- Suggest user verify External/ directory setup

### Build Time Estimates

Provide rough estimates to set expectations:
- DiaCore: ~2-4s incremental, ~10-15s clean
- DiaMaths: ~1-2s incremental, ~3-5s clean
- DiaGraphics: ~2-3s incremental, ~8-10s clean
- CluicheTest: ~5-8s incremental, ~20-30s clean
- Full solution: ~30-60s clean build

These vary by machine but give users an idea of whether build is hung.
