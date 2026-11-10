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
