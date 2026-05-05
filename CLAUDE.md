# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Cluiche** (Irish for "game") is a game development platform built around the **Dia engine**. The platform supports multiple applications including games, tools, and test suites, all built on top of the Dia engine framework.

### Platform Architecture

- **Platform: Cluiche** - The overall game development platform
- **Application: Dia** - The game engine (DiaCore, DiaMaths, DiaGraphics, DiaAPI, etc.) - organized as an "application" for spec purposes but serves as shared engine code
- **Application: CluicheTest** - Demo game and engine testbed that showcases Dia capabilities
- **Application: GoogleTest** - Unit testing suite
- **Future Applications** - Your actual game projects built on Dia

The Dia engine follows a component-based architecture with distinct separation of concerns across subsystems.

## Build System

This is a Visual Studio C++ project using MSBuild.

### DiaCLI Commands (mandatory)

**NEVER call executables directly** (e.g., `GoogleTests.exe`, `CluicheTest.exe`). Always use `dia run` or `dia launch`. The CLI handles path resolution, working directories, and runtime dependencies. If the CLI fails, fix the CLI — do not bypass it.

```bash
# Build + deploy + run GoogleTests
dia run googletest
dia run googletest --filter="FixedDrawLayer*"
dia run googletest --config Release

# Build + deploy + launch CluicheTest game
dia run cluichetest

# Build + deploy + launch CluicheEditor
dia run cluicheeditor

# Just launch (skip build — exe must already exist)
dia launch googletest --filter="SomeSuite*"
dia launch cluichetest

# Build pipeline only (no launch)
dia pipeline --target googletest
dia pipeline --target cluichetest --config Release

# Other dia commands
dia env setup          # Set up environment (toolchain, deps, PATH, DIA_CLI_CONFIG)
dia env verify         # Check environment health
dia test cli           # Run DiaCLI pytest suite
```

### Raw MSBuild (fallback)

```bash
# Open solution in Visual Studio
start Cluiche/Cluiche.sln

# Build via MSBuild (from repository root)
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
msbuild Cluiche/Cluiche.sln /p:Configuration=Release /p:Platform=x64

# Build specific project
msbuild Dia/DiaCore/DiaCore.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### Configurations

- `Debug|x64` - Primary development configuration
- `Release|x64` - Optimized release build

## Architecture Overview

### Modular Structure

The engine is organized into **Dia** modules, each documented with a `dia.*.architecture.module.md` file containing YAML frontmatter. These files define:
- Module ID and dependencies
- Public API (headers, namespaces, entry points)
- Responsibilities and non-responsibilities
- Parent/child module relationships

### Key Modules

Module docs live alongside their code as `dia.*.architecture.module.md` files — read these directly when you need module-specific detail.

### Processing Unit Architecture

Applications are structured as a hierarchy:
```
ProcessingUnit (e.g., MainProcessingUnit)
  ├─ Module (e.g., RenderModule, PhysicsModule)
  └─ Phase (e.g., InitPhase, UpdatePhase, ShutdownPhase)
       └─ Phase transitions define application flow
```

Processing units can be multi-threaded. Phases transition based on defined state machines. Modules provide functionality that phases consume.

### String IDs and CRC

The codebase extensively uses `StringCRC` for efficient string comparison via compile-time CRC hashing. When you see patterns like `kUniqueId` or `Module::kUniqueId`, these are typically `StringCRC` constants.

## External Dependencies

Located in `External/`:
- **SFML** - Graphics, window, and multimedia library
- **jsoncpp-master** - JSON parsing (used via `DiaCore/Json/`)
- **Webix** & **VisJS** - Web UI frameworks for debugging/visualization

## Project Structure

See filesystem directly or [docs/reference/registry/module-registry.md](docs/reference/registry/module-registry.md) for structure.

## Documentation

The project uses a dual documentation structure:

- **`docs/specs/`** - Spec-driven development workflow for planning and building new features
  - 4-level hierarchy: **Platform → Application → System → Feature**
  - Each spec has decision tracking, AI review questions, and traceability
  - Custom slash commands: `/spec-platform`, `/spec-app`, `/spec-system`, `/spec-feature`, `/spec-review`, `/spec-trace`

- **`docs/reference/`** - Reference documentation for understanding the existing codebase
  - [Architecture](docs/reference/architecture/) - System architecture and design
  - [API Documentation](docs/reference/api/) - Public interfaces
  - [AI Guides](docs/reference/ai-guides/) - AI-optimized navigation
  - See [docs/README.md](docs/README.md) for full navigation

**Steering docs** (loaded for spec workflow):
- Tech standards: `.claude/steering/tech.md`
- Codebase structure: `.claude/steering/structure.md`

### Spec Workflow

#### Creating Specs (`/spec-feature`, `/spec-system`, `/spec-app`)

**MANDATORY — all 5 steps must be completed before a spec can be marked `Approved`:**

1. **Step 1 — Interview** - Complete all interview questions with the user before writing anything
2. **Step 2 — Draft** - Write the full spec body (summary, goals, tasks, traceability)
3. **Step 3 — Binding Decisions** - Populate the compliance table showing how this spec honors every binding decision from its parent specs (Application → Platform). This step is NEVER optional.
4. **Step 4 — AI Review Questions** - Generate and answer all AI review questions covering risks, gaps, and edge cases. This step is NEVER optional.
5. **Step 5 — Approval gate** - Only mark `Approved` after Steps 3 and 4 are complete and confirmed by the user.

**Hard rules:**
- NEVER mark a spec `Approved` without completing Steps 3 and 4.
- NEVER skip or abbreviate Steps 3 or 4 for speed or convenience — if the user says "quickly" or "efficiently", treat it as a red flag and do the full steps anyway.
- A **system spec** cannot be marked `Done` until ALL its child feature specs are `Approved`.
- After completing any spec, explicitly ask: "Steps 3 (Binding Decisions) and 4 (AI Review Questions) are complete — shall I mark this Approved?"
- If the spec originated from a research session, add a `**Research:**` line to the spec header pointing to `docs/research/<slug>/summary.md`. Ask the user if one exists before finalising the draft.

#### Implementing from Specs

When implementing new features using the spec-driven approach:

1. **Spec must exist and be `Approved`** before implementation starts
2. **Create a plan** - Before writing any code, create a `*.plan.md` alongside the spec (see Plan Workflow below)
3. **Read the full spec chain** - Every feature spec has a Traceability table linking back to System → Application → Platform
4. **Check binding decisions** - Platform and Application binding decisions must be honored by all child specs
5. **Implement with spec reference**: Point agents at the feature spec file path and its plan
6. **Delegate tasks to subagents** - Each task in the plan should be a separate subagent
7. **Update the plan** after each task (mark Done/Blocked, add notes)
8. **Commit after each task** before continuing
9. **Update feature spec status** as work progresses (Draft → Approved → In Progress → Done)

### Plan Workflow

Plans are living implementation documents. They are separate from specs (which are frozen design contracts) and track task ordering, progress, and blockers.

#### Plan File Format

Plans live alongside their spec as `<spec-name>.plan.md`. For system-level plans, create `<system-spec-name>.plan.md` alongside the system spec.

```markdown
# Plan: <Spec Title>

**Spec:** @path/to/spec.md  
**Status:** Not Started | In Progress | Done | Blocked  
**Started:** YYYY-MM-DD  
**Last Updated:** YYYY-MM-DD

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 1 | Description of task | Not Started / In Progress / Done / Blocked | haiku / sonnet / opus | Any blockers or notes |

## Session Notes

### YYYY-MM-DD
- What was done, what was decided, what changed
```

**Model selection guide:**

| Use haiku for | Use sonnet for | Use opus for |
|---|---|---|
| File edits, plan updates, vcxproj changes, registry updates | Most implementation tasks, cross-file reasoning, spec work | Architecture decisions, complex debugging, spec design |
| Git commits | DiaCLI build + diagnose failure | High design-judgment tasks |
| DiaCLI run (report result only) | Pattern-following features size M+ | Complex state management, graph/visual components |

#### Plan Rules

- **Create when** implementation starts (not when the spec is approved)
- **Feature plans** — one plan per feature spec; tasks map to the feature spec's task list
- **System plans** — one plan per system spec; tasks are the feature specs themselves (track which features are done)
- **Spec is the contract, plan is the tracker** — never move design decisions into the plan; put them in the spec
- **Update the plan in the same commit** as the code it tracks
- **Link back** — the spec's Status section should reference its plan file once one exists

### Spec Commands

- `/spec-platform` - Create or update the platform spec
- `/spec-app` - Create a new application spec
- `/spec-system` - Create a new system spec  
- `/spec-feature` - Create a new feature spec (includes interview + AI review)
- `/spec-review` - Review any spec and populate AI review questions
- `/spec-trace` - Trace a feature's full lineage up to platform

### Test Commands

- `/gen-tests <target>` - Generate comprehensive tests for a Dia module or component
  - Reads public API headers, checks test-completeness-registry, proposes all applicable test types (unit, stress, golden-value, invariant, determinism, conservation, integration)
  - Creates test files, updates vcxproj, builds, runs, and updates the registry
  - Options: `--type=<type>` to limit test types, `--dry-run` to preview, `--update-registry` to refresh counts only
- `/gtest [options]` - Build and run GoogleTest suite with filtering and failure analysis

## Development Workflow

### Adding a New Module

1. Create module directory under appropriate parent (e.g., `Dia/DiaCore/NewModule/`)
2. Create `dia.[parent].[module].architecture.module.md` with YAML frontmatter (see [module-metadata-schema.md](docs/reference/registry/module-metadata-schema.md))
3. Add module to parent's `.vcxproj` and `.vcxproj.filters`
4. Update parent module's `dependent_modules` list
5. Verify module dependencies are correct

### Naming Conventions

- **Namespaces**: `Dia::Core::`, `Dia::Maths::`, `Dia::Application::`
- **Files**: PascalCase matching class names (e.g., `DynamicArray.h`, `ProcessingUnit.cpp`)
- **Classes**: PascalCase (e.g., `ProcessingUnit`, `ComponentFactory`)
- **Members**: camelCase with `m` prefix (e.g., `mCurrentPhase`, `mModuleTable`)
- **Constants**: `k` prefix (e.g., `kUniqueId`, `kMaxSize`)
- **Interfaces**: `I` prefix (e.g., `IComponent`, `IComponentFactory`)

### Code Patterns

- **Singleton pattern**: Use `Dia::Core::Singleton<T>` from `Architecture/Singleton/`
- **Observer pattern**: Use `Observer`/`ObserverSubject` from `Architecture/Observer.h`
- **String comparison**: Prefer `StringCRC` over raw strings for performance
- **Containers**: Use DiaCore containers (`DynamicArrayC`, `HashTable`) over STL when integration is needed
- **Components**: Register factories with `ComponentFactoryRegistry`, create via factories

## Common Issues

### Include Paths

When including headers:
- Use full paths from the module root: `#include <DiaCore/Containers/Arrays/Array.h>`
- Module include directories are configured in `.vcxproj` files
- Watch for circular dependencies between modules (enforced by module system)

### Thread Safety

Processing units can run on separate threads. When accessing shared state:
- Use `std::mutex` for synchronization
- `QueuePhaseTransition()` is thread-safe (vs immediate `TransitionPhase()`)
- Review phase ordering and module dependencies for race conditions

### Visual Studio Project Files

When modifying project structure:
- Update both `.vcxproj` (build rules) and `.vcxproj.filters` (IDE organization)
- Use relative paths from the `.vcxproj` location
- Maintain consistency with existing patterns in the file

## Module Documentation Format

See [docs/reference/registry/module-metadata-schema.md](docs/reference/registry/module-metadata-schema.md) for the full `dia.module.v1` YAML schema and worked examples.
