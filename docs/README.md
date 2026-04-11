# Cluiche Documentation

Welcome to the comprehensive documentation for the Cluiche game framework and Dia engine.

---

## Quick Navigation

### New to Cluiche?

Start here to get oriented:

- **[5-Minute Quickstart](00-getting-started/quickstart.md)** - Essential concepts and orientation
- **[Building the Project](00-getting-started/building-the-project.md)** - Get up and running
- **[Common Tasks](00-getting-started/common-tasks.md)** - Everyday developer workflows
- **[Glossary](00-getting-started/glossary.md)** - Terms and acronyms

### For AI Agents

Optimized documentation for AI codebase navigation:

- **[AI Entry Point](06-ai-guides/AI-README.md)** - Start here for AI-optimized navigation
- **[Codebase Map](06-ai-guides/codebase-map.md)** - Structured directory and file navigation
- **[Entry Points](06-ai-guides/entry-points.md)** - Where to start for common tasks
- **[Patterns Reference](06-ai-guides/patterns-reference.md)** - Code patterns with examples

---

## Core Documentation

### Architecture

Understanding how the system is built:

- **[Architecture Overview](01-architecture/architecture.md)** ⭐ PRIMARY - Complete system architecture
- [Cluiche Application Architecture](01-architecture/cluiche-application.md) - Application layer details
- [Dia Engine Architecture](01-architecture/dia-engine.md) - Engine subsystems
- [Threading Model](01-architecture/threading-model.md) - Main/Render/Sim thread design
- [Module System](01-architecture/module-system.md) - Module/Phase/ProcessingUnit pattern
- [Level System](01-architecture/level-system.md) - Level loading and lifecycle
- [External Dependencies](01-architecture/external-dependencies.md) - Third-party integration
- [Architecture Diagrams](01-architecture/diagrams/) - Mermaid diagrams

### Design

Understanding why things are the way they are:

- **[Design Philosophy](02-design/design.md)** ⭐ PRIMARY - Core design intent and rationale
- [Why Dia Engine?](02-design/why-dia.md) - Engine design decisions
- [Why Module/Phase/PU?](02-design/why-module-phase-pu.md) - Threading architecture rationale
- [Why Type System?](02-design/why-type-system.md) - Reflection system benefits
- [Design Patterns](02-design/design-patterns.md) - Common patterns used
- [Historical Decisions](02-design/historical-decisions.md) - Legacy choices and context
- [Future Directions](02-design/future-directions.md) - Planned evolution

### Requirements

What the system must do:

- **[Requirements Overview](03-requirements/requirements.md)** ⭐ PRIMARY - Complete requirements checklist
- [Functional Requirements](03-requirements/functional-requirements.md) - What it does
- [Non-Functional Requirements](03-requirements/non-functional-requirements.md) - How well it does it
- [Cluiche Requirements](03-requirements/cluiche-requirements.md) - Application-specific
- [Dia Requirements](03-requirements/dia-requirements.md) - Engine-specific
- [Traceability Matrix](03-requirements/traceability-matrix.md) - Requirements → Implementation

### Testing

How to verify correctness:

- **[Testing Strategy](04-testing/test.md)** ⭐ PRIMARY - Complete testing approach
- [Unit Testing](04-testing/unit-testing.md) - Unit test guidelines
- [Integration Testing](04-testing/integration-testing.md) - Cross-system testing
- [Performance Testing](04-testing/performance-testing.md) - Performance validation
- [Thread Safety Testing](04-testing/thread-safety-testing.md) - Concurrency testing
- [Test Coverage Targets](04-testing/test-coverage-targets.md) - Coverage goals

### API Documentation

Public interface reference:

- **[API Overview](05-api/api-overview.md)** - API documentation hub
- [API Conventions](05-api/conventions.md) - Design principles

#### Dia Engine APIs

- [DiaApplication API](05-api/dia/application-api.md) - Module/Phase/ProcessingUnit
- [DiaCore API](05-api/dia/core-api.md) - Containers, Type system, Time
- [DiaGraphics API](05-api/dia/graphics-api.md) - ICanvas, Frame
- [DiaMaths API](05-api/dia/maths-api.md) - Vector, Matrix, Transform, Shape
- [DiaInput API](05-api/dia/input-api.md) - Input events
- [DiaUI API](05-api/dia/ui-api.md) - UI integration
- [DiaWindow API](05-api/dia/window-api.md) - Window management
- [DiaIO API](05-api/dia/io-api.md) - File I/O
- [DiaPhysics API](05-api/dia/physics-api.md) - Physics simulation
- [DiaAI API](05-api/dia/ai-api.md) - AI systems
- [DiaSFML API](05-api/dia/sfml-api.md) - SFML backend

#### Cluiche Application APIs

- [MainProcessingUnit](05-api/cluiche/main-processing-unit.md) - Main thread API
- [RenderProcessingUnit](05-api/cluiche/render-processing-unit.md) - Render thread API
- [SimProcessingUnit](05-api/cluiche/sim-processing-unit.md) - Simulation thread API
- [Level Interface](05-api/cluiche/level-api.md) - Level system API
- [Module Catalog](05-api/cluiche/module-catalog.md) - Available modules

---

## AI-Optimized Guides

Documentation structured for AI agent comprehension:

- **[AI README](06-ai-guides/AI-README.md)** - AI entry point
- [Codebase Map](06-ai-guides/codebase-map.md) - Structured navigation
- [Entry Points](06-ai-guides/entry-points.md) - Common task starting points
- [Patterns Reference](06-ai-guides/patterns-reference.md) - Code patterns
- [System Boundaries](06-ai-guides/system-boundaries.md) - Responsibility matrix
- [Dependency Graph](06-ai-guides/dependency-graph.md) - Module dependencies
- [Thread Safety Guide](06-ai-guides/thread-safety-guide.md) - Concurrency patterns
- [Quick Reference](06-ai-guides/quick-reference.md) - Fast lookups

---

## Subsystem Deep Dives

Detailed exploration of major subsystems:

### DiaApplication
- [Overview](07-subsystems/dia-application/overview.md)
- [Module Lifecycle](07-subsystems/dia-application/module-lifecycle.md)
- [Phase Scheduling](07-subsystems/dia-application/phase-scheduling.md)

### DiaCore
- [Overview](07-subsystems/dia-core/overview.md)
- [Containers](07-subsystems/dia-core/containers.md)
- [Type System](07-subsystems/dia-core/type-system.md)

### DiaGraphics
- [Overview](07-subsystems/dia-graphics/overview.md)
- [Rendering Pipeline](07-subsystems/dia-graphics/rendering-pipeline.md)

### DiaMaths
- [Overview](07-subsystems/dia-maths/overview.md)
- [Known Issues](07-subsystems/dia-maths/known-issues.md)
- [Performance Notes](07-subsystems/dia-maths/performance-notes.md)

[More subsystems to be documented...]

---

## External Tools

Lightweight references to development tools:

- [Tools Overview](08-tools/tools-overview.md) - Available tooling
- [CLI Tool](08-tools/cli-tool.md) - MDK command-line interface
- [Console Tool](08-tools/console-tool.md) - Blue Console debugger
- [Dia Modules Script](08-tools/dia-modules-script.md) - Module analysis utility

---

## Development Guides

Development process and guidelines:

- [Contributing](09-development/contributing.md) - Contribution workflow
- [Coding Standards](09-development/coding-standards.md) - Code style guide
- [Visual Studio Guide](09-development/visual-studio-guide.md) - Project management
- [Debugging Tips](09-development/debugging-tips.md) - Debugging strategies
- [Known Issues](09-development/known-issues.md) - Current bugs and workarounds
- [Changelog](09-development/changelog.md) - Change history

---

## Reference Material

Catalog and reference information:

- [Module Registry](10-reference/module-registry.md) - All 56 module architecture files
- [Module Metadata Schema](10-reference/module-metadata-schema.md) - dia.module.v1 format
- [File Locations](10-reference/file-locations.md) - Where things live
- [External Links](10-reference/external-links.md) - Third-party documentation

---

## Working Files

Interim documentation tracking:

- [Documentation TODO](DOCUMENTATION_TODO.md) - Implementation progress tracker
- [Module Audit](MODULE_AUDIT.md) - Module file review status
- [API Documentation Template](API_DOCUMENTATION_TEMPLATE.md) - Template for API docs
- [Mermaid Diagram Sources](MERMAID_DIAGRAM_SOURCES.md) - Diagram tracking

---

## Documentation Status

**Last Updated:** 2026-03-31

**Phase:** Foundation (Phase 1 of 10)

**Progress:**
- ✅ Phase 1: Foundation - In Progress
- ⏳ Phase 2: Core Architecture - Not Started
- ⏳ Phase 3: Design Intent - Not Started
- ⏳ Phase 4: AI Guides - Not Started
- ⏳ Phases 5-10 - Planned

See [DOCUMENTATION_TODO.md](DOCUMENTATION_TODO.md) for detailed progress tracking.

---

## Documentation Conventions

- **⭐ PRIMARY** - Core documents requested by user
- **Bold links** - High-priority or entry-point documents
- Status indicators: ✅ Complete | 🚧 In Progress | ❌ Not Started | ⚠️ Blocked
- Priority: P0 (Critical) | P1 (High) | P2 (Medium) | P3 (Low)

---

## Getting Help

**Cannot find what you're looking for?**

- Check the [Module Registry](10-reference/module-registry.md) for subsystem-specific `.architecture.module.md` files
- See [AI Guides](06-ai-guides/AI-README.md) for structured navigation
- Review [File Locations](10-reference/file-locations.md) to understand directory organization