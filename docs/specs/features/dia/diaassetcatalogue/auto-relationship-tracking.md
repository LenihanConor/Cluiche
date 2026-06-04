---
status: Done
parent: docs/specs/systems/dia/diaassetcatalogue.md
---

# Auto-Relationship Tracking

**Parent:** [DiaAssetCatalogue System](../../systems/dia/diaassetcatalogue.md)

## Summary

Asset dependencies (e.g. a scene referencing an entity blueprint) are currently managed manually in the catalogue. This feature adds two complementary mechanisms: (1) editors register relationships in real-time as assets are placed/removed, and (2) a scan-based inferrer reconciles the relationship graph on-demand from file contents.

## Goals

- Eliminate manual relationship management for cross-asset references
- Keep the relationship graph accurate with respect to what's actually in the files
- Provide a fallback for cases where editors miss an update (manual file edits, bugs)

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | DiaSceneEditor calls `add_relationship(scene, "uses", blueprint)` when placing an entity/camera/light, and `remove_relationship` when removing one |
| AC2 | A scan-based inferrer can be run on-demand (via Validate or dedicated button) to scan `.diascene` files and reconcile missing/stale edges |
| AC3 | Inferrer re-adds edges even if previously manually deleted — file content is source of truth |
| AC4 | Duplicate edges are not created (idempotent — `add_relationship` already handles this) |
| AC5 | Relationship type is `uses` for all scene→blueprint references |

## Binding Decisions

- **SD-CAT-012** (relationships via edges, not embedded fields): Compliant — we use the existing `add_relationship`/`remove_relationship` handlers, not embedded fields in records.

## Open Design Questions

1. **Inferrer scope expansion** — When other editors gain cross-asset references (e.g. DiaEntityTemplateEditor referencing textures), the inferrer will need per-type parsers. Should the inferrer architecture be pluggable from the start, or hardcoded to `.diascene` for v1?
2. **Stale edge cleanup** — If an entity is removed from a scene, the editor calls `remove_relationship`. But the inferrer only *adds* missing edges — should it also *remove* edges that no longer exist in the file? (This would make reconciliation bi-directional.)
