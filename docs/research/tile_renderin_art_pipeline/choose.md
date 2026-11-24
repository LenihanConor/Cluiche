# Research: Choice — Tile Rendering & AI-Only Art Pipeline (V-1)

**Date:** 2026-11-20
**Chosen candidate:** Data-Driven Illustrated Map: C8 look-dev spike (2D vs 3D bake-off), then C5 ink pass + C3 procedural ground, then C7 weather and water, then C6 props via the C4 asset forge.

## Rationale

The user has no artist and a limited AI budget. This candidate moves the Hilda style (outlines, hatching, grain, palette, season grade, depth zone) and every simulation channel (terrain, biome, hydrology, ecology, succession, weather) into code driven by cell data. AI is used only for props and optional ground detail: about 100–200 Gemini generations at most, with Claude-authored SVG first. It scored highest on the Art Feasibility axis, which the user weighted most. It also replaces the GDD's 180 stamped tiles, which the user identified as producing the repetitive "early Civ" look.

## What Was Ruled Out

| Candidate | Reason not chosen |
|---|---|
| C9 AI Stamped Tiles (GDD pipeline, Gemini as artist) | About 180 matching, hex-seamless images; consistency cannot be held by prompting; repetition at zoom-out |
| C10 AI-Painted Macro Map | The model moves rivers and coasts away from the data; cannot update weekly; the map is larger than one generation |
| Spike only, decide later | User prefers to commit to the direction now; the spike still runs inside this research |
| Commit to 3D now | The user wants both looks compared; 2D is the safety net if 3D drifts from Hilda |

## Pre-Spec Commitments

1. **The prototype is part of this research.** A browser look-dev spike runs before `summary.md` is written, and its findings feed the summary and the spec. It is throwaway: it lives in `docs/research/tile_renderin_art_pipeline/lookdev/`, not in Dia or CoW code.
2. **Bake-off scope.** Only the base map geometry is either/or: 2D field (C1) vs 3D heightfield (C2). The ink pass, ground, weather and water, and props are shared and combined. Ground source (procedural vs AI detail) and prop source (SVG vs Gemini) are mixes, decided per asset type.
3. **The prototype uses real data.** It runs on a real S-1 Small world's cell layers, with sliders standing in for A1, A4 and A5 data until those systems exist.
4. **Missing Dia features are work to build, not reasons against.** Expected engine work, each needing AD-001 sign-off: texture arrays and mips, batched or instanced sprites, particles, a game-side shader root, a terrain mesh (if 3D), and a possible full-screen pass (RB-001 revisit).
5. **AI budget.** About 100–200 Gemini generations at most. Claude SVG is used first for props. Palette-masked greyscale props are recoloured by the shader, so one sprite serves many biomes and seasons.
6. **Every channel is shown.** Terrain, biome, hydrology (rivers, lakes, floods, ice), ecology, succession, weather (clouds, precipitation, snow pack, wind) and season. Soil and ground water stay debug-only, as the GDD says. Particles and animation are in scope.
7. **The GDD look stays as the acceptance test; the mechanism is reopened.** Fold-back text goes to the GDD session:
   - replace the tile pipeline;
   - settle the biome set, ecology state names and season names;
   - make snow a continuous pack, not a bool.

## Bake-Off Result (2026-11-20)

The user reviewed the `lookdev/` prototype:
- **3D heightfield chosen** over the 2D field.
- **Peak sprites dropped:** terrain relief carries the mountains.
- **Weather must be per cell, not global**, which matches GDD TileRenderingPipeline §7.2. The prototype now uses moving storm fronts writing per-cell cloud, precipitation, snow fraction and gust.

The user confirmed the direction ("as a direction this is amazing, let's go with it"). Details are in [summary.md](summary.md).

**Tooling ships with it (user, 2026-11-20).** The look-dev controls are part of what V-1 builds, not a throwaway. This follows the CoW testbed rule that the system owns its tools. The tools to keep:
- live style tuning: ink weight, hatch, toon, shade, grade strength, height scale, camera;
- per-channel data views (biome, elevation, health, depth zone, temperature, margin);
- stand-in drivers for ecology, water and weather until A1/A4/A5 exist, then switchable to live data;
- layer toggles;
- screenshots.

Every control is a DiaAPI command with the panel calling it. Tuned values persist as a style parameter file.

## Next Step

1. Run the look-dev spike (`lookdev/`), then write `summary.md` with its results.
2. Run `/spec-system` for V-1 (CoW testbed), plus the Dia system specs it needs.
   - **Suggested parents:** CoW V-1 Tile Rendering (testbed).
   - **Dia home** (decided by the bake-off): grow DiaBgfx `CellFieldRenderer` (2D), or a new `DiaTerrain3D` (3D). Plus DiaParticles, NPR shader functions, and a DiaCLI AI-asset handler.
