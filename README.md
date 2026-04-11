# Cluiche Game Framework

**Cluiche** is a multi-threaded game application framework built on the **Dia** engine, designed for high-performance game development with clear separation between simulation, rendering, and UI coordination.

---

## What is Cluiche?

Cluiche provides a phase-based, module-driven architecture for building games with explicit threading control. The framework separates concerns across three independent threads (Main, Render, Sim) while providing a pluggable level system for game state management.

---

## Project Structure

```
Cluiche/
├── Cluiche/           Multi-threaded game application framework
│   ├── Main/Render/Sim threads with phase-based execution
│   ├── Pluggable level system (DummyLevel, UnitTestLevel)
│   └── Module-based architecture for extensibility
│
├── Dia/               Core game engine (13 subsystems)
│   ├── DiaApplication - Module/Phase/ProcessingUnit framework
│   ├── DiaCore       - Containers, Type system, Time management
│   ├── DiaGraphics   - Platform-agnostic rendering (ICanvas)
│   ├── DiaMaths      - Vector, Matrix, Transform, Shape math
│   ├── DiaInput      - Input event system
│   ├── DiaUI         - UI integration (Awesomium)
│   └── [7 more subsystems]
│
├── Tools/             Development utilities
│   ├── CLI/          - MDK command-line interface (Python)
│   └── Console/      - Blue Console web-based runtime debugger (TypeScript)
│
└── External/          Third-party dependencies
    ├── SFML/         - Graphics/Audio/Input backend
    ├── jsoncpp/      - JSON configuration
    └── [3 more libraries]
```

---

## Quick Start

### For Humans

- **[5-Minute Quickstart](docs/00-getting-started/quickstart.md)** - Get oriented fast
- **[Building the Project](docs/00-getting-started/building-the-project.md)** - Build instructions
- **[Common Development Tasks](docs/00-getting-started/common-tasks.md)** - Developer workflows

### For AI Agents

- **[AI Agent Entry Point](docs/06-ai-guides/AI-README.md)** - Start here for AI-optimized navigation
- **[Codebase Map](docs/06-ai-guides/codebase-map.md)** - Structured codebase navigation
- **[Entry Points Guide](docs/06-ai-guides/entry-points.md)** - Where to start for common tasks

---

## Documentation

### Core Documentation

- **[Architecture Overview](docs/01-architecture/architecture.md)** - System architecture and design
- **[Design Philosophy](docs/02-design/design.md)** - Why things are the way they are
- **[Requirements](docs/03-requirements/requirements.md)** - Feature requirements checklist
- **[Testing Strategy](docs/04-testing/test.md)** - Testing approach and guidelines
- **[API Documentation](docs/05-api/api-overview.md)** - API reference by subsystem

### Browse All Documentation

- **[Full Documentation Index](docs/README.md)** - Complete documentation navigation

---

## Key Concepts

### Module/Phase/ProcessingUnit Pattern

Dia's threading architecture is built on three core concepts:

- **Module** - Reusable component with explicit dependencies (e.g., TimeServer, InputManager)
- **Phase** - Execution stage within a thread (e.g., Boot, Running)
- **ProcessingUnit** - Thread of execution that runs phases and their modules

### Three-Thread Architecture

Cluiche uses three independent threads for performance and clarity:

| Thread | Purpose | Frame Rate |
|--------|---------|------------|
| **Main** | Bootstrap application, coordinate UI, spawn other threads | As needed |
| **Render** | Graphics rendering, display management | 60 FPS target |
| **Sim** | Game simulation, physics, logic | Variable rate |

### Level System

Pluggable game state management through the `ILevel` interface:

- **DummyLevel** - Example level implementation
- **UnitTestLevel** - Testing harness for in-engine tests
- **Custom Levels** - Register via `LevelFactory` for runtime loading

---

## Current Status

**Platform:** Windows (Visual Studio 2015+)  
**Language:** C++11  
**Build System:** Visual Studio projects (.sln/.vcxproj)  
**Thread Model:** 3 threads (Main/Render/Sim) with std::mutex synchronization

### Recent Work

- ✅ Thread-safe Random number generation (2026-03)
- ✅ DiaMaths bug fixes (Interpolation, IntersectionTests) (2026-03)
- ✅ DiaCore cleanup (deprecated code removal) (2026-03)
- ✅ Comprehensive documentation initiative (2026-03)

See [Known Issues](docs/09-development/known-issues.md) and [Changelog](docs/09-development/changelog.md) for details.

---

## Getting Help

- **Documentation:** See [docs/](docs/) for comprehensive guides
- **Module Reference:** 56 `.architecture.module.md` files throughout Dia subsystems
- **External Tools:** See [Tools Documentation](docs/08-tools/tools-overview.md)

---

## Project Philosophy

Cluiche and Dia prioritize:

- **Explicit over Implicit** - No hidden behaviors or magic
- **Composition over Inheritance** - Module aggregation instead of deep hierarchies
- **Thread Safety by Design** - Clear thread boundaries with explicit synchronization
- **Type Safety** - Compile-time type IDs and strong typing
- **Fail Fast** - Assertions and validation at boundaries

See [Design Philosophy](docs/02-design/design.md) for complete rationale.

---

## License

[License information to be added]

---

## Contributing

See [Contributing Guide](docs/09-development/contributing.md) for development workflow and coding standards.