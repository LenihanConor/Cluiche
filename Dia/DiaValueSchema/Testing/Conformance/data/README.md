# DiaValueSchema data-file conformance cases

Self-contained scenarios for the data-file rules of `dia validate schema`. Consumers: Python pytest (`Dia/DiaCLI/tests/test_validate_schema_data.py`) and T-2's TypeScript live checks, so the editor and the CLI cannot disagree.

## Case file: `<case>.case.json`

```json
{ "description": "...",
  "schemas": [ { "format": "dia.value_schema", "version": 1, "for": "...", "entries": [ ... ] } ],
  "files": [ { "name": "Global/Ecology/ecology_parameters.json", "pack": false,
               "content": { "format": "...", "version": 1 } } ],
  "expected": [ { "severity": "FAIL", "rule": "data.out-of-range", "file": "Global/Ecology/ecology_parameters.json",
                  "key": "grazing.rate", "path": "grazing.rate", "record": null,
                  "messageContains": "2.5", "suggestion": null } ] }
```

- `schemas`: full `dia.value_schema` documents; each data file is checked against the schemas whose `for` equals its `format`.
- `files[].pack`: true for a pack (partial override; defaults are not checked), false for a defaults file.
- `expected`: every finding of every file, in any order. `[]` means all files are clean. Findings are matched on severity, rule, file, key, path, record, `suggestion` (null = none) and `messageContains` (a substring of the message). Extra or missing findings fail the case.
- `schemaExpected` (optional, default `[]`): findings of the schema-level checks (`affects`/`related` resolution, `ui.ramp`) over `schemas`, same shape as `expected`. The `file` of such a finding is `schemas[<index>]`.
- Cross-format rules (`ref`, `ref[]`, `ref[2][]`, `map` keys) resolve against ALL `files` of the case, grouped by `format` (defaults and packs together). A file whose format has no schema is still a source of ids. A ref to a format with no files in the case gives one WARN (`data.ref-format-missing`) per file and format.
- Keys are dotted paths; record-table columns use `species[].growth_rate`; `path` is the location with record indexes (`species[2].growth_rate`); `record` is the enclosing record's `id`, else null.

## Rule ids

Data (per file): `data.no-schema-entry`, `data.wrong-type`, `data.out-of-range`, `data.bad-enum`, `data.default-mismatch`, `data.unresolved-ref`, `data.ref-format-missing` (WARN), `data.duplicate-unique`, `data.missing-required`, `data.unresolved-map-key`, `data.incomplete-map`, `data.constraint-violated`, `data.nested-group-pack`. `data.default-mismatch` applies to `kind: field` entries only. For a `ref[]` item or a map value the `path` extends to the item (`species[0].diet[1]`, `species[0].affinity.Desert`). Schema-level: `schema.unresolved-reference`, `schema.unknown-ramp`.

## Rules

- Schema-level default checks (`data.unresolved-map-key` and `data.incomplete-map` for object map defaults (one `data.incomplete-map` per entry naming every keyRef id the default omits, path `default`; the C++ load fails naming the first; packs stay sparse and are not checked),  `data.constraint-violated` on defaults) go in `schemaExpected`; a violated constraint on a pack is reported on the pack file.
- The `cow.biomes` file (ids TemperateForest, Desert, Tundra) in the map cases is the shared keyRef key data file; the C++ conformance tests reuse it.
- Packs use FLAT keys (2026-11-20 user decision): a parameter is addressed by its full key `<group>.<key>`, the same key language as `ParameterFile::Set` and map columns `<key>.<mapKey>`. A nested group object in a pack (`{"grazing": {"rate": 2.5}}`) is `data.nested-group-pack` (key and path are the group name, the message names the flat key); `ApplyPack` drops it with a warning (`nested-group-pack.case.json`). An object under a MAP parameter name is the map-columns form and stays valid; nesting in a defaults file and inside record tables is unchanged.
- A pack may carry a top-level `basedOn` (hex schema hash, written by `ParameterFile::ToJson(Diff)`); it is envelope, not a parameter, so it needs no entry (`parameter-file-to-json-diff.case.json` is the committed `ToJson(Diff)` output).
- A new data rule must add a fixture here.
- These are data files: they are not listed in any vcxproj.
