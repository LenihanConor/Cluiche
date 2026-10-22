# DiaValueSchema ramp conformance fixtures

Shared fixtures for ramp sampling, legend ticks and enum palettes. Each file carries its own expectations so the TypeScript implementation can load it without C++.

Consumers:
- C++: `Cluiche/Tests/GoogleTests/DiaValueSchema/TestRampConformance.cpp` loads every file and asserts `SampleRamp`, `BuildLegend` and `ResolvePalette` match the `expected*` fields exactly (ticks use a tight float tolerance).
- TypeScript: `sampleRamp`, `buildLegend`, `resolvePalette` must produce the same results from the same files.

Every file has `"format"` and `"version"` fields.

## samples.json (`dia.ramps.conformance.samples`)

- `ramps`: verbatim copy of the `ramps` array in `Data/ramps.json` (the C++ test fails if they drift).
- `samples`: `{ "ramp": id, "t": number, "expectedHex": "#rrggbb" }`. `t` is clamped to [0,1] (-0.5 and 1.5 are included). Channels are interpolated linearly in sRGB between the surrounding stops and rounded half up; alpha is always ff.

## legend-ticks.json (`dia.ramps.conformance.legend_ticks`)

- `cases`: `{ "min", "max", "unit"?, "expectedUnit", "expectedTicks": [...] }`, a float entry with those bounds.
- Algorithm: the smallest step of the form {1,2,5} x 10^n leaving at most 3 multiples strictly inside (min,max); ticks = min, those multiples, max. `min == max` or `min > max` gives a single tick `[min]`. Interior ticks are noise free (0.3, not 0.30000000000000004).

## palettes.json (`dia.ramps.conformance.palettes`)

- `categorical`: the `categorical.12` colours.
- `cases`: `{ "name", "values": [{ id, label, colour? }], "expectedColours": ["#rrggbb"...], "expectedWrapped": bool }`. A given colour wins; a colour-less value at index i gets `categorical[i % 12]`; `expectedWrapped` is true when colour-less values exceed 12.

## Rules

- TypeScript must produce the same results. A new function (or a changed rule) must add a fixture here.
- These are data files: not listed in any vcxproj.
