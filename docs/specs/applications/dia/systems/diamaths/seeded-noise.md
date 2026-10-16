# Feature Spec: Seeded Noise

## Parent System
@docs/specs/applications/dia/systems/diamaths/diamaths.md

**Gameplay Domains:** spatial, simulation

**Need:** [dia-needs.md](../../../cow/dia-needs.md) #15 (CoW S-1 Terrain Generator; weather later).

## Problem Statement

Dia has no noise function (only `SmootherStep` in `Interpolation.h`). Procedural terrain, textures and variation all need smooth, repeatable noise. CoW's S-1 Terrain Generator needs it first (continent shape, mountain ridges, ruggedness, coastline warping), and A4 Weather later needs time-evolving fields.

The result must be **bit-identical** across runs, builds and machines, because S-1 guarantees "same seed, same world" and F-2 enforces registered RNG streams. So the noise cannot call `std::sin`/`std::fmod` in its lattice path or draw from a hidden global generator.

## Goals

- **Four 2D base noises**, all returning a defined range:
  - **Value noise:** smoothly interpolated lattice values, [-1, 1].
  - **Gradient noise** (Perlin-style, **16 gradient directions**), [-1, 1].
  - **Simplex noise:** **OpenSimplex2S 2D** (the smoother variant), [-1, 1]. Chosen to avoid the simplex patent question: it is the public-domain (CC0) OpenSimplex2 algorithm family, re-implemented here from its published definition, with no code or tables copied from patented simplex implementations.
  - **Worley / cellular noise:** returns **F1**, **F2**, **F2 - F1** and the **nearest-cell id** from one call, using a **5 x 5 cell search** so F1 and F2 are exact.
- **3D gradient noise** (`Noise3D`, `FractalNoise3D`) for time-evolving fields: use `z` as time.
- **Fractal (fBm)**, **ridged** and **domain warping** work over any of Value, Gradient and Simplex, and fBm and warping also work over Worley (via a chosen scalar output). The base noise is a **compile-time template parameter** (no runtime switch).
- **fBm `normalise` flag:** lower octaves can stay fixed while only fine detail changes (exact behaviour below).
- **Bit-identical results** from integer hashing, a fixed arithmetic order and `#pragma fp_contract(off)`.
- **Pure functions:** no global state, no allocation, safe from any thread.
- **Caller-supplied seed:** a `uint32_t` the caller draws once from its own `RandomStream`; the noise itself never draws randomness.

## Non-Goals

- **3D value, simplex, Worley and 3D ridged/warp:** no consumer; weather needs smooth time evolution, which 3D gradient covers. Add when needed.
- **Ridged noise over Worley:** ridged folds a signed noise at zero; Worley's F2 - F1 already produces crest lines. Not supported (compile-time `static_assert`).
- **A runtime noise-type switch:** not built now; see Resolved Decisions 3.
- **Tileable (seamless-wrapping) noise**, **4D noise**, **a GPU/shader version**.
- **Hex-specific noise:** callers sample at `HexTopology::CellCenter` positions as plain 2D floats.

## Determinism

Everything from `(seed, x, y[, z])` to the result is deterministic:

1. **Lattice and hash are integers only.** `floor` of the input is an integer cast with a documented range check (inputs outside +-2^23 are clamped). The corner hash is a fixed 32-bit integer mix (multiply/xorshift, no `std::` calls). Integer-to-float conversion uses exact scaling (`(h >> 8) * 2^-24`, which is exactly representable), so Value lattice values and Worley feature-point jitter are exact.
2. **Gradients come from constant tables** indexed by the hash: 16 unit 2D vectors (22.5 degree steps) for gradient noise, the OpenSimplex2S 2D gradient set (24 vectors, scaled by the published normaliser) for simplex, and 16 entries for 3D (the 12 classic edge vectors padded by 4 repeats). All written as float literals; no `sin`/`cos`.
3. **Float arithmetic is `+`, `-`, `*` plus `sqrtf`** (correctly rounded under IEEE 754, so deterministic) in an explicitly written order. Worley compares squared distances over its 25 candidate points and takes `sqrtf` only for the outputs. No `std::pow`, `std::fma` or fast-math.
4. **Compiler contraction is off.** The implementation files use `#pragma fp_contract(off)`, so Release builds with FMA enabled don't change results.
5. **Golden bit-pattern tests** (AC1) pin results for every noise type, so a compiler or flag change that alters results fails a test instead of silently changing every world.
6. **fBm octave `i` uses `seed + i * kOctaveSeedStride`**, so octave i is the same regardless of the octave count.
7. **No hidden dependencies:** no thread-locals, no statics, no `RandomStreamManager` access inside the noise functions. The caller seeds from a registered stream, so nothing bypasses F-2's stream enforcement.

## Public Interface

```cpp
// Dia/DiaMaths/Noise/Noise.h  (includes the per-type headers below)
#pragma once
#include <cstdint>

namespace Dia { namespace Maths {

enum class NoiseType : uint8_t { Value, Gradient, Simplex, Worley };

// ---- Single-octave 2D base noises ------------------------------------------------------
float ValueNoise2D   (uint32_t seed, float x, float y);   // [-1, 1]
float GradientNoise2D(uint32_t seed, float x, float y);   // [-1, 1]; exactly 0 at integer lattice points
float SimplexNoise2D (uint32_t seed, float x, float y);   // [-1, 1]; OpenSimplex2S 2D

// Worley: feature point per unit cell, jittered by 'jitter' in [0, 1] (0 = regular grid).
// Searches the 5 x 5 cells around the sample, so f1 and f2 are exact for any jitter.
// Distances are in cell units (scale the input to change feature size).
struct WorleyResult {
    float    f1;            // distance to nearest feature point
    float    f2;            // distance to second nearest
    float    f2MinusF1;     // f2 - f1 (>= 0; 0 on cell borders: crest lines)
    uint32_t cellId;        // hash id of the nearest feature point's cell (stable, seed dependent)
    float    cellValue;     // cellId mapped to [0, 1) (exact)
};
WorleyResult WorleyNoise2D(uint32_t seed, float x, float y, float jitter = 1.0f);

// ---- Fractal / ridged / warp over a base type (compile-time template parameter) -------
enum class WorleyOutput : uint8_t { F1, F2, F2MinusF1, CellValue };

struct FractalParams {
    WorleyOutput worley     = WorleyOutput::F2MinusF1; // used only when the base is NoiseType::Worley (output remapped to [-1, 1])
    float        jitter     = 1.0f;                    // Worley only
    int          octaves    = 4;                       // 1..kMaxNoiseOctaves
    float        frequency  = 1.0f;                    // multiplies x, y before octave 0
    float        lacunarity = 2.0f;                    // frequency multiplier per octave
    float        gain       = 0.5f;                    // amplitude multiplier per octave
    bool         normalise  = true;                    // see "fBm normalise"
};
constexpr int      kMaxNoiseOctaves  = 12;
constexpr uint32_t kOctaveSeedStride = 0x9E3779B9u;

// Total amplitude S = sum(gain^i, i < octaves): the range bound when normalise == false.
float FractalMaxAmplitude(const FractalParams& params);

// fBm. normalise == true: result in [-1, 1]. normalise == false: result in [-S, S].
template <NoiseType Base>
float FractalNoise2D(uint32_t seed, float x, float y, const FractalParams& params);

// Ridged multifractal over Value, Gradient or Simplex (static_assert(Base != Worley)). Per octave
// r = (1 - |n|)^sharpness, summed with the same amplitudes. Result in [0, 1] when
// normalise == true (1 on a ridge crest), [0, S] otherwise.
struct RidgedParams {
    FractalParams fractal;
    int           sharpness = 2;   // integer 1..4, repeated multiplication (no std::pow)
};
template <NoiseType Base>
float RidgedNoise2D(uint32_t seed, float x, float y, const RidgedParams& params);

// Domain warp: displaces the point by two independent fractal noises scaled by 'strength'
// (input units). Feed the result to any noise call.
struct Warp2DResult { float x; float y; };
template <NoiseType WarpBase>
Warp2DResult Warp2D(uint32_t seed, float x, float y, float strength, const FractalParams& warp);

// Convenience: FractalNoise2D<NoiseBase> sampled at the warped point.
template <NoiseType WarpBase, NoiseType NoiseBase>
float WarpedNoise2D(uint32_t seed, float x, float y, float strength,
                    const FractalParams& warp, const FractalParams& noise);

// ---- 3D gradient noise (time-evolving fields: pass time as z) --------------------------
float Noise3D(uint32_t seed, float x, float y, float z);                              // [-1, 1]; 0 at integer lattice points
float FractalNoise3D(uint32_t seed, float x, float y, float z, const FractalParams& params);  // gradient only

} } // namespace Dia::Maths
```

Files: `Dia/DiaMaths/Noise/` with `NoiseHash.h` (hash + tables), `ValueNoise`, `GradientNoise`, `SimplexNoise`, `WorleyNoise`, `FractalNoise` (templates in `.inl`, explicitly instantiated for the four base types in the `.cpp`), `Noise3D` (`.h/.cpp`), added to `DiaMaths.vcxproj` and filters (`dia docs vcxproj-add DiaMaths`). Sub-module doc `dia.maths.noise.architecture.module.md` (parent `dia.maths`).

### fBm normalise (exact behaviour)

Octave `i` has amplitude `a_i = gain^i` and frequency `frequency * lacunarity^i`; `S = sum a_i`.

- **`normalise = true` (default):** `result = (sum a_i * n_i) / S`, so the range stays [-1, 1]. Adding an octave **rescales** the earlier octaves (their share shrinks), so low-frequency shape changes slightly when the octave count changes.
- **`normalise = false`:** `result = sum a_i * n_i`, range [-S, S]. Octaves are accumulated in order `0..N-1`, so the N+1-octave result is **bit-identical** to the N-octave result plus the next term: lower octaves stay fixed and only fine detail is added. Use `FractalMaxAmplitude` to rescale into a range afterwards.

Ridged and Warp follow the same flag.

## Usage (S-1 Terrain Generator, for context)

```cpp
const uint32_t seed = stream.NextUInt();                  // drawn once from the WLD RandomStream
const Vector2D p = HexTopology::CellCenter(col, row);
auto  w         = Warp2D<NoiseType::Gradient>(seed + 2, p.x, p.y, 4.0f, coastWarp);   // bend the coastline
float continent = FractalNoise2D<NoiseType::Simplex>(seed, w.x, w.y, continentParams);
float ridges    = RidgedNoise2D<NoiseType::Gradient>(seed + 1, p.x, p.y, ridgeParams);
float plates    = WorleyNoise2D(seed + 3, p.x * 0.1f, p.y * 0.1f).cellValue;     // region seeds
float weather   = Noise3D(seed + 4, p.x * 0.05f, p.y * 0.05f, weekIndex * 0.1f); // evolves with time
```

## Acceptance Criteria

Tests live in `Cluiche/Tests/GoogleTests/DiaMaths/` (`TestNoise*.cpp`). "Every type" means Value, Gradient, Simplex, Worley (each of its outputs) and 3D gradient.

| # | Criterion |
|---|-----------|
| AC1 | **Golden values:** for every noise type, a table of >= 16 (seed, coordinate) pairs (including negative, zero, large and lattice-aligned coordinates, and several seeds) is checked against stored **bit patterns** (`uint32_t` bit-cast), and for fBm, ridged, warp and Worley (`f1`, `f2`, `f2MinusF1`, `cellId`) as well. Built with the project's Release and Debug configs; both must match |
| AC2 | **Determinism:** the same `(seed, x, y[, z])` returns bit-identical floats across 1,000 repeated calls for every type and composite |
| AC3 | **Range:** over 100,000 random samples, Value, Gradient and Simplex stay in [-1, 1] and 3D gradient stays in [-1, 1]; Worley outputs satisfy their documented bounds (`f1 >= 0`, `f2MinusF1 >= 0`, `cellValue` in [0, 1)); fBm (normalise on) stays in [-1, 1] and (normalise off) within +-`FractalMaxAmplitude`; ridged stays in [0, 1] / [0, S] |
| AC4 | **Distribution sanity:** over 65,536 samples the mean of each signed type is within +-0.03 of 0 and the standard deviation lies in a documented per-type band; Worley `f1` mean lies in a documented band; `cellValue` is uniform within a documented chi-square bound |
| AC5 | **Seed sensitivity:** two different seeds differ at >= 95% of 1,024 points for every type; the same seed is identical |
| AC6 | **Lattice behaviour:** Gradient and 3D gradient are exactly 0 at integer lattice points; Value equals its hashed lattice value there (and is not generally 0) |
| AC7 | **Continuity:** for Value, Gradient, Simplex and 3D gradient the absolute difference between samples 0.001 apart is below a documented Lipschitz bound, including across lattice cell borders and at 0 and negative coordinates. Worley `f1` is continuous (same test); `cellId` changes only across Voronoi borders |
| AC8 | **Gradient set:** gradient noise uses 16 directions (a test counts the distinct gradients selected over 100,000 hashes: 16) and its autocorrelation along x and y differs by less than a documented bound (axis-bias check) |
| AC9 | **Worley ordering and exactness:** for every sample, `f1 <= f2`, `f2MinusF1 == f2 - f1` (bit-exact), and `f2MinusF1 >= 0`. Both `f1` and `f2` match a brute-force reference that scans a 9 x 9 cell window exactly (bit-for-bit) over 100,000 random samples at jitter 0, 0.5 and 1 (the 5 x 5 window is exact, not approximate) |
| AC10 | **Worley jitter:** `jitter = 0` puts `f1 == 0` exactly at every integer-cell feature point; `jitter = 1` gives different feature points per cell; `cellId` is the same for all points whose nearest feature point is the same cell and differs between neighbouring Voronoi cells |
| AC11 | **Simplex (OpenSimplex2S 2D):** output in [-1, 1] with measured maximum >= 0.9 (no wasted range; the published normaliser is verified here); no visible diagonal artefact: rotating the input by 45 degrees changes the standard deviation by less than a documented bound |
| AC12 | **3D slice continuity:** `Noise3D` is continuous in `z` (samples `dz` apart differ by less than the Lipschitz bound times `dz`), each constant-`z` slice is continuous in x and y, and the `z = 0` slice is not identical to the `z = 1` slice (it evolves) while `z` and `z + 0.001` are highly correlated (correlation > 0.99 over 1,024 points) |
| AC13 | **fBm / ridged / warp over any base:** for `NoiseType::Value`, `Gradient` and `Simplex`, `FractalNoise2D`, `RidgedNoise2D` and `Warp2D` return in-range, deterministic results; fBm and `Warp2D` also work over `Worley` (each `WorleyOutput`); `RidgedNoise2D<NoiseType::Worley>` does not compile (`static_assert`) |
| AC14 | **Octave behaviour:** with `normalise = false`, the N+1-octave result equals the N-octave result plus the (N+1)th term **bit-exactly** (lower octaves fixed); with `normalise = true`, results stay in [-1, 1] for octaves 1..12 and equal `unnormalised / FractalMaxAmplitude` within 1 ulp |
| AC15 | **Ridged shape:** crests (value near 1) occur where the base noise crosses zero; raising `sharpness` from 1 to 4 never lowers the variance of the output; `sharpness` outside 1..4 clamps |
| AC16 | **Domain warp:** `Warp2D` with `strength == 0` returns the input exactly; with `strength > 0` the displacement magnitude is bounded by `strength * sqrt(2) * FractalMaxAmplitude(warp)` (and by `strength * sqrt(2)` when normalised) |
| AC17 | **Purity:** the noise sources contain no `static` mutable state, no `<random>`, no `std::sin/cos/pow/fmod/floor` and no thread-locals (checked by a source-scan test); `#pragma fp_contract(off)` is present in each implementation file |
| AC18 | **Thread safety:** the same calls from 4 threads at once return the same results as single-threaded, for every type |
| AC19 | **Performance (Release):** a 66 x 42 grid (2,772 samples) x 6 octaves of `FractalNoise2D` completes in under 1 ms for Value, Gradient and Simplex, under 2 ms for `FractalNoise3D`, and under **4 ms for Worley** (the 5 x 5 search evaluates 25 candidate points per sample, about 2.8x the cost of a 3 x 3 search; single-octave Worley over the grid stays under 1 ms); ridged and warped composites of Gradient under 2 ms. Debug sanity: each under 100 ms. The measured time per type is logged |
| AC20 | `dia run googletest --filter="*Noise*"` passes and the full suite is unaffected |

## Binding Decisions

No binding constraints beyond the parent: Dia stays free of CoW types (the interface is plain `float`/`uint32_t`), the functions are pure, and `#pragma fp_contract(off)` is mandatory in the noise implementation files (resolved 2026-10-15).

## Resolved Decisions

All three former open questions were settled by the user on 2026-10-15.

1. **Worley uses a 5 x 5 search window** so F1 and F2 are exact for any jitter. Cost: 25 candidate points per sample instead of 9 (about 2.8x slower than a 3 x 3 search); AC19 budgets Worley at under 4 ms for 2,772 samples x 6 octaves. AC9 checks exactness against a 9 x 9 brute-force reference.
2. **Simplex is OpenSimplex2S 2D** (the smoother variant, fewer artefacts than the "F" form), using the **published reference scale constant**. The constant is a **build-time detail**, not a design question: the implementation records the reference's 2D gradient normaliser (cited from memory as `N2 = 0.05481866495625118` for OpenSimplex2S; verify against the published reference at build time and record the verified value in `SimplexNoise.cpp`) and applies it to its gradient table. AC11 measures the output maximum (>= 0.9) as the check; if it fails, the constant was transcribed wrong, not the algorithm.
3. **The base noise is a compile-time template parameter** (`FractalNoise2D<NoiseType::Simplex>`), with explicit instantiation for the four base types. No runtime switch for now. If a tool needs to choose the type from a setting, a thin runtime wrapper (`FractalNoise2DDynamic(NoiseType, ...)` that switches once to the templated call) can be added later without changing the templates.

## Open Design Questions

None remaining.

## Status

`Done` — 2026-10-16. Built per [seeded-noise.plan.md](seeded-noise.plan.md) (8 tasks). All 68 `*Noise*` tests pass in Debug and 67 in Release|x64 (the Release run omits the Debug-only perf-limit variant), including the golden bit patterns (AC1) and the Release performance limits (AC19: Worley fBm 2.2 ms of 4, Simplex 0.46 ms of 1, 3D 0.80 ms of 2 per 66x42 grid x 6 octaves). OpenSimplex2S N2 = 0.05481866495625118, measured max 0.9998. `Noise3D` peaks at about 0.89 (gradient scale 1.0). Release GoogleTests needed `PreferredToolArchitecture=x64` in `dia`'s compile step to avoid `C1002`.
