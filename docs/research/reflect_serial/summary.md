# Research Summary: Reflection & Serialization

**Date:** 2026-05-20 → 2026-05-21
**Folder:** docs/research/reflect_serial/
**Status:** Complete

## Question

What architecture should Cluiche adopt for serializing runtime game-object state (rigid bodies, entities, component graphs, animation states) — distinct from the existing config/asset serialization?

## Answer

**C7: Archive pattern** — a `serialize(Archive& ar, T& obj, unsigned version)` free function per type, with swappable archive implementations (JSON read/write, binary read/write). **Registration syntax** uses macros as a thin DSL (`DIA_SERIALIZE` / `DIA_FIELD`) that expand to the free function — cleaner call sites, no global-init side effects, with an escape hatch to raw free functions for types needing custom version migration.

## Key findings

1. The existing macro system (DIA_TYPE_DECLARATION/DEFINITION) is adequate for config loading but structurally unsuitable for state serialization — no pointers, no binary, no versioning.
2. Eight candidates were evaluated across five axes (Engine Value, Game Value, Implementation Cost, Risk, Cluiche Fit). C7 scored highest (3.95/5.0).
3. The archive pattern's defining advantage is **native versioning** — the version parameter is in the function signature, migration is `if (version < N)` guards. Every other candidate requires bolting versioning on after the fact.
4. MSVC constraints eliminated constexpr-heavy approaches (C4, parts of C8) that would rank higher on GCC/Clang.
5. Two structural overlays (auto-aggregate for trivial types, describe-style for plain-data types) are compatible with C7 and can reduce boilerplate for the simplest types.

## Deliverables

| Stage | File |
|-------|------|
| Explore | [explore.md](explore.md) |
| Ideate | [ideate.md](ideate.md) |
| Evaluate | [evaluate.md](evaluate.md) |
| Choose | [choose.md](choose.md) |
| Summary | this file |

## Implications

- A new system spec is needed (DiaReflect or DiaSerialize — naming TBD)
- The existing DiaCore/Type system stays for config/asset loading
- The new system is additive — types opt in one at a time
- First implementation targets: DiaMaths primitives (Vector2D, Matrix), DiaRigidBody2D state, simple component graphs
