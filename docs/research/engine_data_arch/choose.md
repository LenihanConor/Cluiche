# Research: Choice — Engine Data Architecture Foundation

**Date:** 2026-04-27
**Chosen candidate:** Cut C — End-to-End (all 8 candidates)

## Rationale

The user wants the complete three-layer system delivered: authoring (editor explorer), build (pipeline + validation), and runtime (stage & bundle loader). The reasoning is straightforward — a small AI-developed game needs the full loop working before game content gets built. Building foundation and pipeline without the runtime loader or editor means content can't be loaded in-game or browsed visually, which limits the practical value of the earlier layers.

Cut B was recommended on cost/risk grounds, but the user values having the complete authoring→build→runtime workflow in place before starting game development.

## What Was Ruled Out

| Option | Reason not chosen |
|--------|------------------|
| Cut A — Foundation Only (8+2+1) | No validation gate, no pipeline, no runtime loading. Too minimal to be useful before game development starts. |
| Cut B — Foundation + Build (8+2+1+6+4) | Missing runtime loader and editor — can build assets but can't load them in-game or browse them in the editor. Incomplete loop. |

## Pre-Spec Commitments

- **Three-layer architecture is the organizing principle:** Authoring (Editor), Build (Pipeline), Runtime (Loading). All specs should map to these layers.
- **Stage is the hard game transition boundary.** A Stage owns systems and loads Bundles. Bundles reference assets and can contain other Bundles.
- **Asset taxonomy (settled):** Texture, Mesh/Sprite, Audio, Config, Entity Definition, Stage, UI Definition, Bundle, UI Bundle
- **An asset** = authored content with a stable identity that goes through the pipeline and gets loaded at runtime
- **Validation is mandatory at the build boundary.** AI-authored content must pass schema + referential integrity checks before reaching runtime.
- **Reverse dependencies are first-class.** "What uses this?" must be answerable in the editor.
- **Build pipeline must be transparent and debuggable.** Structured logging (NDJSON) so you can trace what happened and why.
- **Content hashing from the start.** Design for incremental builds even if the logic comes later.
- **StringCRC identity** (PD-001), **DiaCore containers** in public APIs (PD-004), **ProcessingUnit/Phase/Module** for pipeline structure (PD-002), **Component system** awareness in entity definitions (PD-003)

## Build Order

| Order | Candidate | Size | Layer |
|-------|-----------|------|-------|
| 1 | JSON Definition Loader | S | Utility |
| 2 | Asset Type Framework | S-M | Backbone |
| 3 | Identity & Relationship Backbone | M | Backbone |
| 4 | Validation Engine | S-M | Build + Authoring |
| 5 | Build Pipeline Core | M-L | Build |
| 6 | Stage & Bundle Runtime Loader | M | Runtime |
| 7 | Editor Asset Explorer | M | Authoring |
| 8 | Build Debugger & Log Viewer | S | Build tooling |

**Estimated total: 8-14 weeks**

## Next Step

This is a system-level scope (8 candidates, multiple modules). Run `/spec-system` to create the system spec, with each candidate becoming a child feature spec.

Suggested home: New `DiaData` module (Dia/DiaData/) as the primary module, with DiaAPI commands for pipeline tooling and a DiaEditor plugin for the explorer.
