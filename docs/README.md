---
tags:
  - overview
---

# Cluiche Documentation

**Cluiche** is a game development platform built around the **Dia engine**.

<div class="home-cards" markdown>

| | | |
|:---:|:---:|:---:|
| **Learn** | **Build** | **Reference** |
| Understand architecture, design, and how the engine works. | Specs, development guides, and testing for active feature work. | Module registry, AI guides, API docs — look things up. |
| [Getting Started &rarr;](reference/getting-started/quickstart.md) | [Specs &rarr;](specs/README.md) | [Module Registry &rarr;](reference/registry/module-registry.md) |

</div>

---

## Quick Links

| | |
|---|---|
| **New to Cluiche?** | [Quickstart](reference/getting-started/quickstart.md) &rarr; [Architecture](reference/architecture/architecture.md) &rarr; [Design Philosophy](reference/design-rationale/design.md) |
| **Building a feature?** | [Specs Overview](specs/README.md) &rarr; [Contributing](reference/development/contributing.md) &rarr; [Coding Standards](reference/development/coding-standards.md) |
| **Debugging?** | [Debugging Tips](reference/development/debugging-tips.md) &middot; [Threading Model](reference/architecture/threading-model.md) &middot; [Known Issues](reference/development/known-issues.md) |
| **AI Agent?** | [AI Entry Point](reference/ai-guides/AI-README.md) &middot; [Codebase Map](reference/ai-guides/codebase-map.md) |

---

## Platform at a Glance

```mermaid
graph LR
    A[Dia Engine] --> B[CluicheTest]
    A --> C[CluicheEditor]
    A --> D[GoogleTests]
    A --> E[Future Games]
    
    style A fill:#89b4fa,color:#1e1e2e,stroke:#89b4fa
    style B fill:#a6e3a1,color:#1e1e2e,stroke:#a6e3a1
    style C fill:#f9e2af,color:#1e1e2e,stroke:#f9e2af
    style D fill:#94e2d5,color:#1e1e2e,stroke:#94e2d5
    style E fill:#cba6f7,color:#1e1e2e,stroke:#cba6f7
```

!!! tip "Backlog"
    Active work is tracked in the [Backlog](BACKLOG.md). Completed items in [History](BACKLOG-HISTORY.md).
