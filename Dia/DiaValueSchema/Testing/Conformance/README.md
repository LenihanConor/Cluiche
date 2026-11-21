# DiaValueSchema conformance fixtures

Shared good and bad `dia.value_schema` v1 files. Each `*.schema.json` here has one verdict in `expected.json`.

Consumers:
- C++: `Cluiche/Tests/GoogleTests/DiaValueSchema/TestValueSchemaConformance.cpp` loads every file with `ValueSchemaLoader` and checks it against `expected.json`.
- Python: `dia validate schema` (Feature 2) runs the same set in pytest, so the C++ loader and the Python validator cannot drift.

## expected.json

```json
{ "format": "dia.value_schema.conformance", "version": 1,
  "cases": { "<file name>": { ... } } }
```

Good case: `"ok": true`, `"entryCount": N`, `"for": "<file-level for>"`, optional `"warnings": N` (count of non-fatal warnings).

Bad case: `"ok": false`, `"failedEntry": "<entry key>"` (empty string for a file-level failure or an entry with no usable key), `"reasonContains": "<short substring of the reason>"`.

Keep `reasonContains` short and stable (usually the offending key name) so wording tweaks do not break it.

## Rules

- A new format rule must add a fixture here (a bad file for each rejection, a good file for each new accepted shape) and an `expected.json` entry.
- Every `*.schema.json` needs an `expected.json` entry and every entry needs a file; the GoogleTest fails on drift.
- These are data files: they are not listed in any vcxproj.

## CLI stricter than the loader

`dia validate schema` deliberately rejects some inputs the C++ `ValueSchemaLoader` accepts (the loader checks shape, later stages and the CLI check meaning). These are not fixtures here (a fixture has one verdict for both sides); each is pinned on both sides in `Dia/DiaCLI/tests/test_validate_schema_parity.py` (Q3) and, where C++ is the accepting side, in `TestValueSchemaConformance.cpp`:

- Empty or blank `description` (`schema.empty-description`).
- Duplicate full key in one file (`schema.duplicate-key`; the registry resolves it first-wins).
- `keyRef` (and `ref`) not shaped `<format>/<record path>` (`schema.ref-malformed`); the loader accepts any `keyRef` string and the key source is resolved later by `FromValueSchema`.
- `keyEnum` (a map keyed by an enum entry of the same file, instead of `keyRef`): both checks (`fields 'keyRef' and 'keyEnum' are mutually exclusive`, `keyEnum '<target>' names no entry in this file`, `keyEnum '<target>' is not an enum entry`) use the same wording in the loader and the CLI (`schema.map-misuse`); fixtures `good-map-key-enum`, `bad-map-keyref-and-keyenum`, `bad-map-keyenum-target-missing`, `bad-map-keyenum-target-not-enum`, `bad-keyenum-on-non-map`.
- Every bad entry is reported, where the loader stops at the first.

Data-side findings (`data.*`, including `data.constraint-violated` and `data.unresolved-map-key`) are implemented by the CLI only until `FromValueSchema` exists; the C++ side checks that the schemas of every `data/*.case.json` load and read back the declared structure.
