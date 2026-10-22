# Feature Spec: DiaValueSchema Ramps

**Parent:** @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md
**Builds on:** @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md (Feature 1), @docs/specs/applications/dia/systems/diavalueschema/tooltips.md (Feature 3, the query)

**Gameplay Domains:** debug, tools

## Summary

A named colour-ramp and palette library that every surface shares, so a value looks the same in the in-game viewer, the editors and T-1's mini-map. It delivers a `dia.ramps` data file with the Dia ramps, a loader and lookup in `ValueSchemaService`, functions that resolve a schema entry to ramp or palette data and to legend data, and the same functions in TypeScript. DiaValueSchema returns **neutral colour data**; the consumer (the DiaCellLayers viewer) converts it to CellField's ramp type, so DiaValueSchema has no dependency on DiaGraphics.

## Problem

A value's colours must come from one place: the legend in the game, the hex map in an editor and the delta view all have to agree. Without a shared library each consumer would pick its own colours, and a category such as a biome would change colour between tools. The system spec also named a `ToCellFieldRamp` function that would couple a schema module to the renderer.

## Goals

1. **One definition.** Ramps and palettes live in a JSON data file read by both C++ and the editors.
2. **Named and open.** Dia ships a fixed set; a game may add more through its own `*.ramps.json`.
3. **Resolve from a schema entry.** `ui.ramp` names a ramp; enum entries get a palette from their value colours; sensible defaults apply when nothing is named.
4. **Legends from the same source.** Legend data (ticks, units, swatches, "seen in view" range) comes from one function in C++ and one in TypeScript, tested against the same fixtures.
5. **Delta ramps.** A symmetric diverging ramp for DiaCellHistory's delta view.
6. **No renderer dependency.** DiaValueSchema never includes CellField or DiaGraphics.

## Non-Goals

- Converting to CellField's `CellFieldRampStop` or registering with CellField (DiaCellLayers Feature 1).
- Drawing a legend (UI features draw it; this builds the data).
- Per-user theming, dark/light variants, reversible ramps.
- Perceptual interpolation (Lab); stops interpolate in sRGB (see Open Q2).
- Editing ramps in a UI.

## Architecture

### Ramp file (`dia.ramps` v1)

```json
{
  "format": "dia.ramps", "version": 1,
  "ramps": [
    { "id": "sequential.green", "kind": "continuous",
      "stops": [ { "t": 0.0, "colour": "#f7fcf5" }, { "t": 0.5, "colour": "#74c476" }, { "t": 1.0, "colour": "#00441b" } ] },
    { "id": "categorical.12", "kind": "categorical",
      "colours": [ "#a6cee3", "#1f78b4", "..." ] }
  ]
}
```

- **Continuous:** 2 to 16 stops (CellField's limit), `t` strictly increasing from 0 to 1, colours as `#rrggbb`.
- **Categorical:** 2 to 32 colours in order.
- **Ids** are global (not `<for>/<key>`), lower-case dotted names.

**Shipped by Dia** (`Dia/DiaValueSchema/Data/ramps.json`):

| Id | Kind | Use |
|---|---|---|
| `sequential.green` | continuous | vegetation, biomass |
| `sequential.blue` | continuous | water, precipitation |
| `sequential.heat` | continuous | temperature |
| `sequential.grey` | continuous | default for unnamed numeric values |
| `sequential.purple` | continuous | secondary scalar fields |
| `sequential.terrain` | continuous | elevation |
| `diverging.blue_red` | continuous | deltas, anomalies (midpoint at `t = 0.5`) |
| `categorical.12` | categorical | default for category values |

**Design rules** (checked by tests, final colours chosen at build):
- Sequential ramps have monotonically changing lightness from `t = 0` to `t = 1`.
- The diverging ramp is symmetric in lightness about a light neutral midpoint.
- `categorical.12` colours are pairwise distinguishable (a minimum Lab distance, a tunable constant) and are chosen to remain distinct under common colour-blindness simulations.

**Game ramps.** A game may ship `*.ramps.json` of the same format, found through the path aliases like schema sidecars. A game ramp whose id equals a Dia ramp is rejected with an Error (Dia wins); ids between game files follow Feature 1's rule (first wins, Error names both).

### Loading and lookup

`ValueSchemaModule` (Feature 1) also loads `dia.ramps` files at start, with the same failure policy: a bad file is skipped with an Error, and a bad ramp inside a file is skipped with an Error naming the id. The published `ValueSchemaService` gains:

```cpp
const RampData* FindRamp(StringCRC rampId) const;
```

Over the DiaDebugServer, the `valueschema.get` result (Feature 3) gains a `ramps` array so editors and the panel use the same definitions.

### Resolving a schema entry

```cpp
struct RampData    { StringCRC id; RampKind kind; RampStop stops[16]; uint32 stopCount; };   // neutral: t + RGBA8
struct PaletteData { PaletteEntry entries[...]; uint32 count; uint32 fallbackRgba8; };       // enum id → RGBA8

RampData    ResolveRamp(const ValueInfo&, const ValueSchemaService&);      // float entries
PaletteData ResolvePalette(const ValueInfo&, const ValueSchemaService&);   // enum entries
RampData    ResolveDeltaRamp(const ValueInfo&, const ValueSchemaService&, float maxAbs);
```

- **`ResolveRamp`:** uses `ui.ramp`; if absent, `sequential.grey`; if the id is unknown, `sequential.grey` plus one Warning per key on channel `VSC` (`dia validate schema` fails it). The entry's `min`/`max` are returned alongside, so consumers map values to `t`.
- **`ResolvePalette`:** enum value `colour` where given; otherwise `categorical.12` by value order, wrapping with a Warning once per entry if there are more than 12 values (see Open Q3). `fallback` is a neutral grey.
- **`ResolveDeltaRamp`:** `ui.delta_ramp`, default `diverging.blue_red`; the range is symmetric, `[-maxAbs, +maxAbs]`, so zero maps to the neutral midpoint.
- **Sampling:** `SampleRamp(ramp, t)` clamps `t` to [0, 1] and interpolates linearly between the surrounding stops in sRGB, matching the 256 × 1 texture CellField bakes from the same stops.

The DiaCellLayers viewer calls these, converts the stops to `CellFieldRampStop`, and registers them with CellField. That conversion is outside DiaValueSchema.

### Legend data

```cpp
struct LegendData {
    // ramp legends
    RampData ramp; float min, max; const char* unit; float ticks[7]; uint32 tickCount;
    bool hasSeen; float seenMin, seenMax;
    // category legends
    SwatchEntry swatches[...]; uint32 swatchCount;       // id, label, colour
};
LegendData BuildLegend(const ValueInfo&, const ValueSchemaService&, bool hasSeen, float seenMin, float seenMax);
```

- **Ticks:** up to 5 tick values at "nice" numbers across `min`–`max` (1, 2, 5 × 10ⁿ steps), always including both ends.
- **Seen in view:** the observed range is returned beside the schema range so a legend can show both.
- **Categories:** one swatch per enum value in schema order, with label and colour.

### TypeScript side

`@dia/editor-ui` (or a small `ramps` module it exports) has `resolveRamp`, `resolvePalette`, `sampleRamp`, `buildLegend` taking the `ValueInfo`-shaped entry and the `ramps` array from the query. They match the C++ results. `CellGridCanvas` consumes the output (`Ramp`/`Palette` props).

Both sides are tested against the same fixtures in `Dia/DiaValueSchema/Testing/Conformance/ramps/`: ramp samples at chosen `t`, legend ticks for chosen ranges, palette assignment for enums, each with an `expected.json`.

### Logging and observation

- **Logs:** `VSC` Warning once per unknown ramp id and once per enum that wraps the categorical palette; Error for bad ramp files and clashing ids.
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on ramp file load.
- **Metrics:** ramps loaded, unknown-ramp fallbacks.
- **Health:** a failed ramp file degrades the module's health reporter (as in Feature 1).

## Acceptance Criteria

1. `ramps.json` loads the eight Dia ramps; continuous ramps have 2–16 strictly increasing stops from 0 to 1; categorical ramps have 2–32 colours.
2. A ramp file with a bad colour, non-increasing stops, too many stops, or a wrong `format`/`version` is rejected (the file, or for a single bad ramp that ramp only) with an Error; the other ramps still load.
3. A game ramp with the same id as a Dia ramp is rejected with an Error; Dia's ramp stays.
4. `FindRamp` returns a ramp by id and null for an unknown id.
5. `ResolveRamp` uses `ui.ramp`; with no `ui.ramp` it returns `sequential.grey`; with an unknown id it returns `sequential.grey` and logs one Warning per key.
6. `ResolvePalette` uses an enum value's `colour` when given and `categorical.12` by order when not; an enum with more than 12 colour-less values wraps and logs one Warning.
7. `ResolveDeltaRamp` returns a symmetric range `[-maxAbs, +maxAbs]` and uses `ui.delta_ramp` (default `diverging.blue_red`).
8. `SampleRamp` clamps `t` to [0, 1] and interpolates linearly in sRGB between stops.
9. `BuildLegend` gives up to 5 nice-number ticks including both ends, the unit, the seen-in-view range when supplied, and one swatch per enum value in order.
10. The sequential ramps have monotonically changing lightness; the diverging ramp is symmetric about its midpoint; `categorical.12` colours are pairwise at least the minimum Lab distance apart.
11. The TypeScript resolve, sample and legend functions match the shared conformance fixtures, as do the C++ ones.
12. `valueschema.get` returns the `ramps` array alongside the entries.
13. `DiaValueSchema` has no include or dependency on DiaGraphics or DiaBgfx (`dia check deps` clean).
14. Visual match against the mockup's colour legend.

## Test Strategy

- **GoogleTests (TDD):** ramp file loading and rejections; game-vs-Dia id clash; `FindRamp`; each resolve function including defaults and fall-backs; `SampleRamp` at fixed `t`; `BuildLegend` ticks for several ranges (including negative and tiny ranges); wrap warning; shared conformance fixtures.
- **Colour tests:** computed lightness monotonicity, diverging symmetry, categorical pairwise distance, in the same GoogleTests using a small Lab conversion in the test utilities.
- **Vitest:** the TS functions against the same conformance fixtures.
- **Query test:** `valueschema.get` includes `ramps`.
- **Dependency check:** `dia check deps` shows no DiaGraphics edge.
- **Test utilities** live with the module in `DiaValueSchema/Testing/`.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| VS-002 | Sidecar files keyed by format | Ramps are a separate `dia.ramps` file type, global ids, not keyed by `for` |
| VS-004 | Strict coverage; runtime clamp and warn | Unknown ramp ids fail in `dia validate schema` and fall back with one Warning at runtime |
| VS-005 | No singleton | Lookup through `ValueSchemaService` |
| PD-001 | Ids are StringCRC | Ramp ids hashed; collisions handled as in Feature 1 |
| PD-004 / AD-002 | No STL in public APIs; DiaCore containers | `RampData`, `PaletteData`, `LegendData` use fixed arrays or DiaCore containers |
| AD-001 (Dia) | New data file and module edits | `ramps.json` ships with the module; no new module |
| Engine layering | DiaValueSchema must not depend on the renderer | Neutral colour data; CellField conversion lives in DiaCellLayers |

No conflicts. This replaces the system spec's `ToCellFieldRamp` (amended in the system spec).

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Module, service, failure policy | Approved |
| Feature 3 | `valueschema.get` query to extend | Approved |
| DiaCellLayers Feature 1 | Converts `RampData` to CellField ramps | Draft (TBD) |
| CellField | Ramp stop limit (16) and 256 × 1 bake | Approved, not built; no code dependency |

## Open Design Questions

1. **Where `ramps.json` lives at runtime.** It ships beside the module source; the load path must resolve through the app's path aliases like sidecars. Decide the alias (a `dia_data` root, or the module's asset folder) when the module is scaffolded.
2. **Interpolation colour space.** sRGB linear interpolation matches CellField's baked texture and is simplest, but can look muddy across wide hue changes (heat, terrain). If it does on real maps, add more stops rather than change the colour space.
3. **More than 12 categories.** Wrapping reuses colours, which can mislead. If a real layer has more than 12 values, give those values explicit `colour`s; validate (Feature 2) could warn when an enum has more than 12 values and any lack a colour.

## Status

`Done` (2026-10-22). Plan: [ramps.plan.md](ramps.plan.md). C++ (AC 1-10, 13), TypeScript against the shared conformance fixtures (AC 11), `ramps` in `valueschema.get` (AC 12) and the visual match on the generated preview page (AC 14, user-confirmed) are all closed. Free-function resolvers became the `RampResolver` class (C++ and TypeScript) so the warn-once state stays out of the immutable service. `sequential.terrain` is a hypsometric ramp and is exempt from the monotonic-lightness rule (tests say so). `categorical.12` was chosen by search for colour-blind safety: all 12 are distinguishable but a few are dark or saturated (navy, brown, charcoal, lime); revisit if a real layer looks poor.
