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
- Keys are dotted paths; record-table columns use `species[].growth_rate`; `path` is the location with record indexes (`species[2].growth_rate`); `record` is the enclosing record's `id`, else null.

## Rules

- A new data rule must add a fixture here.
- These are data files: they are not listed in any vcxproj.
