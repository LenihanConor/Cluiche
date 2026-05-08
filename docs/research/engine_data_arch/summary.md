# Research Summary — Engine Data Architecture Foundation

**Session folder:** docs/research/engine_data_arch/
**Date:** 2026-04-27

## One-Line Answer

Build a complete end-to-end data architecture for Dia — identity backbone, validated build pipeline, stage-based runtime loading, and editor explorer — as a new `DiaData` module with 8 deliverables across three layers.

## Journey

1. **Explored:** Surveyed the full 12-subsystem reference architecture against Dia's existing infrastructure. Found that TypeSystem, StringCRC, PathStore, DiaAPI, and ProcessingUnit are production-ready and map directly to data architecture needs. The gap is that none are wired together into a unified data lifecycle.
2. **Ideated:** Initial pass generated 10 candidates sliced by subsystem. After discussion, reframed around three workflow layers (Authoring, Build, Runtime) with a shared backbone. Settled on asset taxonomy (Texture, Mesh/Sprite, Audio, Config, Entity Definition, Stage, UI Definition, Bundle, UI Bundle) and "Stage" as the hard game-transition boundary. Produced 8 candidates forming a clear dependency chain.
3. **Evaluated:** Scored candidates individually and as three cut points (A: foundation only, B: foundation + build, C: end-to-end). Cut B scored highest on cost/risk, Cut C scored lowest but delivers the complete authoring→build→runtime loop.
4. **Chose:** Cut C — the full end-to-end system. User wants the complete loop working before game development starts. 8-14 week estimated scope.

## Chosen Work Item

**Name:** Engine Data Architecture — End-to-End
**Home module:** New `DiaData` module (Dia/DiaData/), with DiaAPI commands and a DiaEditor plugin
**Suggested spec type:** System (with 8 child feature specs)
**Estimated size:** XL (8-14 weeks)

## Key Insights from Exploration

- **Dia's TypeSystem + StringCRC + PathStore are the natural backbone.** Runtime RTTI, CRC-keyed identity, and path aliasing already solve the hardest parts of the identity and schema layers. The work is wiring them together, not building from scratch.
- **Validation matters more for AI-authored content.** AI makes structural mistakes (wrong field names, dangling refs, out-of-range values). The build pipeline's validation gate is the critical safety layer — it must reject bad data, not silently pass it through.
- **Reverse dependencies require a computed index.** Forward refs are stored on the record; reverse refs ("what uses this texture?") must be computed and maintained separately. This is architectural — it affects the registry's data model, not just a query layer.
- **Stage ≠ Bundle.** A Stage is a hard game transition that owns systems and loads Bundles. A Bundle is a grouping of assets (including nested Bundles). Keeping these distinct prevents conflating gameplay boundaries with data packaging.
- **UI Bundles are a distinct asset type.** UI has specific structure (layouts + styles + icons + fonts as a unit) that warrants its own validation rules, not just a generic Bundle.
- **Content hashing should be designed in from day one.** Even without incremental build logic, storing hashes at discover time is cheap insurance that avoids rework later.
- **The 8 candidates form a strict dependency chain (8→2→1→6→4→5→3→7).** This means they must be built bottom-up, but each layer is independently testable and shippable.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Cut A — Foundation Only | No validation gate, no pipeline. AI-authored content errors only caught at runtime. Incomplete for starting game development. |
| Cut B — Foundation + Build | Missing runtime loader and editor explorer. Can build assets but can't load them in-game or browse them visually. Incomplete loop. |

(All 8 individual candidates from the reframed ideation are included in Cut C — none were discarded.)

## References

- docs/research/engine_data_arch/explore.md
- docs/research/engine_data_arch/ideate.md
- docs/research/engine_data_arch/evaluate.md
- docs/research/engine_data_arch/choose.md
