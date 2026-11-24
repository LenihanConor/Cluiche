# World-Sim Naming Conventions

**Type:** Reference
**Spec:** @docs/specs/applications/cow/systems/worldstep/worldsim-kit.md (section 5)
**Research:** @docs/research/worldsim_data_driven/summary.md (section 0.1)

Every world-sim system (S-1 worldgen, A4 weather, A5 hydrology, A1 ecology, A3 succession) names its commands, layers, schema keys, saves, formats, files and stages from one system token. `Cluiche/CoW/Shared/World/WorldSimNames.h` builds each name from the token; kit code uses only those builders. Fields and verbs are `snake_case` (lowercase letters, digits, underscores).

## Tokens

`worldgen`, `weather`, `hydro`, `ecology`, `succession` (`<sys>` below). Stage ids are the tokens themselves; the WorldStepModule chain order is `weather`, `hydro`, `ecology` (`World::kStageOrder`).

## Patterns

| Thing | Pattern | Example | Builder |
|---|---|---|---|
| Commands | `cow.<sys>.<verb>` | `cow.hydro.set_param` | `CommandName` |
| Dotted subcommands | `cow.<sys>.<group>.<verb>`, every segment snake_case | `cow.worldgen.preset.load` | none (see note) |
| Param verbs | `params set\|get\|reload\|list` | | |
| Layer ids | `<sys>.<field>` | `hydro.soil_moisture` | `LayerId` |
| Schema keys, layers | `cow.<sys>.layers/<field>` | `cow.hydro.layers/soil_moisture` | `LayerSchemaKey` |
| Schema keys, params | `cow.<sys>.params/<key>` (key may be dotted) | `cow.weather.params/snow.melt_threshold` | `ParamSchemaKey` |
| Save ids | `cow.<sys>` | `cow.weather` | `SaveId` |
| Formats | `cow.<sys>_state`, `cow.<sys>_params`, `cow.<sys>_preset`, `cow.biomes` | | `StateFormat`, `ParamsFormat`, `PresetFormat` |
| Parameter files | `Global/<System>/<sys>_parameters.json`, sidecar `<same>.schema.json` beside it | | |
| Packs | `Global/<System>/Packs/<id>.<sys>.json` | | |
| Presets | see Presets below | | `PresetPath` |
| Metrics | `cow.<sys>.<metric>` | `cow.weather.step_ms` | `MetricName` |
| Stage ids | `<sys>` | `weather` | `StageId` |
| Events | `<System><Thing>`, declared in `.diagamemessages` | `WeatherUpdated` | |

Note: `CommandName` accepts a single snake_case verb only, so it rejects S-1's dotted `preset.load` and `preset.save`. The pattern test checks those by segment.

## Presets

Format `cow.<sys>_preset`, file `Global/<System>/Presets/<id>.<sys>_preset.json` (`<System>` is the capitalised token), built by `PresetFormat` and `PresetPath`. This replaces `.weatherpreset.json`, `.hydropreset.json` and `cow.hydrology_preset`. S-1's `cow.worldshape` presets are a different concept and are unchanged. A preset's parameter values form their own pack layer, applied last and replaced by the next `ApplyPreset`; user packs are kept.

## Log channels

Short codes: `WGEN` (S-1 worldgen, in the CoW module and shared files), `WTH` (weather), `HYD` (hydrology), `ECO` (ecology). Kit-level code logs on `WorldSim`. The Draoi generator keeps `HexWorld` and is not renamed.

## Unknown biome

Log once per id, then use the biome registry's `fallback` biome (`ResolveOrFallback`). Code does not hard-code a fallback row or constant. Source: research section 0.1; A4 decision 13 and the A5 hydrology spec.

## Enforcement

`Cluiche/Tests/GoogleTests/CoW/WorldSimKit/TestWorldSimNamesRegistrations.cpp` walks S-1's real registrations (the 18 command names, save id, stage ids, schema family) against these patterns. The layer id and layer schema key check is committed `DISABLED_` until the S-1 layer rename (kit plan Task 34), because today's 35 ids are camelCase without the `worldgen.` prefix. Later kit users add their registrations to the same test.
