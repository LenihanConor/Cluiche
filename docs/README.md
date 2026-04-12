# Cluiche Documentation

Welcome to the comprehensive documentation for the Cluiche game framework and Dia engine.

---

## 📁 Documentation Structure

This documentation is organized into two main sections:

### [`specs/`](specs/) - Spec-Driven Development Workflow
**Planning and building new features** using a structured 4-level methodology:
- **Platform** → **Application** → **System** → **Feature**
- Decision cascading with binding constraints
- AI-assisted review and validation
- Traceability from platform to implementation

*Created via `setup-specs.sh` - run this script to initialize the spec structure.*

### [`reference/`](reference/) - Reference Documentation
**Understanding the existing codebase:**
- Architecture, API documentation, design rationale
- Testing strategies and development guides
- AI-optimized navigation and code patterns
- Module registry and schemas

**Quick rule:** Building something new? → Start in `specs/`. Understanding existing code? → Start in `reference/`.

---

## Quick Navigation

### New to Cluiche?

Start here to get oriented:

- **[5-Minute Quickstart](reference/getting-started/quickstart.md)** - Essential concepts and orientation
- **[Building the Project](reference/getting-started/building-the-project.md)** - Get up and running
- **[Common Tasks](reference/getting-started/common-tasks.md)** - Everyday developer workflows
- **[Glossary](reference/getting-started/glossary.md)** - Terms and acronyms

### For AI Agents

Optimized documentation for AI codebase navigation:

- **[AI Entry Point](reference/ai-guides/AI-README.md)** - Start here for AI-optimized navigation
- **[Codebase Map](reference/ai-guides/codebase-map.md)** - Structured directory and file navigation
- **[Entry Points](reference/ai-guides/entry-points.md)** - Where to start for common tasks
- **[Patterns Reference](reference/ai-guides/patterns-reference.md)** - Code patterns with examples

---

## Core Documentation

### Architecture

Understanding how the system is built:

- **[Architecture Overview](reference/architecture/architecture.md)** ⭐ PRIMARY - Complete system architecture
- [Cluiche Application Architecture](reference/architecture/cluiche-application.md) - Application layer details
- [Dia Engine Architecture](reference/architecture/dia-engine.md) - Engine subsystems
- [Threading Model](reference/architecture/threading-model.md) - Main/Render/Sim thread design
- [Module System](reference/architecture/module-system.md) - Module/Phase/ProcessingUnit pattern
- [Level System](reference/architecture/level-system.md) - Level loading and lifecycle
- [External Dependencies](reference/architecture/external-dependencies.md) - Third-party integration
- [Architecture Diagrams](reference/architecture/diagrams/) - Mermaid diagrams

### Design

Understanding why things are the way they are:

- **[Design Philosophy](reference/design-rationale/design.md)** ⭐ PRIMARY - Core design intent and rationale
- [Why Dia Engine?](reference/design-rationale/why-dia.md) - Engine design decisions
- [Why Module/Phase/PU?](reference/design-rationale/why-module-phase-pu.md) - Threading architecture rationale
- [Why Type System?](reference/design-rationale/why-type-system.md) - Reflection system benefits
- [Design Patterns](reference/design-rationale/design-patterns.md) - Common patterns used
- [Historical Decisions](reference/design-rationale/historical-decisions.md) - Legacy choices and context
- [Future Directions](reference/design-rationale/future-directions.md) - Planned evolution

### Requirements

What the system must do:

- **[Requirements Overview](reference/requirements-as-built/requirements.md)** ⭐ PRIMARY - Complete requirements checklist
- [Functional Requirements](reference/requirements-as-built/functional-requirements.md) - What it does
- [Non-Functional Requirements](reference/requirements-as-built/non-functional-requirements.md) - How well it does it
- [Cluiche Requirements](reference/requirements-as-built/cluiche-requirements.md) - Application-specific
- [Dia Requirements](reference/requirements-as-built/dia-requirements.md) - Engine-specific
- [Traceability Matrix](reference/requirements-as-built/traceability-matrix.md) - Requirements → Implementation

### Testing

How to verify correctness:

- **[Testing Strategy](reference/testing/test.md)** ⭐ PRIMARY - Complete testing approach
- [Unit Testing](reference/testing/unit-testing.md) - Unit test guidelines
- [Integration Testing](reference/testing/integration-testing.md) - Cross-system testing
- [Performance Testing](reference/testing/performance-testing.md) - Performance validation
- [Thread Safety Testing](reference/testing/thread-safety-testing.md) - Concurrency testing
- [Test Coverage Targets](reference/testing/test-coverage-targets.md) - Coverage goals

### API Documentation

Public interface reference:

- **[API Overview](reference/api/api-overview.md)** - API documentation hub
- [API Conventions](reference/api/conventions.md) - Design principles

#### Dia Engine APIs

- [DiaApplication API](reference/api/dia/application-api.md) - Module/Phase/ProcessingUnit
- [DiaCore API](reference/api/dia/core-api.md) - Containers, Type system, Time
- [DiaGraphics API](reference/api/dia/graphics-api.md) - ICanvas, Frame
- [DiaMaths API](reference/api/dia/maths-api.md) - Vector, Matrix, Transform, Shape
- [DiaInput API](reference/api/dia/input-api.md) - Input events
- [DiaUI API](reference/api/dia/ui-api.md) - UI integration
- [DiaWindow API](reference/api/dia/window-api.md) - Window management
- [DiaIO API](reference/api/dia/io-api.md) - File I/O
- [DiaPhysics API](reference/api/dia/physics-api.md) - Physics simulation
- [DiaAI API](reference/api/dia/ai-api.md) - AI systems
- [DiaSFML API](reference/api/dia/sfml-api.md) - SFML backend

#### Cluiche Application APIs

- [MainProcessingUnit](reference/api/cluiche/main-processing-unit.md) - Main thread API
- [RenderProcessingUnit](reference/api/cluiche/render-processing-unit.md) - Render thread API
- [SimProcessingUnit](reference/api/cluiche/sim-processing-unit.md) - Simulation thread API
- [Level Interface](reference/api/cluiche/level-api.md) - Level system API
- [Module Catalog](reference/api/cluiche/module-catalog.md) - Available modules

---

## AI-Optimized Guides

Documentation structured for AI agent comprehension:

- **[AI README](reference/ai-guides/AI-README.md)** - AI entry point
- [Codebase Map](reference/ai-guides/codebase-map.md) - Structured navigation
- [Entry Points](reference/ai-guides/entry-points.md) - Common task starting points
- [Patterns Reference](reference/ai-guides/patterns-reference.md) - Code patterns
- [System Boundaries](reference/ai-guides/system-boundaries.md) - Responsibility matrix
- [Dependency Graph](reference/ai-guides/dependency-graph.md) - Module dependencies
- [Thread Safety Guide](reference/ai-guides/thread-safety-guide.md) - Concurrency patterns
- [Quick Reference](reference/ai-guides/quick-reference.md) - Fast lookups

---

## Subsystem Deep Dives

Detailed exploration of major subsystems:

### DiaApplication
- [Overview](reference/subsystems/dia-application/overview.md)
- [Module Lifecycle](reference/subsystems/dia-application/module-lifecycle.md)
- [Phase Scheduling](reference/subsystems/dia-application/phase-scheduling.md)

### DiaCore
- [Overview](reference/subsystems/dia-core/overview.md)
- [Containers](reference/subsystems/dia-core/containers.md)
- [Type System](reference/subsystems/dia-core/type-system.md)

### DiaGraphics
- [Overview](reference/subsystems/dia-graphics/overview.md)
- [Rendering Pipeline](reference/subsystems/dia-graphics/rendering-pipeline.md)

### DiaMaths
- [Overview](reference/subsystems/dia-maths/overview.md)
- [Known Issues](reference/subsystems/dia-maths/known-issues.md)
- [Performance Notes](reference/subsystems/dia-maths/performance-notes.md)

[More subsystems to be documented...]

---

## External Tools

Lightweight references to development tools:

- [Tools Overview](reference/tools/tools-overview.md) - Available tooling
- [CLI Tool](reference/tools/cli-tool.md) - MDK command-line interface
- [Console Tool](reference/tools/console-tool.md) - Blue Console debugger
- [Dia Modules Script](reference/tools/dia-modules-script.md) - Module analysis utility

---

## Development Guides

Development process and guidelines:

- [Contributing](reference/development/contributing.md) - Contribution workflow
- [Coding Standards](reference/development/coding-standards.md) - Code style guide
- [Visual Studio Guide](reference/development/visual-studio-guide.md) - Project management
- [Debugging Tips](reference/development/debugging-tips.md) - Debugging strategies
- [Known Issues](reference/development/known-issues.md) - Current bugs and workarounds
- [Changelog](reference/development/changelog.md) - Change history

---

## Reference Material

Catalog and reference information:

- [Module Registry](reference/registry/module-registry.md) - All 56 module architecture files
- [Module Metadata Schema](reference/registry/module-metadata-schema.md) - dia.module.v1 format
- [File Locations](reference/registry/file-locations.md) - Where things live
- [External Links](reference/registry/external-links.md) - Third-party documentation

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

- Check the [Module Registry](reference/registry/module-registry.md) for subsystem-specific `.architecture.module.md` files
- See [AI Guides](reference/ai-guides/AI-README.md) for structured navigation
- Review [File Locations](reference/registry/file-locations.md) to understand directory organization