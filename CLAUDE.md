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

### Build Commands

```bash
# Open solution in Visual Studio
start Cluiche/Cluiche.sln

# Build via MSBuild (from repository root)
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=Win32
msbuild Cluiche/Cluiche.sln /p:Configuration=Release /p:Platform=Win32

# Build specific project
msbuild Dia/DiaCore/DiaCore.vcxproj /p:Configuration=Debug /p:Platform=Win32
```

### Test Commands

```bash
# Run unit tests (after building UnitTests project)
Cluiche/bin/exe/Debug/UnitTests.exe
```

### Configurations

- `Debug|Win32` - Primary development configuration
- `Release|Win32` - Optimized release build
- `Debug|x64`, `Release|x64` - 64-bit configurations (limited support)

## Architecture Overview

### Modular Structure

The engine is organized into **Dia** modules, each documented with a `dia.*.architecture.module.md` file containing YAML frontmatter. These files define:
- Module ID and dependencies
- Public API (headers, namespaces, entry points)
- Responsibilities and non-responsibilities
- Parent/child module relationships

Use `Tools/dia_modules.py` to analyze module dependencies and generate graphs.

### Key Modules

**DiaCore** (`Dia/DiaCore/`) - Foundation library containing:
- **Architecture/** - Patterns (Singleton, Factory, Observer, Functor) and Component system
- **Containers/** - Custom data structures (Arrays, HashTables, LinkList, BitFlag, Graphs, Strings)
- **Core/** - Assertions, call stacks, logging
- **Memory/** - Memory management utilities
- **Type/** - Type system and serialization
- **CRC/** - CRC hashing for string IDs
- **Time/** & **Timer/** - Time tracking and frame limiting
- **FilePath/** - File I/O and path handling
- **Json/** - JSON parsing (jsoncpp wrapper)

**DiaMaths** (`Dia/DiaMaths/`) - Math library with vectors, matrices, and shapes (2D/3D)

**DiaApplication** (`Dia/DiaApplication/`) - Application framework defining:
- **ProcessingUnit** - High-level execution containers (can run on separate threads)
- **Phase** - Execution stages within a processing unit with state transitions
- **Module** - Functional units that phases depend on
- **StateObject** - Base class for stateful application objects

**DiaGraphics** (`Dia/DiaGraphics/`) - Graphics abstraction layer
**DiaWindow** (`Dia/DiaWindow/`) - Window management
**DiaInput** (`Dia/DiaInput/`) - Input handling
**DiaUI/DiaUIAwesomium** (`Dia/DiaUI*/`) - UI systems using Awesomium
**DiaSFML** (`Dia/DiaSFML/`) - SFML integration layer
**DiaAPI** (`Dia/DiaAPI/`) - Plugin-based CLI framework for build operations and asset pipelines

### Component System

The engine uses a component-based architecture (`DiaCore/Architecture/Components/`):
- **IComponent** - Base component interface with unique IDs
- **IComponentObject** - Objects that contain components
- **IComponentFactory** - Factory interface for creating components
- **ComponentFactoryRegistry** - Central registry for factories
- **DynamicComponentFactory** - Heap-allocated components
- **StaticPooledComponentFactory** - Pre-allocated object pools

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
- **Awesomium SDK** - HTML UI framework
- **jsoncpp-master** - JSON parsing (used via `DiaCore/Json/`)
- **Webix** & **VisJS** - Web UI frameworks for debugging/visualization

## Project Structure

```
Cluiche/
├── Dia/                          # Main engine modules
│   ├── DiaCore/                  # Core utilities and containers
│   ├── DiaMaths/                 # Math library
│   ├── DiaGraphics/              # Graphics abstraction
│   ├── DiaWindow/                # Window management
│   ├── DiaInput/                 # Input handling
│   ├── DiaApplication/           # Application framework
│   └── ...                       # Other subsystems
├── Cluiche/                      # Main solution directory
│   ├── Cluiche.sln               # Visual Studio solution
│   ├── CluicheTest/              # Main executable project
│   │   ├── Main.cpp              # Entry point
│   │   ├── ApplicationFlow/      # Main processing unit
│   │   └── Levels/               # Game levels (DummyLevel, UnitTestLevel)
│   └── Tests/UnitTests/          # Unit test project
├── External/                     # Third-party dependencies
├── Tools/                        # Build and analysis tools
│   └── dia_modules.py            # Module dependency analyzer
└── docs/                         # Documentation
    ├── specs/                    # Spec-driven development (Platform→App→System→Feature)
    └── reference/                # Reference docs (architecture, API, design, testing)
```

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

#### Implementing from Specs

When implementing new features using the spec-driven approach:

1. **Spec must exist and be `Approved`** before implementation starts
2. **Read the full spec chain** - Every feature spec has a Traceability table linking back to System → Application → Platform
3. **Check binding decisions** - Platform and Application binding decisions must be honored by all child specs
4. **Implement with spec reference**: Point agents at the feature spec file path
5. **Delegate tasks to subagents** - Each task in the feature spec should be a separate subagent
6. **Commit after each task** before continuing
7. **Update feature spec status** as work progresses (Draft → Approved → In Progress → Done)

### Spec Commands

- `/spec-platform` - Create or update the platform spec
- `/spec-app` - Create a new application spec
- `/spec-system` - Create a new system spec  
- `/spec-feature` - Create a new feature spec (includes interview + AI review)
- `/spec-review` - Review any spec and populate AI review questions
- `/spec-trace` - Trace a feature's full lineage up to platform

## Development Workflow

### Adding a New Module

1. Create module directory under appropriate parent (e.g., `Dia/DiaCore/NewModule/`)
2. Create `dia.[parent].[module].architecture.module.md` with YAML frontmatter
3. Add module to parent's `.vcxproj` and `.vcxproj.filters`
4. Update parent module's `dependent_modules` list
5. Verify with: `python Tools/dia_modules.py --validate`

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

### Deprecated Code

The `Dia/DiaCore/Deprecated/` folder contains old implementations that have been superseded:
- Old `CollectionShit/` utilities → Use `Architecture/` instead
- Old `LinkLists/` → Use `Containers/LinkList/` instead

These files are not compiled and are kept for historical reference only.

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

When creating or updating module documentation files:

```yaml
---
schema: dia.module.v1
module_id: dia.parent.module
name: ModuleName
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaParent/DiaModule
language: cpp
parent_module_id: dia.parent

summary: >
  Brief one-line description.

intent: >
  Detailed purpose and goals.

responsibilities:
  - What this module owns
  - What it provides

non_responsibilities:
  - What it explicitly does not handle

dependent_modules:
  - dia.module.child1
  - dia.module.child2

public_api:
  headers:
    - Dia/DiaModule/PublicHeader.h
  namespaces: []
  entry_points:
    - ClassName
    - FunctionName

dependencies:
  required:
    - dia.core.containers
  forbidden:
    - dia.graphics  # Example: prevent circular deps
---
```
