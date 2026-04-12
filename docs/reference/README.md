# Reference Documentation

This directory contains reference documentation for the **existing Cluiche codebase** - understanding how the current system works, its architecture, APIs, and design rationale.

---

## 📚 Quick Navigation

### Getting Started
**New to Cluiche?** Start here:
- [5-Minute Quickstart](getting-started/quickstart.md) - Essential concepts and orientation
- [Building the Project](getting-started/building-the-project.md) - Get up and running
- [Common Tasks](getting-started/common-tasks.md) - Everyday developer workflows
- [Glossary](getting-started/glossary.md) - Terms and acronyms

### Architecture
**How the system is built:**
- [Architecture Overview](architecture/architecture.md) ⭐ - Complete system architecture
- [Dia Engine Architecture](architecture/dia-engine.md) - Engine subsystems
- [Threading Model](architecture/threading-model.md) - Main/Render/Sim threads
- [Module System](architecture/module-system.md) - Module/Phase/ProcessingUnit pattern
- [Level System](architecture/level-system.md) - Level loading and lifecycle

### Design Rationale
**Why things are the way they are:**
- [Design Philosophy](design-rationale/design.md) ⭐ - Core design intent
- [Why Dia Engine?](design-rationale/why-dia.md) - Engine design decisions
- [Why Module/Phase/PU?](design-rationale/why-module-phase-pu.md) - Threading architecture
- [Design Patterns](design-rationale/design-patterns.md) - Common patterns used

### Requirements (As-Built)
**What the current system does:**
- [Requirements Overview](requirements-as-built/requirements.md) ⭐ - Complete requirements
- [Functional Requirements](requirements-as-built/functional-requirements.md)
- [Non-Functional Requirements](requirements-as-built/non-functional-requirements.md)
- [Traceability Matrix](requirements-as-built/traceability-matrix.md)

### API Reference
**Public interfaces:**
- [API Overview](api/api-overview.md) - API documentation hub
- [Dia Engine APIs](api/dia/) - All engine subsystem APIs
- [Cluiche Application APIs](api/cluiche/) - Application-level APIs

### Testing
**How to verify correctness:**
- [Testing Strategy](testing/test.md) ⭐ - Complete testing approach
- [Unit Testing](testing/unit-testing.md)
- [Integration Testing](testing/integration-testing.md)
- [Performance Testing](testing/performance-testing.md)
- [Thread Safety Testing](testing/thread-safety-testing.md)

### AI Guides
**AI-optimized documentation:**
- [AI README](ai-guides/AI-README.md) - AI entry point
- [Codebase Map](ai-guides/codebase-map.md) - Structured navigation
- [Entry Points](ai-guides/entry-points.md) - Common task starting points
- [Patterns Reference](ai-guides/patterns-reference.md) - Code patterns

### Subsystems
**Deep technical dives:**
- [DiaMaths Deep Dive](subsystems/dia-maths/)
- [More subsystems...](subsystems/)

### Development
**Development process:**
- [Contributing](development/contributing.md) - Contribution workflow
- [Coding Standards](development/coding-standards.md) - Code style guide
- [Visual Studio Guide](development/visual-studio-guide.md) - Project management
- [Debugging Tips](development/debugging-tips.md) - Debugging strategies
- [Known Issues](development/known-issues.md) - Current bugs and workarounds

### Tools
**Development tooling:**
- [Tools Overview](tools/tools-overview.md)
- [CLI Tool](tools/cli-tool.md)
- [Console Tool](tools/console-tool.md)
- [Dia Modules Script](tools/dia-modules-script.md)

### Registry
**Module catalog and schemas:**
- [Module Registry](registry/module-registry.md) - All 56 module architecture files
- [Module Metadata Schema](registry/module-metadata-schema.md) - dia.module.v1 format
- [File Locations](registry/file-locations.md) - Where things live
- [External Links](registry/external-links.md) - Third-party documentation

---

## 📁 Looking for Specs?

This is **reference documentation** for understanding the existing codebase.

If you're looking to plan new features or systems, see **[`docs/specs/`](../specs/)** for the spec-driven development workflow.

---

## Documentation Conventions

- **⭐** - Core/primary documents
- **Bold links** - High-priority or entry-point documents
- Status: ✅ Complete | 🚧 In Progress | ❌ Not Started | ⚠️ Blocked
- Priority: P0 (Critical) | P1 (High) | P2 (Medium) | P3 (Low)
