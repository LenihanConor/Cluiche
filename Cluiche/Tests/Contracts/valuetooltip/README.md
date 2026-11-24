# Value tooltip golden fixture

Shared contract for the DiaValueSchema Feature 3 tooltip. One `ValueInfo` plus renderer
options goes in; an ordered list of tooltip sections with exact text comes out.

- **Consumers (both MUST pass the same files):**
  - the debug panel's plain-JS `renderValueTooltip()` (panel page tests, Vitest/jsdom);
  - the React `ValueLabel` / `ValueTooltip` in `@dia/editor-ui` (`Dia/DiaEditorUI`, Vitest).
- **No producer.** The files are hand-authored, reviewed by diff. A renderer change that alters
  output must update `expected.json` and be re-verified against BOTH renderers.
- **Self-check:** `node Cluiche/Tests/Contracts/valuetooltip/check_fixture.mjs` verifies the two
  files agree (case ids, section names and order, section presence rules, and text) using a small
  reference implementation of the rules below.

Files: `entries.json`, `expected.json`, `check_fixture.mjs`.

## entries.json

`{ "format": "dia.value_tooltip_fixture", "version": 1, "entries": [ case, ... ] }`

Each case: `{ "id", "for", "entry", "options" }`.

- `for` - format id of the sidecar file; the full key is `<for>/<key>`.
- `entry` - a `ValueInfo` using sidecar field names: key, kind, label, description, hint, unit,
  type, min, max, default, affects, related, source, values [{id,label,description}],
  ui {group, step, decimals, ramp, layer, hidden}.
- `options` - renderer options:
  - `inspector` (bool) - the tooltip is on an inspector row (shows `hint`); sliders/panel rows are false.
  - `packId` (string or null) - pack overriding this value; non-null shows the pack badge.
  - `resolvable` (string[]) - keys from `related` that have a matching control on the panel
    right now. The renderer's "is this on the panel" lookup must be answered from this list.

## expected.json

`{ "format": "dia.value_tooltip_expected", "version": 1, "cases": { "<case id>": { "sections": [...] } } }`

`sections` is ordered; a section with no data is ABSENT. Order and shapes:

| section | present when | shape |
|---|---|---|
| `title` | always | `{text}` = `label` |
| `description` | always | `{text}` = `description`, never truncated |
| `enum_values` | `values` non-empty | `{values:[{id,label,description}]}` |
| `hint` | `options.inspector` and `hint` set | `{text}` |
| `meta` | any of unit/range/default | `{items:[{kind,text}]}`, kinds in order unit, range, default |
| `affects` | `affects` non-empty | `{chips:[key,...]}` in entry order |
| `related` | `related` non-empty | `{links:[{key,link}]}`; `link` true iff key in `options.resolvable`, else plain text |
| `source` | `source` set | `{source, defined_in}` where `defined_in` = `<for> · <key>` |
| `pack_badge` | `options.packId` set | `{text}` = `overridden by pack <packId>` |

`·` is U+00B7 (middle dot) with a space either side. Presence tests use "is defined", not truthiness:
`default: 0` and `min: 0` must still show.

## Formatting convention (both renderers implement exactly this)

- **Number (min, max):** shortest round-trip form (`String(n)` in JS): `0`, `2`, `0.05`. No padding.
- **Range:** both bounds: `min – max` (en dash U+2013, one space each side), e.g. `0 – 2`.
  Only `min`: `≥ min`. Only `max`: `≤ max`.
- **Default:** if it is a number and `ui.decimals` is set, `toFixed(decimals)` (`1.0` with 2 decimals
  gives `1.00`); otherwise shortest form / `String(value)` (`0` gives `0`).
- **Unit:** the `unit` string verbatim.
- Text fields (label, description, hint, source) are emitted verbatim: no trimming, truncation, or
  ellipsis. Any visual clamping is CSS only and must not change text content.

## Adding a case

1. Add the case to `entries.json` (unique `id`).
2. Add the matching entry under `cases` in `expected.json`, following the table above.
3. Run `node Cluiche/Tests/Contracts/valuetooltip/check_fixture.mjs`.
4. Run both renderer test suites; both must pass.
