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

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Description of task | TestName that defines done (or — for non-TDD tasks) | Not Started / In Progress / Done / Blocked | haiku / sonnet / opus | Any blockers or notes |

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

#### Subagent Dispatch Protocol

When delegating plan tasks to subagents, follow `.claude/skills/dispatch.md`. Key rules:

- **Inline all context** — subagents start with zero conversation history. Spec excerpts, relevant code, and constraints must be in the prompt. Never say "read the plan."
- **Two-stage verification** — after a subagent reports DONE, check (1) spec compliance and (2) code quality before marking the task complete.
- **Model matches task** — haiku for mechanical edits, sonnet for standard implementation, opus for architecture/debugging.
- **Parallel only when independent** — different files, no shared headers/vcxproj, no output dependencies.
- **BLOCKED triggers debugging** — if a subagent reports BLOCKED, enter the debug skill or fix the plan before re-dispatching.

### Debugging

When something breaks (build failure, test failure, crash), follow `.claude/skills/debug.md`. Key rules:

- **Investigate before fixing** — read the full error, identify the boundary, check recent changes.
- **One hypothesis, one change** — never change two things at once.
- **Three-Fix Ceiling** — after 3 failed attempts at the same root issue, STOP and report what was tried. Wait for user direction.
- **No shotgun debugging** — no commenting out code, no pragma disables, no "just make it compile" hacks.

### Verification Gate

Every task completion requires fresh evidence. Follow `.claude/skills/verify.md`. Key rules:

- **Run the verification command NOW** — not "I ran it earlier." Fresh run, full output, quoted evidence.
- **Banned language** — never say "should work", "likely passes", "previously verified." Replace with actual output.
- **Failure enters debugging** — if verification fails, the task is not done. Enter the debug skill.
- **Applies to** — plan tasks marked Done, commit claims, subagent DONE reports.
- **Does not apply to** — specs, plans, docs, intermediate edits.

### Anti-Rationalization

Never accept these excuses to skip process — they are the most common failure modes:

- "This is simple enough to skip the test" — No. If it's simple, the test is fast to write.
- "I'll write the test after since I need to explore the API first" — No. Write a test for what you THINK the API should be. If you're wrong, the test evolves.
- "The Three-Fix Ceiling doesn't apply here because each error is different" — No. Three failed fixes is three failed fixes. Stop.
- "I'll verify later once the next task is also done" — No. Verify NOW. Cascading failures are harder to debug.
- "I don't need to inline context because the subagent can read the file" — No. Inline it. Subagents that explore waste tokens and miss things.
- "This is just a refactor so it doesn't need a test" — If it has no test, how do you know it's still correct?
- "The spec doesn't explicitly say this, but obviously..." — No. If it's not in the spec, ask. Don't invent requirements.
- "I'll skip the RED step because I know the test will fail" — No. Prove it. Quote the output.

If you catch yourself forming a sentence that starts with "I can skip X because...", treat that as a signal to do X more carefully, not less.

### Test-Driven Development (New Features)

All new feature work follows RED-GREEN-REFACTOR. See `.claude/skills/tdd.md`. Key rules:

- **Write the test FIRST** — assert the desired behavior before implementing it.
- **Confirm RED** — run the test, quote the failure output. If it passes, investigate why.
- **Minimal GREEN** — write only enough code to make the test pass. Stop.
- **Refactor while green** — clean up with the safety net of a passing test.
- **Applies to** — new public API, new behavior, new ACs in feature specs.
- **Does not apply to** — mechanical tasks (vcxproj, registry), bug fixes, refactors with existing coverage, prototypes.
- **`/gen-tests` still runs after** — TDD writes essential tests per AC. `/gen-tests` audits for exhaustiveness (edge cases, stress, boundary).

### Spec Commands

- `/spec-platform` - Create or update the platform spec
- `/spec-app` - Create a new application spec
- `/spec-system` - Create a new system spec  
- `/spec-feature` - Create a new feature spec (includes interview + AI review)
- `/spec-review` - Review any spec and populate AI review questions
- `/spec-trace` - Trace a feature's full lineage up to platform

### Review Command

- `/review <spec-path>` - Review implementation against its spec with 8 passes
  - Passes: Spec Compliance, Test Exhaustiveness, Architecture, Product, Performance, Thread Safety, Binding Decisions, API Surface
  - Outputs a punch-list table with severity (Critical/Important/Minor), location, and suggestion per finding
  - Verdict: PASS / PASS WITH ISSUES / BLOCKED
  - Options: `--pass=<name>` for single pass, `--severity=<level>` to filter, `--diff=<range>` for custom diff range
  - Use after completing a feature (all plan tasks done) or before merging to master

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
