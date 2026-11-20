# Research: Evaluate — Tile Rendering & AI-Only Art Pipeline (V-1)

**Input:** docs/research/tile_renderin_art_pipeline/ideate.md

## Scoring Criteria

**Deviation from the standard rubric (user request, 2026-11-20):** a sixth axis, **Art Feasibility**, scores how well a candidate works with **no artist and a limited AI budget**. It carries the largest weight, and the other weights are reduced to make room.

| Axis | Weight | Meaning (5 = best) |
|---|---|---|
| **Art Feasibility** | 0.30 | Few AI images to produce; low consistency risk across the set; tolerant of a weak image (style held elsewhere); regenerable; no manual painting |
| Engine Value | 0.15 | Reusable Dia capability any game could use |
| Game Value | 0.15 | Makes CoW's map read the sim (all channels) and look like the reference |
| Implementation Cost | 0.15 | Inverse of engineering effort |
| Risk | 0.15 | Inverse of uncertainty (technical and look) |
| Cluiche Fit | 0.10 | Module structure, PD-001..007, layer boundary, existing Dia packages, RB-001 |

Total = Art×0.30 + Engine×0.15 + Game×0.15 + Cost×0.15 + Risk×0.15 + Fit×0.10

## AI Budget Reality Check

The numbers below are counted in **accepted images** and **generations**, not money (pricing varies by Gemini plan). Rule of thumb: 3–5 generations per accepted image for textures, 5–10 for sprites that must match a set.

| Route | Accepted AI images | Est. generations | Consistency difficulty |
|---|---|---|---|
| C9 Stamped tiles (GDD as-is) | ~183 (180 tiles + weather) | ~900–1,800 | **Very high.** 180 images must match each other, tile seamlessly on hex edges and keep exact state/season deltas. Effectively impossible to hold by prompting |
| C10 AI-painted macro map | 1 per baked world (×3 sizes) | ~10–30 per world | High. Gemini output (~1–2K px) is smaller than the map, so it must be tiled (seams). The model moves rivers and coasts, so it drifts from the data |
| C1/C2 with AI ground + AI props | ~10 ground + ~40–60 props | ~250–550 | Medium. Ground textures are style-anchored and post-processed; props are the hard part |
| C1/C2 with **procedural ground (C3)** + AI props | ~40–60 props | ~200–450 | Medium (props only) |
| C1/C2 with procedural ground + **grayscale/mask props** (one sprite recoloured per biome and season by shader) | ~15–25 props | ~100–200 | Low–medium |
| … plus **Claude-authored SVG props** (Hilda's flat fills and bold outlines suit vector) | 0 Gemini; Claude iterations only | ~0 image gens | Low. Deterministic, editable, rasterised by a free tool (resvg/Inkscape CLI) |
| C5 ink, C7 weather/water, C3 ground | 0 | 0 | None. Code only |
| C8 spike | ~10–20 | ~40–80 | Low. Small set; this is the experiment |

**Takeaway:**
- **Every channel the GDD needs can be shown with zero AI images.** Season, depth zone, ecology health, weather, water, snow and succession are parameters, not art.
- **AI is needed only for "flavour":** ground detail and props.
- **Cut the budget by having code own the style.** Have the shader hold outlines, hatching, grain and palette, and author props as palette-masked sprites (Claude SVG first, Gemini only where SVG falls flat). That takes the AI budget from roughly 1,000+ generations to roughly 100–200, or near zero.

## Scores

**Rescored 2026-11-20 (user direction):** a missing Dia capability is **not** a cost or risk. It becomes a Dia feature to build. Cost and Risk now count only non-engine effort and **look/art** uncertainty.

| Candidate | Art (0.30) | Engine (0.15) | Game (0.15) | Cost (0.15) | Risk (0.15) | Fit (0.10) | Total |
|---|---|---|---|---|---|---|---|
| C5 Data-Driven Ink Pass | 5 | 4 | 5 | 4 | 3 | 4 | **4.30** |
| C7 Weather & Water Layer | 5 | 4 | 5 | 4 | 3 | 4 | **4.30** |
| C4 AI Asset Forge | 5 | 4 | 4 | 4 | 3 | 4 | **4.15** |
| C8 Look-Dev Spike | 5 | 2 | 3 | 5 | 5 | 4 | **4.15** |
| C3 Pure-Procedural Ground | 5 | 3 | 3 | 4 | 3 | 5 | **3.95** |
| C1 Illustrated 2D Field | 3 | 4 | 4 | 4 | 4 | 5 | **3.80** |
| C2 3D Hex Heightfield | 3 | 5 | 5 | 3 | 3 | 4 | **3.70** |
| C6 Prop Scatter | 3 | 4 | 5 | 4 | 3 | 4 | **3.70** |

*Original scores, which penalised missing engine features: C7 4.00, C1 3.65, C2 3.40, C6 3.55.*

**C1 vs C2 is now a near tie on paper.** C2's remaining risk is about the look:
- far cells hidden behind peaks under perspective;
- AI ground textures having to hold up under lighting.

C1's remaining limit is fixed or faked perspective. The spike decides.
| C10 AI-Painted Macro Map | 2 | 1 | 3 | 3 | 1 | 2 | **2.00** |
| C9 AI Stamped Tiles | 1 | 2 | 2 | 3 | 1 | 3 | **1.80** |

Rationale for key scores:
- **C9 scores Art 1 and Risk 1.** 180 matching, hex-seamless AI tiles with precise state and season deltas is the single hardest thing to get from an image model. It still shows the repetition you called out ("early Civ").
- **C10 scores Risk 1.** The painting does not respect the data (the model relocates rivers and coasts), it cannot update weekly, and the map is larger than one generation. It is useful only as optional far-zoom flavour.
- **C1 vs C2:**
  - *Art feasibility is the same for both.* Both need the same assets.
  - *What C2 adds:* real perspective and height, and mountains from geometry (fewer peak sprites).
  - *What C2 costs:* the most engine work (no terrain, instancing or texture arrays in Dia), and AI ground textures must hold up under perspective and lighting.
  - *What C1 builds on:* a renderer that already does per-cell GPU data, blending and hillshade. The reference image itself is a 2D oblique illustration.
- **C6 scores Art 3.** Props are where AI consistency bites. Mitigation: palette-masked grayscale sprites and Claude SVG. With both, Art rises to about 4.

## Top 3 Candidates

### Rank 1: C5 Data-Driven Ink Pass (score: 4.30)
**Why:** This is the answer to "no artist". The Hilda look (outlines, hatch density by depth zone, outline weight 1×/1.2×/1.5×, paper grain, season grade and blend modes) becomes code driven by data the sim already has. Every AI image is then pulled into one consistent style, so a weak image no longer breaks the map. It is also how depth zone and season are expressed, at zero art cost.
**Watch out for:** naive screen-space edge detection looks like a "filter". Outlines must come from data boundaries (coast, biome border, cliff slope, river bank). The season grade and paper grain may want a full-screen pass, which RB-001 currently rules out, so that needs revisiting (AD-001).

### Rank 2: C8 Look-Dev Spike (score: 4.15, tie with C4)
**Why:** It is one week and about 40–80 generations, and it answers the three open questions with evidence instead of guesses: 2D oblique vs 3D heightfield, procedural vs AI ground, and SVG vs Gemini props. It costs almost nothing in AI budget and protects the L/XL investment that follows.
**Watch out for:** spike code must stay throwaway (no Dia headers without sign-off). Judge against *Strategic Map Overview* at strategic zoom, not at close zoom.

### Rank 3: C4 AI Asset Forge (score: 4.15)
**Why:**
- *Budget:* it is the budget multiplier. Prompts are generated from Visual Bible tokens and anchored on the existing reference PNGs, which cuts retries.
- *Automatic post-processing:* code-enforced seamless edges, palette lock, background removal, mips and packing replace the manual cleanup an artist would do.
- *Regenerable:* recording prompt, seed and model makes every asset reproducible.
- *Reusable:* it is engine-side tooling (DiaCLI handler) any Dia game can use.
**Watch out for:** generation stays a manual Gemini step (no API key in the build), so the forge automates everything *after* the image exists. Palette lock can flatten painterly detail; tune the strength per asset type.

## Recommendation

**Treat V-1 as one system, built in this order. Do not build the GDD's stamped-tile pipeline (C9).**

**System name:** "Data-Driven Illustrated Map".

1. **C8 spike first.** Decide 2D vs 3D and the ground source on evidence.
2. **C5 ink pass and C3 procedural ground.** These carry the style and every data channel with zero art.
3. **C7 weather and water.**
4. **C6 props.** Use palette-masked sprites, Claude SVG first and Gemini where needed, fed through **C4 forge**.

**Map approach (C1 vs C2), after the rescore:** these are an even pair, and the spike decides.
- **C1 (2D field)** matches the 2D reference and extends `CellFieldRenderer`.
- **C2 (3D heightfield)** gives the true perspective and height the user wants for grandeur.
- **Rule:** missing Dia features count as work to build, never as reasons against either one.

**Budget:** with this design, AI images are needed only for props and optional ground detail, **about 100–200 generations total, not 1,000+**. Every gameplay channel is still on the map.

**Constraints:**
- Fits PD-002: the render module reads sim state via FrameStream.
- Fits PD-004.
- New Dia pieces need AD-001 sign-off: texture arrays, batching or instancing, particles, a possible post-process pass (RB-001), and a game-side shader root.
