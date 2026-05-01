# Application Spec: CluicheTest

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose

CluicheTest is a demo game and engine testbed built on the Dia engine. It serves as both a demonstration of the engine's capabilities and as a testbed for validating engine features. The application showcases the multi-threaded ProcessingUnit architecture, component-based entity system, and modular design patterns. This is one of several applications that can be built on the Cluiche platform using the Dia engine.

## Systems

| System | Description | Spec |
|--------|-------------|------|
| ApplicationFlow | Main processing unit orchestration, level management, core game loop | TODO |
| Levels | Level loading, lifecycle management, and level-specific logic | TODO |
| Rendering | Graphics rendering, canvas management, frame composition | TODO |
| Input | Input event handling, keyboard/mouse state management | TODO |
| UI | User interface rendering and interaction | TODO |

## Application-Specific Architecture

### Threading Model

Cluiche implements the platform's multi-threaded architecture with three ProcessingUnits:

1. **MainProcessingUnit** - Core game logic, level management, coordination
2. **RenderProcessingUnit** - Graphics rendering on dedicated render thread
3. **SimProcessingUnit** - Physics simulation and game simulation logic

### Level System

- **DummyLevel** - Basic test level for engine validation
- **UnitTestLevel** - Level that runs unit tests within the game context
- Future levels defined per-feature specs

### Entry Point

`Cluiche/Cluiche/Main.cpp` - Application entry point that initializes the Dia engine and starts the main processing unit.

## Platform Dependencies

Which shared platform modules does this app use?

- [x] DiaCore (Containers, Type system, Core utilities)
- [x] DiaMaths (Vector, Matrix, Transform, Shape)
- [x] DiaApplication (ProcessingUnit, Phase, Module)
- [x] DiaGraphics (ICanvas, Frame, rendering abstraction)
- [x] DiaWindow (Window management)
- [x] DiaInput (Input event handling)
- [x] DiaUI (UI framework)
- [x] DiaSFML (SFML backend integration)
- [ ] DiaPhysics (planned, not yet implemented)
- [ ] DiaAI (planned, not yet implemented)

## Out of Scope

What this application deliberately does NOT do:

- **Not a production game** - Cluiche is a demonstration and testbed, not a shipped game product
- **Not a level editor** - Level creation is code-based, not via tooling
- **Not network multiplayer** - Single-player only; networking not in scope
- **Not cross-platform** - Windows-only; other platforms not targeted
- **Not a modding platform** - No plugin system or mod support

## Key Users / Personas

1. **Engine Developers** - Building and testing Dia engine features; need visibility into engine behavior
2. **Game Developers** - Learning how to use the Dia engine; need working examples and patterns
3. **QA/Testers** - Validating engine functionality; need test levels and diagnostic tools
4. **Documentation Writers** - Demonstrating engine capabilities; need reference implementations

## Decisions

<!-- Decisions specific to this application. Binding decisions cascade to all systems and features within it.
     AI: Always check parent platform decisions (Cluiche.md) first — those take precedence.
     Use AD- prefix for application-level decision IDs. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AD-001 | Use three ProcessingUnits (Main/Render/Sim) | Demonstrates platform's multi-threading capabilities; validates thread-safe phase transitions | This app + all systems | Accepted | Yes |
| AD-002 | Levels are code-based (not data-driven) | Simpler for testing and demonstration; no need for asset pipeline yet | Levels system | Accepted | Yes |
| AD-003 | Entry point in Main.cpp initializes engine and starts main PU | Standard pattern; clear application startup sequence | ApplicationFlow | Accepted | Yes |
| AD-004 | Test levels (DummyLevel, UnitTestLevel) included in application | Validates engine features; enables in-context testing | Levels system | Accepted | No |
| AD-005 | Application serves as engine testbed, not shipped product | Frees team from production constraints; focus on engine validation | This app + all systems | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`  
**Binding:** `Yes` = enforced constraint on all child systems and features · `No` = guidance only

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Systems | Should the three ProcessingUnits (Main/Render/Sim) be separate systems or part of ApplicationFlow? | Part of ApplicationFlow - they are orchestration infrastructure, not game logic systems |
| 2 | Dependencies | Is DiaIO (File I/O) used? Not listed but likely needed for assets. | Yes, should be added to dependencies - used for level loading and asset management |
| 3 | Out of Scope | Are there plans to build a production game on this platform later? | TBD - out of scope for current application spec; would be a separate application if pursued |
| 4 | Architecture | How do the three ProcessingUnits communicate? Shared state? Message passing? | Design TBD - needs system-level spec for inter-PU communication patterns |

## Status

`Active` - Primary demonstration application for the Dia engine platform
