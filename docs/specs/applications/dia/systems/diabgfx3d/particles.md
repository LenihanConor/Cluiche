# Feature Spec: DiaBgfx3D Particles (Feature C)

## Parent System
@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md (Amendment 2026-11-20)

**Gameplay Domains:** rendering
**Depends on:** Instanced Sprites (B) for the camera-facing quad path
**Needed by:** DraoiHexWorldRender3D Map Props and Emitters (precipitation, smoke)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/weather.js` (`Precip`, `spawnSmoke`, `PRECIP_VS`/`PRECIP_FS`)

## Status
`Approved` (2026-11-24, user, after spec review: area-based spawn rate, `spawnAbsolute`, tweakable params with sidecar). Drafted 2026-11-20 from the approved prototype. **AD-001 signed off 2026-11-24 (user).**

## Summary

A small, deterministic, CPU-simulated particle system for world-space effects: rain streaks, snow flakes and smoke puffs. Spawning is **local**: the game hands over a density field, and particles appear only where the density is above zero, which is what makes weather per-cell (SM-003).

It is intentionally small: three kinds, a fixed budget, a fixed-step simulation and one draw per kind. It is not a general VFX system.

## Problem

- Weather and fire must be visible on the map: rain where a storm is, snow on cold cells, smoke above burning cells. Global screen-space particles (the first prototype) were wrong.
- Dia has no particle system. `Dia/DiaSoftBody2D/Particle.h` is a physics particle.
- D3D11 has no point-size output, so the prototype's `gl_PointSize` flakes can't be ported. All three kinds must be quads.

## Goals

1. Three kinds: **Streak** (velocity-aligned thin quad), **Flake** (disc with an ink ring) and **Puff** (soft radial blob that grows and fades).
2. **Density-driven spawning**: a world-space `DensityField` per kind; spawn points are sampled where density is above zero, accepted with probability equal to the density.
3. **Wind**: a uniform wind vector plus optional per-kind drift, applied each step.
4. **Ground collision** through an `IGroundQuery`, so particles die at terrain height.
5. A **fixed particle budget** and spawn budget per step; deterministic from a seed with a fixed simulation step.
6. Draws as camera-facing quads through Feature B's transient instance path, with depth test and no depth write.

## Non-Goals

- Emitter shapes, curves, trails, collisions between particles, GPU simulation, textures or sprite-sheet particles.
- Sorting, lighting, soft-particle depth fade.
- Sim state. A particle system lives on the render side and owns no gameplay state; saves do not include particles.

## Design

```cpp
namespace Dia::Bgfx3D {
    enum class ParticleKind : uint8_t { Streak, Flake, Puff };

    struct DensityField {                          // borrowed; row-major
        const float* values; uint32_t width, height;
        float originX, originY, cellW, cellH;      // world rect covered
        float Sample(float x, float y) const;      // nearest; 0 outside
    };
    class IGroundQuery { public: virtual ~IGroundQuery() {}
                         virtual float HeightAt(float x, float y) const = 0; };

    struct ParticleKindParams {
        uint32_t maxLive = 4096;  uint32_t spawnPerStep = 64;   // spawnPerStep is a per-step CAP
        float spawnRate = 8.0f;                    // particles per world unit^2 per second at density 1
        float spawnHeight[2] = {2.0f, 6.0f};       // min/max; above ground, or absolute altitude when spawnAbsolute
        bool  spawnAbsolute = false;               // true: spawnHeight is an altitude (rain from a cloud layer)
        float wobbleAmount = 0.15f;                // flake lateral wobble, world units (formerly "small")
        float fallSpeed = 9.0f;  float riseSpeed = 0.0f;   // flakes fall slowly; puffs rise
        float windScale = 1.0f;  float sizeStart = 0.05f, sizeEnd = 0.05f;
        float lifeMax = 8.0f;                      // seconds, upper bound; ground contact ends earlier
        uint32_t colourStart = 0xFFFFFFFF, colourEnd = 0x00FFFFFF;
        float streakLength = 0.45f;
    };

    class ParticleSystem {
    public:
        void Configure(ParticleKind, const ParticleKindParams&);
        void SetDensity(ParticleKind, const DensityField&);   // null values = none
        void SetWind(float x, float y);                        // world units per second
        void SetGround(const IGroundQuery*);
        void SetSpawnRect(float x0, float y0, float x1, float y1);   // the camera footprint plus margin; default: the density rect
        void SetSeed(uint32_t);
        void Update(float dtSeconds);                          // fixed internal step 1/60, accumulates
        void Draw(const Camera3D&, float timeSeconds);
        uint32_t LiveCount(ParticleKind) const;
    };
}
```

- **Step.** `Update` accumulates `dt` and runs fixed `1/60 s` steps (at most 5 per call). Each step per kind: spawn up to `spawnPerStep` candidates, advance live particles, remove dead ones.
- **Spawn.** Per step, the candidate count is `spawnRate × spawnRectArea × dt`, rounded with a Bernoulli on the fraction and capped at `spawnPerStep`. This makes rain per unit area independent of the spawn rect, so zooming out does not thin it. Candidate `(x, y)` is drawn uniformly over the spawn rect (default: the density field's rect; `SetSpawnRect` narrows it, and the map passes the camera footprint plus margin); accepted when `rng() < density(x, y)` and the live count is under `maxLive`. When the cap truncates the count, the effective rate is lower and a metric counts the truncation.
- **Advance.** `pos += (wind × windScale + (0, 0, −fallSpeed + riseSpeed)) × dt`, with a deterministic lateral wobble of amplitude `wobbleAmount` for flakes.
- **Determinism and captures.** State is the history of `Update` calls (seed, density, wind and the dt sequence). A capture that must be byte-identical (DraoiHexWorldRender3D Map Assembly AC 7) drives `Update` with a fixed dt sequence.
- **Tweakable params.** `ParticleKindParams` is a plain struct with a DiaValueSchema sidecar (ranges and defaults) shipped with DiaBgfx3D; a consumer binds it with `ParamBinding` from its own `AttributeSet` and calls `Configure` when it changes. Changing a parameter never resets live particles. A particle dies when `z <= HeightAt(x, y)` or its age exceeds `lifeMax`.
- **Draw.** Instances are built per frame in a transient buffer: Streak = quad stretched along velocity by `streakLength`; Flake = camera-facing quad with a shader disc plus ink ring; Puff = quad with radial alpha, size and colour interpolated start→end over life. One draw per kind.

## Acceptance Criteria

1. **Budget.** `LiveCount(kind) ≤ maxLive` over 10,000 steps with spawn saturated; spawn per step ≤ `spawnPerStep`.
2. **Local spawning.** With density 0 everywhere except a 10 × 10 world-unit square, every spawned particle's spawn position lies inside that square (checked over 100,000 candidates).
3. **Density scaling.** Spawn rate is proportional to density: density 1.0 yields about 2× the particles of density 0.5 (within 15%) over a long run, with `maxLive` and `spawnPerStep` set high enough that neither saturates.
3a. **Rate independent of the spawn rect.** With the same density and `spawnRate`, spawn rects of area A and 4A give the same particle count per unit area (within 15%), until the `spawnPerStep` cap truncates (which is counted).
4. **No density, no particles.** A null/empty density spawns nothing; `SetDensity` with all zeros drains the system within `lifeMax`.
5. **Ground.** Rain over a ramp-shaped `IGroundQuery` dies at the ground height (no live particle is more than one step below ground); with no ground set, particles use `z = 0`.
6. **Wind.** A particle falling for t seconds under wind `w × windScale` has moved by `w × windScale × t` (±5%).
7. **Determinism.** The same seed, density, wind and sequence of `dt` give identical live particle positions and counts. A `dt` of 1/30 once equals two steps of 1/60.
8. **Puff life.** A puff's size goes from `sizeStart` to `sizeEnd` and its alpha from start to end alpha over its life, monotonically.
9. **Draw.** One draw per kind with live particles; zero draws for an empty kind. Particles behind the terrain are hidden by depth; they do not write depth.
10. **Culling.** Particles outside the camera frustum (plus a margin) are not submitted (counter).
11. **No point sprites.** All three kinds render on D3D11 (golden-image check on streak, flake, puff).
12. **Observability.** Metrics `bgfx.particles_live`, `bgfx.particles_spawned_per_step` and `bgfx.particles_spawn_truncated`; log channel `BGFX`; a profile scope in `Update`.
13. **Absolute altitude.** With `spawnAbsolute`, particles spawn at the given altitude regardless of ground height (checked over a ramp ground), and still die at the ground.
14. **Tweakable params.** Every `ParticleKindParams` field has a sidecar range and default; out-of-range values are clamped with one warning; reconfiguring does not reset live particles.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| SM-003 | Weather is per cell | Density fields are per world position, never global |
| PD-002 | PU/Phase/Module | Render-side only; density arrives from a FrameStream snapshot the game's module owns |
| PD-004 | No STL in public APIs | Fixed pools; `DynamicArrayC` internally |
| AD-001 | Engine API changes need sign-off | Signed off 2026-11-24 |

## Open Design Questions

1. **Spawn sampling cost.** Random uniform candidates waste work when density is sparse (one storm on a large map). The prototype sampled around storm centres. The generic fix is a coarse max-density grid to sample from, or `SetSpawnRect` by the caller. v1 uses `SetSpawnRect`; revisit if spawn efficiency shows in profiles.
2. ~~Particle height above terrain.~~ Answered 2026-11-24: `spawnAbsolute` per kind (AC 13).
3. **Density field shape.** `DensityField` is an axis-aligned grid with nearest sampling, so a hex world's per-cell values arrive as cell-sized squares and rain edges follow them. Fine for weather; if the edges look blocky, the consumer upsamples (DraoiHexWorldRender3D Feature 3 builds the field).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `DensityField`, `ParticleKindParams`, pools, fixed step | AC 1, 7 | Not Started | sonnet | |
| 2 | Spawning from density, area-based rate, spawn rect, absolute altitude | AC 2, 3, 3a, 4, 13 | Not Started | sonnet | |
| 3 | Advance, wind, ground, life | AC 5, 6, 8 | Not Started | sonnet | |
| 4 | Quad shaders and instance build (streak, flake, puff) | AC 9, 11 | Not Started | sonnet | needs Feature B |
| 5 | Culling and draw counts | AC 9, 10 | Not Started | sonnet | |
| 6 | Metrics, profile, docs | AC 12 | Not Started | haiku | |
| 7 | `ParticleKindParams` sidecar and clamping | AC 14 | Not Started | haiku | |
