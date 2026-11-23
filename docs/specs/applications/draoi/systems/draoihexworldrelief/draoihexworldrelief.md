# System Spec: DraoiHexWorldRelief

## Parent Application
@docs/specs/applications/draoi/draoi.md

**Gameplay Domains:** rendering, world generation
**Research:** docs/research/tile_renderin_art_pipeline/summary.md (prototype: `lookdev/js/world.js`)
**Consumers:** DraoiHexWorldRender3D (textures, prop placement). Credible second: CPU code that needs the same relief as the drawn map (line of sight, movement cost by slope) in any hex strategy game.

**Origin (2026-11-23, user, layer split):** DiaCellFieldBake Feature 2 moved here. The Dia bake keeps everything generic (smooth fields, classes, margin, owner, parallel and incremental). Relief by terrain class, lake and sea elevation, and the river field are hex-terrain vocabulary.

## Purpose

DraoiHexWorldRelief adds hex-terrain shape to the generic DiaCellFieldBake output:
- **Relief:** extra height on mountain-like terrain classes, shaped by ridged noise, so ranges read as crags at 3D tilt (TMAP-004);
- **Elevation composition:** flat lakes at their surface height, sea falling away below zero;
- **River field:** wiggled segments from the hex river network, rasterised to a per-texel signed distance, width and flow direction.

It runs on the CPU with no GPU code, so the drawn map and CPU queries agree. It is a plain library (PD-012). Relief weights per class are game data (TMAP-012).

**Dependency chain:**
`DraoiHexWorldRelief → DraoiHexWorld (hex grid, neighbour table), DiaCellFieldBake (scalar layers, WarpedPoint, ITilePostPass), DiaObservation, DiaCore`

## Responsibilities

- Relief weight pre-pass and ridged relief post-pass.
- `ComposeElevation` (sea depth, lake surface pass-through).
- `RiverNetwork` built once per world, river rasterisation per tile, RGBA8 packing.
- Riding the Dia bake's parallel and incremental paths through `ITilePostPass`, with the right influence radius and double-buffered river array.

## Non-Responsibilities

- Smoothing, classes, margin, owner, scheduling (DiaCellFieldBake).
- River routing and flow (S-1 / DraoiHexWorldTerrain, A5 Hydrology). Flow state is a render channel.
- Drawing (DraoiHexWorldRender3D).

## Features

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| 1 | Relief and Rivers | Relief weights as a bake scalar, ridged relief post-pass, `ComposeElevation`, river network, river distance/width/direction field, tile and rebake safe | [relief-and-rivers.md](relief-and-rivers.md) | Draft |

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| HWR-001 | Relief and rivers are a Draoi package over the Dia bake, not part of it (user 2026-11-23) | Accepted | Yes |
| HWR-002 | Relief and rivers run only through DiaCellFieldBake's generic hooks (a scalar layer, `WarpedPoint`, `ITilePostPass`); no Draoi code inside the Dia loop | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains DraoiHexWorldRelief |
|---|---|---|
| PD-011 to PD-014 | Draoi layer rules | Plain library, scheduler-agnostic, depends down only |
| CFB-002, CFB-003 | Majority + margin borders; deterministic and hashable | Same warped point as the class decision; post-pass output hashed |
| TMAP-004 | Mountains are terrain relief | This package is where relief comes from |
| AD-003 | Namespaces | `Draoi::HexWorldRelief::` |

## Open Design Questions

1. **Admission.** CoW is the only consumer today. The credible second is any hex map that wants relief. If that proves thin, the alternative is folding this into DraoiHexWorldRender3D as a feature. Kept separate for now so CPU queries do not pull in rendering.

## Status

`Approved` (2026-11-23, user). Created by the layer split. Feature 1 Draft.
