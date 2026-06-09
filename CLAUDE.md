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

## Engine vs Application Boundary

Applications (CluicheTest, CluicheEditor, CoW, future games) live as siblings under `Cluiche/`. CluicheEditor is a standalone app — not a game mode.

### Layer boundary rule

- **Reusable capability any game could consume → belongs in Dia**
- **Specific to one game's content, logic, or characters → belongs in the Application**

This applies to systems, libraries, and data equally. `Animation2D` is engine — any game could use it. CoW's dragon animation system is application — only CoW needs it. The wiring (calling engine APIs from a game module) always stays in the application; the implementation, once it's generically useful, belongs in Dia.

*Anchor cases: `DiaGame` serializer started in CluicheTest, moved to Dia because it was generic engine capability. The CluicheTest call-site stayed in CluicheTest.*

### Escalation rule

If a task would add a new header or implementation to a Dia project while working in a game project, **stop and flag it** — do not place it in the game. Describe what the Dia API should look like and ask the user to confirm the placement before proceeding.

## Build System

This is a Visual Studio C++ project using MSBuild.

### DiaCLI Commands (mandatory)

**NEVER call executables directly** (e.g., `GoogleTests.exe`, `CluicheTest.exe`). Always use `dia run` or `dia launch`. The CLI handles path resolution, working directories, and runtime dependencies. If the CLI fails, fix the CLI — do not bypass it.

| Command | When to use |
|---|---|
| `dia run <target>` | Build + run in one step (tests, game, editor). Default choice. |
| `dia launch <target>` | Run only — skip build (use when already built). |
| `dia pipeline --target <target>` | Full pipeline: compile-code → build-assets → deploy. Required for editor targets that need UI built and copied. |
| `dia scaffold module <Parent> <Name>` | Create a new Dia engine module (header, cpp, module.md, vcxproj entries). |
| `dia scaffold plugin <Name>` | Create a new editor plugin (IEditorPlugin subclass, UI, vcxproj). |
| `dia scaffold stage <Name>` | Create a new CluicheTest test stage (all 9 touch points). |
| `dia check deps` | Cross-check module dependency declarations vs actual #includes. |
| `dia validate manifest` | Validate .diaapp/.diagame/.diastage against schemas. |
| `dia env setup/verify` | First-time setup or diagnosing missing dependencies. |
| `dia docs plan <path> <#> --status <S>` | Update a plan task row (status, notes, model). Validates transitions. |
| `dia docs registry` | Regenerate module-registry.md from all module YAML files. |
| `dia docs spec-done <spec.md>` | Mark spec Done + update plan header + strike backlog entry. |
| `dia docs vcxproj-add <Project> <File>` | Add .h/.cpp to vcxproj + filters. Idempotent. |
| `dia docs backlog move <name>` | Move backlog entry to BACKLOG-HISTORY.md. |
| `dia docs spec-scaffold <type> <name>` | Generate spec skeleton (feature or system). |
| `dia docs test-scaffold <header.h>` | Generate GoogleTest file from header's public methods. |
| `dia docs precommit` | Run pre-commit checks (forbidden patterns, vcxproj sync, manifests). |

```bash
dia run googletest
dia run googletest --filter="FixedDrawLayer*"
dia run googletest --config Release
dia run cluichetest
dia run cluicheeditor
dia launch googletest --filter="SomeSuite*"
dia launch cluichetest
dia pipeline --target googletest
dia pipeline --target cluichetest --config Release
dia scaffold module DiaCore Serializer --layer platform
dia scaffold plugin SceneInspector --layout dockable
dia scaffold stage RigidBody2D --modules Physics2DModule --budget 900
dia check deps
dia check deps --verbose
dia validate manifest
dia validate manifest --path Cluiche/Assets/CluicheTest/cluichetest.diagame
dia env setup
dia env verify
dia test cli
dia docs plan docs/specs/systems/dia/foo.plan.md 3 --status Done --notes "All tests pass"
dia docs plan foo.plan.md 1 -s "In Progress"
dia docs registry
dia docs registry --dry-run
dia docs spec-done docs/specs/systems/dia/diacamera3d.md
dia docs vcxproj-add DiaCore "NewModule\\NewModule.h" --filter NewModule
dia docs vcxproj-add GoogleTests "DiaCore\\TestNewModule.cpp"
dia docs backlog move DiaCamera3D --notes "All features implemented"
dia docs spec-scaffold feature camera3d-follow --parent DiaCamera3D
dia docs spec-scaffold system DiaNewSystem
dia docs test-scaffold Dia/DiaCore/Containers/Arrays/DynamicArray.h
dia docs precommit
dia docs precommit --staged
```

### Raw MSBuild (fallback)

```bash
start Cluiche/Cluiche.sln
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
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
  - Each spec has a parent link, binding decisions (only those that constrain), and optional open design questions
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

**Steps to approval:**

1. **Interview** — Complete all interview questions with the user before writing anything
2. **Draft** — Write the full spec body (summary, goals, tasks, parent link)
3. **Binding Decisions** — List only the parent decisions that *actually constrain* this feature (not a full compliance matrix of N/A rows). If none constrain, state "No binding constraints apply" and move on.
4. **Open Design Questions** — Surface 2-3 real design uncertainties or risks. Skip if the design is straightforward. These should be questions the user might want to revisit during implementation, not generic checklists.
5. **Approval gate** — Ask the user to approve.

**Rules:**
- A **system spec** cannot be marked `Done` until ALL its child feature specs are `Approved`.
- If the spec originated from a research session, add a `**Research:**` line to the spec header pointing to `docs/research/<slug>/summary.md`. Ask the user if one exists before finalising the draft.

#### Implementing from Specs

When implementing new features using the spec-driven approach:

1. **Spec must exist and be `Approved`** before implementation starts
2. **Create a plan** — `*.plan.md` alongside the spec (see Plan Workflow below)
3. **Check binding decisions** from the spec before implementing
4. **Delegate tasks to subagents** — each task in the plan should be a separate subagent
5. **Update the plan** after each task (mark Done/Blocked, add notes)
6. **Commit after each task** before continuing
7. **Update feature spec status** as work progresses (Draft → Approved → In Progress → Done)

### Plan Workflow

Plans are living implementation documents. They are separate from specs (which are frozen design contracts) and track task ordering, progress, and blockers.

#### Plan File Format

Plans live alongside their spec as `<spec-name>.plan.md`. Tasks table columns: `# | Task | Test | Status | Model | Notes`.

Header: `**Spec:** @path` and `**Status:** In Progress | Done` (no dates — git tracks those).

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

#### Subagent Dispatch Protocol

When delegating plan tasks to subagents, follow `.claude/skills/dispatch.md`. Key rules:

- **Inline all context** — subagents start with zero conversation history. Spec excerpts, relevant code, and constraints must be in the prompt. Never say "read the plan."
- **Two-stage verification** — after a subagent reports DONE, check (1) spec compliance and (2) code quality before marking the task complete.
- **Model matches task** — haiku for mechanical edits, sonnet for standard implementation, opus for architecture/debugging.
- **Parallel only when independent** — different files, no shared headers/vcxproj, no output dependencies.
- **BLOCKED triggers debugging** — if a subagent reports BLOCKED, enter the debug skill or fix the plan before re-dispatching.

### Debugging

Follow `.claude/skills/debug.md`. One hypothesis, one change. Three failed fixes on the same root issue → stop and report. No shotgun debugging.

### Verification Gate

Follow `.claude/skills/verify.md`. Every code task completion needs a fresh run with quoted output. Never say "should work" or "previously verified." Applies to plan tasks, commits, and subagent DONE reports. Does NOT apply to specs, plans, or docs.

### Observation Opportunity Scan

After every feature implementation, at the Verify/Prove step, scan all files touched by the feature for missed instrumentation opportunities across all 5 DiaObservation pillars:

- **Logs** — new code paths (errors, state transitions, load/unload, connect/disconnect) with no `DIA_LOG_*` call
- **Traces** — per-frame or lifecycle operations with no `DIA_TRACE_ZONE`
- **Profiling** — hot loops or frame-boundary work with no `DIA_PROFILE_SCOPE`
- **Metrics** — quantitative signals (counts, durations, depths) with no `MetricRegistry` registration
- **Health** — modules or services with observable failure modes and no `IHealthReporter`

Report findings in the task notes as suggestions. They do **not** block the feature being marked Done. They feed future domain instrumentation work.

### Build and Test Output

When Claude runs `dia run` or `dia pipeline`, report only: pass/fail, failing test names, and error messages. Do not quote full test output. If the user runs tests locally, they see the full output themselves — do not repeat it.

### Anti-Rationalization

If forming the sentence "I can skip X because..." — do X more carefully, not less.

### Test-Driven Development (New Features)

Follow `.claude/skills/tdd.md`. RED-GREEN-REFACTOR for all new public API, new behavior, and new ACs. Prove RED with quoted output. Write minimal GREEN. Does not apply to mechanical tasks, bug fixes, or refactors with existing coverage. `/gen-tests` audits exhaustiveness after TDD completes.

### Commands

See `.claude/steering/commands.md` for full command reference (`/spec-*`, `/review`, `/gen-tests`, `/gtest`).

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
