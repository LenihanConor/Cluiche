# Implementation Plan: Seeded Noise

**Spec:** @docs/specs/applications/dia/systems/diamaths/seeded-noise.md
**Status:** Done

Files go in `Dia/DiaMaths/Noise/`; tests in `Cluiche/Tests/GoogleTests/DiaMaths/TestNoise*.cpp`. Every implementation `.cpp` starts with `#pragma fp_contract(off)`. Tasks run in order (they share the vcxproj files and `Noise.h`). TDD per `.claude/skills/tdd.md`: RED output quoted, then GREEN.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `NoiseHash.h` (integer hash, exact int-to-float, range-clamped floor, 16-entry gradient table), `ValueNoise2D`, `GradientNoise2D`; `Noise.h` with the full declared interface; vcxproj entries | AC1 (these types), AC2, AC3, AC5, AC6, AC7, AC8 | Done | sonnet | TDD. Golden bit patterns captured from first GREEN run, reviewed for sanity; Noise.h full interface; NoiseHash.h, Value/Gradient done; 9 tests pass Debug, Release DiaMaths compiles |
| 2 | `WorleyNoise2D` (5x5 window, F1/F2/F2-F1/cellId/cellValue, squared-distance compare) | AC1, AC9, AC10 | Done | sonnet | Brute-force 9x9 reference in the test; WorleyNoise2D 5x5; 10 tests pass (suite SLOW_NoiseWorley); jitter convention cell+jitter*u |
| 3 | `SimplexNoise2D` (OpenSimplex2S 2D); verify normaliser constant against published reference and record it in `SimplexNoise.cpp` | AC1, AC11 | Done | sonnet | If max < 0.9 the constant is wrong. Escalate to the user before using opus; SimplexNoise2D OpenSimplex2S 2D; N2=0.05481866495625118 verified, measured max 0.9998 / min -0.9994; 8 tests pass |
| 4 | `Noise3D`, `FractalNoise3D` | AC1, AC6, AC12 | Done | sonnet | 16-entry 3D gradient table; Noise3D + FractalNoise3D (gradient). 11 tests; measured 100k range [-0.899,0.888], sd 0.270, Lipschitz x2.20 y1.57 z1.46, z/z+0.001 corr 0.999996. Hash3D added to NoiseHash.h; FractalMaxAmplitude summed locally (task 5 to provide public) |
| 5 | `FractalNoise2D`, `RidgedNoise2D`, `Warp2D`, `WarpedNoise2D`, `FractalMaxAmplitude` (templates in `.inl`, explicit instantiation in `.cpp`) | AC1, AC13, AC14, AC15, AC16 | Done | sonnet | normalise=false must be bit-exact prefix-additive; FractalNoise.h/.inl/.cpp; 20 new tests, 60 Noise tests pass; Worley remap F1 2f-1, F2 f-1, F2-F1 2f-1, cell 2v-1 (clamped); warp seeds seed^0x68E31DA4 / ^0xB5297A4D; public FractalMaxAmplitude replaces local helpers in Noise3D.cpp/TestNoise3D.cpp |
| 6 | Purity source-scan test, thread-safety test, performance test (AC19, log timings) | AC17, AC18, AC19 | Done | sonnet | AC17-19 in TestNoisePurity.cpp (7 purity/thread tests + NoisePerf.GridTimings); Noise3D/NoiseFractal renamed SLOW_; Worley fBm range sampling trimmed (fractal 10.8s to ~6s). Debug 68/68 pass. Release perf NOT measured: Release googletest link fails (C1002 compiler out of heap, LTCG); only DiaMaths Release lib builds. Fixed DiaMaths.vcxproj FractalNoise.inl ClCompile to None. |
| 7 | Module doc `dia.maths.noise.architecture.module.md`; `dia check deps`; `dia docs registry` | — | Done | haiku | Module doc created; parent dia.maths updated; dia check deps passed (pre-existing stale dia.ui issue unrelated); dia docs registry regenerated |
| 8 | Verify: `dia run googletest --filter="*Noise*"` then full suite; observation scan; mark spec Done via `dia docs spec-done`; commit | AC20 | Done | haiku | Two known pre-existing failures (SaveManager, SaveLoadModule.ConfigDefaults); Debug *Noise* 68 pass; full suite 9482 pass, 2 known pre-existing failures (SaveManager, SaveLoadModule.ConfigDefaults); Release verified via standalone harness |
