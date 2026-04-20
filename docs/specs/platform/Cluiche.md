# Platform Spec: Cluiche

## Overview

**Cluiche** (Irish for "game") is a game development platform that provides a complete ecosystem for building, testing, and shipping games on Windows. The platform is built around the **Dia engine** (a modular C++ game engine framework) and supports multiple applications including games, tools, and test suites. The architecture enables developers to build games on a solid engine foundation with component-based design, multi-threaded execution, and strong separation of concerns.

## Applications

| Application | Description | Spec | Status |
|-------------|-------------|------|--------|
| Dia | The game engine (DiaCore, DiaMaths, DiaGraphics, etc.) - shared engine code organized as an application for spec purposes | [dia.md](../applications/dia.md) | Active |
| CluicheTest | Demo game and engine testbed that showcases Dia engine capabilities | [cluichetest.md](../applications/cluichetest.md) | Active |
| CluicheEditor | Plugin-based editor application for building and debugging Cluiche games, built on DiaEditor framework | [cluicheeditor.md](../applications/cluicheeditor.md) | Draft |
| GoogleTests | Unit testing suite for validating Dia engine modules with intelligent dirty test tracking | [googletests.md](../applications/googletests.md) | Active |

## Shared Codebase

The **Dia application** serves as the shared engine code for all other applications on the platform. While organized as an "application" in the spec hierarchy, Dia is functionally the shared game engine infrastructure that other applications consume.

### Dia Engine Modules

The Dia engine is a modular C++ framework organized into subsystems:

**Core Infrastructure:**
- **DiaCore** - Foundation library (containers, type system, serialization, memory management, CRC hashing)
- **DiaMaths** - Math library (vectors, matrices, shapes for 2D/3D)
- **DiaApplication** - Application framework (ProcessingUnit/Phase/Module architecture)

**Rendering & Graphics:**
- **DiaGraphics** - Graphics abstraction layer (ICanvas, Frame)
- **DiaWindow** - Window management
- **DiaSFML** - SFML integration layer

**Input & UI:**
- **DiaInput** - Input handling (keyboard, mouse, events)
- **DiaUI** / **DiaUIAwesomium** - UI systems using Awesomium HTML framework

**Tools & Build Systems:**
- **DiaAPI** - Plugin-based CLI framework for build operations and asset pipelines

**Other Subsystems:**
- **DiaPhysics** - Physics simulation (planned)
- **DiaAI** - AI systems (planned)
- **DiaIO** - File I/O utilities

**Module System:**
- Each module documented with `dia.*.architecture.module.md` YAML frontmatter
- Module dependency graph maintained via `Tools/dia_modules.py`
- 56+ architecture module files defining public APIs, responsibilities, and dependencies

See @docs/specs/applications/dia.md for full Dia engine specification.

### Shared Infrastructure

- **Component System**: IComponent, IComponentObject, IComponentFactory, ComponentFactoryRegistry
- **Factory Patterns**: DynamicComponentFactory (heap), StaticPooledComponentFactory (pools)
- **Threading Model**: Multi-threaded ProcessingUnits (Main/Render/Sim threads)
- **Type System**: Runtime type reflection and serialization
- **String IDs**: Compile-time CRC hashing via StringCRC for efficient comparisons
- **Build System**: Visual Studio MSBuild, Win32 primary target

### Shared Design Patterns

- **Singleton**: `Dia::Core::Singleton<T>`
- **Observer**: `Observer` / `ObserverSubject`
- **Factory**: Component factory registry pattern
- **State Machine**: Phase transition system in DiaApplication
- **Component-Based Architecture**: Composition over inheritance

## Architecture Principles

1. **Modular Design**: Clear separation of concerns through Dia modules with defined dependencies and public APIs
2. **Component-Based Architecture**: Game entities built from composable components rather than deep inheritance hierarchies
3. **Multi-Threaded Execution**: ProcessingUnits can run on separate threads with explicit phase scheduling and synchronization
4. **Type Safety**: Compile-time type checking with runtime reflection support via the Type system
5. **Performance First**: Custom containers, object pooling, compile-time CRC hashing, and zero-cost abstractions where possible
6. **Documentation-Driven**: Every module has architecture documentation; specs drive new feature development

## Non-Functional Requirements

| Concern | Requirement |
|---------|-------------|
| Performance | 60 FPS minimum for typical game scenes; frame time budgets enforced |
| Thread Safety | All shared state protected by mutexes; QueuePhaseTransition() is thread-safe |
| Memory Management | Object pooling for frequently allocated types; no raw new/delete in game code |
| Maintainability | Module dependencies acyclic and validated; public APIs clearly documented |
| Testability | Unit tests via Google Test; subsystems mockable via interfaces |
| Platform Support | Primary: Windows 10+ (Win32); Secondary: x64 configurations (limited) |

## Conventions

Coding standards, naming conventions, and git workflow defined in:
- Tech standards: @.claude/steering/tech.md
- Codebase structure: @.claude/steering/structure.md

## Change Policy

Changes to shared Dia modules follow this process:

1. **Proposal**: Create feature spec using `/spec-feature` in relevant system
2. **Review**: AI review questions must be answered; binding decisions checked
3. **Implementation**: Changes implemented per approved spec
4. **Testing**: Unit tests required for all API changes
5. **Documentation**: Update module architecture files and API docs
6. **Breaking Changes**: Require PD- decision approval; impact analysis on all applications
7. **Communication**: Breaking changes documented in changelog before merge

Module dependency changes validated via `python Tools/dia_modules.py --validate`.

## Decisions

<!-- Architectural and design decisions that apply to ALL applications on this platform.
     Binding decisions cascade down and must be respected by every app, system, and feature.
     AI: When reviewing child specs, check every Binding=Yes decision is honoured. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| PD-001 | Use StringCRC for all entity/component IDs | Compile-time hashing provides zero-cost string comparison; prevents typos | Platform-wide | Accepted | Yes |
| PD-002 | ProcessingUnit/Phase/Module architecture for app structure | Enables multi-threaded execution with explicit scheduling; clear lifecycle management | Platform-wide | Accepted | Yes |
| PD-003 | Component-based entities (IComponent/IComponentObject) | Composition over inheritance; enables flexible runtime entity construction | Platform-wide | Accepted | Yes |
| PD-004 | No STL containers in public APIs | Dia containers (DynamicArrayC, HashTable, LinkList) ensure consistent memory management and integration with engine | Platform-wide | Accepted | Yes |
| PD-005 | Win32 as primary build target | Team expertise and tooling optimized for Windows; x64 secondary priority | Platform-wide | Accepted | No |
| PD-006 | Visual Studio project files are source of truth | MSBuild used for all builds; manual project file maintenance required | Platform-wide | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`  
**Binding:** `Yes` = enforced constraint on all children · `No` = guidance only

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Applications | Should DummyLevel and UnitTestLevel be documented as separate applications or as part of Cluiche? | Part of Cluiche - they are test levels within the main application |
| 2 | Shared Infrastructure | Are there specific performance budgets for frame times on different threads? | Main thread: <16ms, Render thread: <16ms, Sim thread: variable (design to be specified per-application) |
| 3 | Decisions | Should deprecated code in `Dia/DiaCore/Deprecated/` be covered by binding decisions? | No - deprecated code is not compiled and should not be referenced by any new work |
| 4 | Change Policy | What is the process for emergency hotfixes that bypass the spec workflow? | TBD - needs definition for production releases |
