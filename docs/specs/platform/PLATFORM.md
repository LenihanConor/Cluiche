# Platform Spec: Cluiche

## Overview

**Cluiche** (Irish for "game") is a modular C++ game engine platform built around the **Dia framework**. The platform provides a comprehensive set of reusable engine modules (DiaCore, DiaMaths, DiaGraphics, DiaInput, etc.) that enable rapid development of game applications with a component-based architecture, multi-threaded execution model, and strong separation of concerns. The platform serves game developers who need a flexible, performant, and well-architected foundation for building 2D and 3D games on Windows.

## Applications

| Application | Description | Spec | Status |
|-------------|-------------|------|--------|
| Cluiche | Main game application demonstrating the Dia engine capabilities | [cluiche.md](../applications/cluiche.md) | Active |

## Shared Codebase

The platform provides a complete game engine infrastructure through the **Dia framework** - a modular C++ library organized into subsystems.

### Shared Libraries / Modules

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

**Other Subsystems:**
- **DiaPhysics** - Physics simulation (planned)
- **DiaAI** - AI systems (planned)
- **DiaIO** - File I/O utilities

**Module System:**
- Each module documented with `dia.*.architecture.module.md` YAML frontmatter
- Module dependency graph maintained via `Tools/dia_modules.py`
- 56+ architecture module files defining public APIs, responsibilities, and dependencies

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
