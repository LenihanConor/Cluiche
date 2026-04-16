# Application Spec: Dia

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose

Dia is the game engine application that provides all shared engine infrastructure for the Cluiche platform. While organized as an "application" in the spec hierarchy, Dia functionally serves as the shared codebase that all other applications (games, tools, tests) depend on. It provides core systems (DiaCore, DiaMaths, DiaGraphics, DiaInput, etc.), runtime frameworks (DiaApplication), and build tooling (DiaAPI) that enable rapid game development with a component-based architecture, multi-threaded execution model, and modular design.

## Systems

| System | Description | Spec |
|--------|-------------|------|
| DiaAPI | Plugin-based CLI framework for build operations, asset pipelines, and developer tools | [diacli.md](../systems/dia/diacli.md) |
| DiaPython | Python embedding framework - wraps pybind11 with clean C++ API for scripting integration | [diapython.md](../systems/dia/diapython.md) |
| DiaCore | Foundation library (containers, type system, memory, logging, CRC) | TBD |
| DiaMaths | Math library (vectors, matrices, transforms, shapes) | TBD |
| DiaApplication | Application framework (ProcessingUnit/Phase/Module architecture) | TBD |
| DiaGraphics | Graphics abstraction layer (ICanvas, Frame, rendering) | TBD |
| DiaWindow | Window management | TBD |
| DiaInput | Input handling (keyboard, mouse, events) | TBD |
| DiaUI | UI systems (Awesomium integration) | TBD |

## Application-Specific Architecture

### Modular Engine Structure

Dia is organized into independent subsystems with clear responsibilities and dependencies:
- **Module System**: Each module documented with `dia.*.architecture.module.md` YAML frontmatter
- **Dependency Management**: Module dependency graph validated via `Tools/dia_modules.py`
- **Public APIs**: Each module exposes headers, namespaces, and entry points

### Runtime Framework (DiaApplication)

ProcessingUnit/Phase/Module architecture for multi-threaded game execution:
- **ProcessingUnit**: High-level execution containers (can run on separate threads)
- **Phase**: Execution stages with state machine transitions
- **Module**: Functional units providing services to phases

### Build-Time Tooling

DiaAPI provides extensible command-line tools for asset pipelines and build automation.

## Platform Dependencies

Dia is the engine - it provides dependencies for other applications rather than consuming them. External dependencies:

- **SFML** (External/SFML) - Graphics, window, multimedia
- **Awesomium SDK** (External/Awesomium) - HTML UI framework
- **jsoncpp** (External/jsoncpp-master) - JSON parsing
- **Webix / VisJS** (External/) - Web UI for debugging/visualization
- **GoogleTest** (External/googletest) - Unit testing framework

## Out of Scope

What the Dia engine deliberately does NOT provide:

- **Game-specific logic** - Dia is an engine, not a game; game logic belongs in game applications
- **High-level game features** - No built-in inventory, quest, or gameplay systems
- **Content creation tools** - No level editors, asset authoring tools (yet - DiaAPI is first step)
- **Network/multiplayer** - Not yet implemented
- **Mobile/console platforms** - Windows-only currently

## Key Users / Personas

1. **Game Developers** - Building games on Dia; need stable APIs and good documentation
2. **Engine Developers** - Extending Dia systems; need clear module boundaries and architecture
3. **Technical Artists** - Processing assets through build tools; need reliable pipelines
4. **Platform Maintainers** - Managing engine evolution; need dependency tracking and breaking change management

## Decisions

<!-- Decisions specific to this application. Binding decisions cascade to all systems within Dia.
     AI: Always check parent platform decisions (Cluiche.md) first — those take precedence.
     Use AD- prefix for application-level decision IDs. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AD-001 | Module system with YAML frontmatter documentation | Enables tooling (dependency validation, graph generation); enforces clear ownership | All Dia modules | Accepted | Yes |
| AD-002 | No STL containers in public APIs | Dia containers (DynamicArrayC, HashTable) ensure consistent memory management and engine integration | All Dia modules | Accepted | Yes |
| AD-003 | Namespace convention: `Dia::<Module>::` | Clear ownership; prevents naming conflicts | All Dia modules | Accepted | Yes |
| AD-004 | ProcessingUnit/Phase/Module for application structure | Multi-threaded execution with explicit scheduling; clear lifecycle management | DiaApplication | Accepted | Yes |
| AD-005 | Component-based entities (IComponent/IComponentObject) | Composition over inheritance; flexible runtime entity construction | All game-facing modules | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child systems · `No` = guidance only

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Should each Dia module (DiaCore, DiaMaths, etc.) be a system in this spec? | Yes - each major module should have a system spec defining its public API, responsibilities, and features |
| 2 | Scope | Are build tools (DiaAPI) part of Dia or separate application? | Part of Dia - they're engine tooling, depend on DiaCore, and extend the engine's capabilities |
| 3 | Dependencies | Should deprecated code (Dia/DiaCore/Deprecated/) be documented? | No - deprecated code is not compiled and should not be referenced |
| 4 | Decisions | Do Dia decisions (AD-xxx) override platform decisions (PD-xxx)? | No - platform decisions take precedence. Dia decisions add engine-specific constraints within platform rules |

## Status

`Active` - Core engine application for the Cluiche platform
