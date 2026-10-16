---
schema: dia.module.v1
module_id: dia.maths.noise
name: Noise
owner_team: TBD
layer: platform
status: active
maturity: stable

path: Dia/DiaMaths/Noise
language: cpp
parent_module_id: dia.maths

summary: >
  Deterministic seeded noise functions: value, gradient, simplex, and Worley, with fractal, ridged, and domain-warp composites and 3D gradient support.

intent: >
  Provide pure, bit-identical noise generation for procedural terrain, textures, and variation.
  All operations are deterministic from (seed, x, y[, z]) with no global state, allocation, or thread-local dependencies.
  Results are bit-identical across builds, machines, and runs via integer hashing and fixed arithmetic order with #pragma fp_contract(off).

responsibilities:
  - Provide four 2D base noise types: Value, Gradient, Simplex (OpenSimplex2S), and Worley (cellular)
  - Provide 3D gradient noise for time-evolving fields
  - Implement fractal (fBm), ridged, and domain-warp composites over any base type
  - Guarantee bit-identical results via integer lattice hashing and deterministic float operations
  - Support pure functions with caller-supplied seeds (no RandomStream access inside noise)
  - Deliver performance budgets: single-octave < 1 ms per 1,000 samples; 6-octave over 2,772 samples < 4 ms (Worley) / < 2 ms (others)

non_responsibilities:
  - 3D value, simplex, Worley, or 3D ridged/warp (not yet used; weather needs time evolution only)
  - Ridged noise over Worley (fold semantic mismatch; use F2 - F1 instead)
  - Runtime noise-type dispatch (base type is compile-time template parameter; thin wrapper can be added later)
  - Tileable (seamless-wrapping) noise, 4D, GPU/shader versions, or hex-specific indexing
  - Random number generation (caller draws seed once and passes it in)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaMaths/Noise/Noise.h
    - Dia/DiaMaths/Noise/NoiseHash.h
    - Dia/DiaMaths/Noise/ValueNoise.h
    - Dia/DiaMaths/Noise/GradientNoise.h
    - Dia/DiaMaths/Noise/SimplexNoise.h
    - Dia/DiaMaths/Noise/WorleyNoise.h
    - Dia/DiaMaths/Noise/FractalNoise.h
    - Dia/DiaMaths/Noise/Noise3D.h
  namespaces:
    - Dia::Maths
  entry_points:
    - "ValueNoise2D(seed, x, y) → float [-1, 1]"
    - "GradientNoise2D(seed, x, y) → float [-1, 1] (exactly 0 at lattice points)"
    - "SimplexNoise2D(seed, x, y) → float [-1, 1] (OpenSimplex2S)"
    - "WorleyNoise2D(seed, x, y, jitter) → WorleyResult (f1, f2, f2MinusF1, cellId, cellValue)"
    - "FractalNoise2D<NoiseType>(seed, x, y, params) → float (fBm over base type)"
    - "RidgedNoise2D<NoiseType>(seed, x, y, params) → float (ridged multifractal)"
    - "Warp2D<NoiseType>(seed, x, y, strength, params) → Warp2DResult (domain warp)"
    - "WarpedNoise2D<WarpBase, NoiseBase>(seed, x, y, strength, warpParams, noiseParams) → float"
    - "Noise3D(seed, x, y, z) → float [-1, 1] (3D gradient for time evolution)"
    - "FractalNoise3D(seed, x, y, z, params) → float (3D gradient fBm)"
    - "FractalMaxAmplitude(params) → float (range bound helper)"
    - "NoiseType (enum: Value, Gradient, Simplex, Worley)"
    - "WorleyOutput (enum: F1, F2, F2MinusF1, CellValue)"
    - "FractalParams (octaves, frequency, lacunarity, gain, normalise, jitter)"
    - "RidgedParams (fractal, sharpness)"
    - "WorleyResult (struct: f1, f2, f2MinusF1, cellId, cellValue)"
    - "Warp2DResult (struct: x, y)"

dependencies:
  required: []
  optional: []
  forbidden:
    - dia.application

---

# Noise

## Overview

This module provides deterministic noise generation for procedural content.
All operations guarantee bit-identical results through:
- **Integer-only lattice and hash** (no `sin`/`cos`/`floor` in lattice path)
- **Constant gradient tables** indexed by hash (16 directions for Gradient, 24 for Simplex, 16 for 3D)
- **Fixed arithmetic order** with `#pragma fp_contract(off)` to disable FMA
- **Caller-supplied seeds** (no RandomStream access inside noise functions)

## Base Noise Types

- **Value Noise:** Smoothly interpolated lattice values, range [-1, 1]
- **Gradient Noise:** Perlin-style with 16 gradient directions (22.5° steps), exactly 0 at integer lattice points, range [-1, 1]
- **Simplex (OpenSimplex2S 2D):** Smoother simplex variant, public-domain implementation, range [-1, 1]
- **Worley (Cellular):** Feature point per unit cell (jittered), returns F1, F2, F2 - F1, nearest-cell ID via 5×5 cell search (exact for any jitter)

## Composite Noise

- **fBm (Fractal Brownian Motion):** Multi-octave fractal, with `normalise` flag to preserve lower-octave values while adding fine detail (lower octaves bit-identical when normalise = false)
- **Ridged Multifractal:** Folds the base noise at zero (1 - |n|)^sharpness per octave, produces ridge crests; works only over Value, Gradient, Simplex
- **Domain Warp:** Displaces input point by two independent fractal noises; can be chained with any noise call

## 3D Support

- **Noise3D:** 3D gradient noise (use z as time for time-evolving fields), exactly 0 at integer lattice points
- **FractalNoise3D:** Multi-octave 3D gradient (no 3D Value/Simplex/Worley yet; weather use case is time evolution)

## Determinism

All functions are **pure:**
- No `static` mutable state
- No `#include <random>` or RandomStream access
- No thread-locals
- Caller supplies seed once per invocation

Result is **bit-identical** across:
- Multiple runs with same inputs
- Different machines
- Release vs Debug builds (via `#pragma fp_contract(off)`)

## Performance (Release, per 1,000 samples)

- Single-octave (Value, Gradient, Simplex): < 1 ms
- Single-octave Worley: < 1 ms (25-point search)
- 6-octave FractalNoise2D (Value/Gradient/Simplex): < 2 ms
- 6-octave FractalNoise3D: < 2 ms
- 6-octave Worley: < 4 ms
- Ridged/Warp composites: < 2 ms

## Usage Example (S-1 Terrain Generator)

```cpp
const uint32_t seed = stream.NextUInt();  // drawn once from the WLD RandomStream
const Vector2D p = HexTopology::CellCenter(col, row);
auto  w         = Warp2D<NoiseType::Gradient>(seed + 2, p.x, p.y, 4.0f, coastWarp);
float continent = FractalNoise2D<NoiseType::Simplex>(seed, w.x, w.y, continentParams);
float ridges    = RidgedNoise2D<NoiseType::Gradient>(seed + 1, p.x, p.y, ridgeParams);
float plates    = WorleyNoise2D(seed + 3, p.x * 0.1f, p.y * 0.1f).cellValue;
float weather   = Noise3D(seed + 4, p.x * 0.05f, p.y * 0.05f, weekIndex * 0.1f);
```

## Implementation Notes

- **Base type is compile-time template parameter** (`FractalNoise2D<NoiseType::Simplex>`) for zero runtime dispatch cost; thin wrapper can be added later if tool needs runtime selection
- **Explicit template instantiation** for four base types in `.cpp` files prevents binary bloat
- **Integer hash** (multiply/xorshift, no std::* calls) maps (seed, integer coords) → unique pattern
- **Worley 5×5 window** finds exact F1 and F2 for any jitter; costs 25 evaluations vs 9 for a 3×3 search
- **OpenSimplex2S normaliser** verified at build time (reference value recorded in SimplexNoise.cpp)
- **#pragma fp_contract(off)** in each `.cpp` file ensures Release builds with FMA enabled match Debug

## Testing

All acceptance criteria in `docs/specs/applications/dia/systems/diamaths/seeded-noise.md`:
- AC1: Golden bit-pattern tests for all types
- AC2: Determinism over 1,000 repeated calls
- AC3: Range bounds verified over 100,000 random samples
- AC4: Distribution sanity (mean within ±0.03, std deviation in documented bands)
- AC5: Seed sensitivity (≥95% difference at 1,024 points)
- AC6: Lattice behaviour (Gradient exactly 0 at integer points)
- AC7: Continuity and Lipschitz bounds
- AC8: Gradient set uses 16 directions; axis bias < documented threshold
- AC9: Worley F1 ≤ F2, F2 - F1 exact, exactness vs 9×9 brute-force reference
- AC10: Jitter behaviour (jitter=0 puts F1=0 at cell points; jitter=1 randomizes)
- AC11: Simplex range, normaliser verification, no diagonal artefact
- AC12: 3D slice continuity in z, x, y; z evolution verified
- AC13: Fractal/ridged/warp over all base types; Ridged<Worley> doesn't compile
- AC14: Octave behaviour (normalise=false: bit-exact additive; normalise=true: [−1,1] range)
- AC15: Ridged crest shape; sharpness monotonicity
- AC16: Warp displacement bounds
- AC17: Purity scan (no mutable state, no std::sin/cos/pow/fmod/floor, no <random>)
- AC18: Thread safety (4 threads, same results as single-threaded)
- AC19: Performance budgets per type
- AC20: Full test suite passes

---

## References

- **Spec:** docs/specs/applications/dia/systems/diamaths/seeded-noise.md
- **Tests:** Cluiche/Tests/GoogleTests/DiaMaths/TestNoise*.cpp
- **OpenSimplex2 Reference:** Public-domain CC0 reference at https://github.com/KdotJPG/OpenSimplex2
