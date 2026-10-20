# Feature Spec: `dia validate schema`

**Parent:** @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md
**Builds on:** @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md (Feature 1)

**Gameplay Domains:** tools

## Summary

A DiaCLI command that checks every value-schema sidecar and every data file it describes, before anything runs. It enforces VS-004: every data key has an entry with a description, values sit inside their range, defaults match, cross-references resolve, record tables have unique and required columns. It fails on real errors, warns on coverage gaps, and runs on changed files in `dia docs precommit`. It is a Python command next to `cli_validate.py`.

## Problem

Tooltips and editors are only as good as the schema behind them. Without a validator a new parameter ships with no description, a pack is saved with an out-of-range value, or a species refers to a biome that was renamed, and the first sign is a blank tooltip or a clamped value at runtime. The check has to run without building the game.

## Goals

1. **Fail early.** Every FAIL rule in the system spec is checked from files alone, with no build and no running app.
2. **Pair files by format.** A data file is checked against the schema whose `for` equals the file's `format` (F-3 envelope).
3. **Helpful messages.** Each finding names file, key and reason; a broken reference or unknown key gets one "did you mean" when something is close.
4. **Same rules as the runtime loader.** The format rules the validator applies are tested against the same fixtures as Feature 1's C++ loader.
5. **Fast enough for precommit.** Changed files only, in under a second for typical changes.
6. **Machine-readable.** `--json` gives the findings for the editor and CI.

## Non-Goals

- Loading or evaluating the game's data (F-3 and each system do that).
- Checking that a description is *good*; only that it exists and is not empty.
- Auto-fixing findings.
- SARIF output (no consumer yet).
- Code-side coverage when no schema export exists (see Code-side coverage).

## Architecture

New files under `Dia/DiaCLI/dia_cli/`, following the existing pattern (`cli/cli_validate.py` registers commands):

```
cli/cli_validate.py                      registers `schema` as a subcommand of `dia validate`
commands/validate/schema_cmd.py          click command, scope resolution, output
commands/validate/schema_rules.py        pure rule functions (format rules + data rules)
commands/validate/schema_suggest.py      closest-match suggestions
commands/validate/schema_discovery.py    find schema files, data files, pair them by format
```

Rules are pure functions over parsed JSON so they can be tested without files.

### Command

```
dia validate schema [PATHS...] [--target T] [--json] [--code-side PATH]
```

- **PATHS:** files or folders to check. Default: every `*.schema.json` and every data file reachable from the app's path aliases for `--target`.
- **Exit code:** 0 when there are no FAILs (warnings do not change it); 1 when any FAIL; 2 for usage errors such as an unknown target.
- **Output:**
  ```
  FAIL Global/Ecology/ecology_parameters.json
       key 'grazing.juvenile_rate' has no schema entry (did you mean 'grazing.juvenile_rate_scale'?)
  FAIL Global/Ecology/Packs/lush.ecology.json
       'grazing.rate' = 2.5 outside [0, 2]
  WARN metric cow.ecology.herbivores_total has no MetricInfo
  OK   Global/Weather/weather_parameters.json (24 keys)
  ```
  `--json` prints `[{ severity, rule, file, key, message, suggestion? }]`.

### Discovery and pairing

1. Find every `*.schema.json` under the alias roots; parse each. A schema file that fails the format rules is a FAIL itself and its `for` is not used.
2. Find candidate data files: `*.json` under the alias roots (excluding schema files, catalogue files and `out/`) whose top-level object has a `format` string.
3. Pair each by `format == for`.
4. A data file whose `format` matches no schema is a **WARN** ("format `X` has no schema"), not a FAIL: a game opts in by shipping a schema for a format.
5. A data file inside a schema'd folder with **no** `format` is a FAIL ("no `format`; cannot find its schema"). Other JSON in unrelated folders is ignored.

A file is a **pack** when it sits in a `Packs/` folder; otherwise it is a **defaults file**. Defaults files are checked for `default` matches and for completeness; packs are partial overrides (F-3) and are not.

### Rules

**Schema file rules (FAIL), the same as Feature 1's loader:**
- wrong `format`/`version`; missing `for`, `entries`, or a required entry key (`key`, `kind`, `label`, `description`, `type`); wrong key types; unknown `kind` or `type`; an `enum` with no `values`; a duplicate full key; an empty `description`.
- Unknown keys inside `ui` and `history` are **WARN** (open blocks); unknown core keys are FAIL.

**Data file rules (FAIL):**
- a leaf key with no schema entry (envelope keys `format` and `version` are exempt; bookkeeping keys such as `id` still need an entry, marked `ui.hidden`);
- a value outside `min`/`max`, or not one of an enum's ids, or of the wrong `type`;
- in a defaults file: a `default` that differs from the file's value;
- an `affects`/`related` key that does not resolve (checked on the schema);
- an unknown `ui.ramp` (checked against the Feature 5 ramp names; until Feature 5 exists, the five names in the system spec);
- a `ref`/`ref[]` that matches no id in the referenced format's record array, with a suggestion;
- a duplicate value in a column marked `unique`;
- a record missing a column marked `required` (record arrays only).

**Record paths:** an entry key like `species[].growth_rate` describes a column; the validator walks arrays of objects and applies the entry to every record.

**Warnings:**
- a format with no schema (above);
- an `ui`/`history` unknown key;
- code-side coverage gaps (next section).

### Code-side coverage

Metrics, command arguments and component fields are described in code (Feature 4, Feature 7) and reach the validator only through the schema export (Feature 6, `Cluiche/out/<App>/valueschema.json`).

- If the export exists (or `--code-side PATH` is given), the validator reads it and WARNs about metrics with no `MetricInfo`, commands with no `ValueParam`, and component fields with a range but no description.
- **Grouped output (amended 2026-10-20, Feature 4):** these coverage warnings are grouped by owner with a count, for example `WARN DiaAssets: 41 commands without argument descriptions`, and the individual items are listed only with `--verbose`. `--json` always lists every item. Commands that set `noArgs` are never reported.
- **Unit lint (amended 2026-10-20, Feature 4):** a metric whose name ends in a known unit suffix (`_ms`, `_us`, `_bytes`, `_kb`, `_mb`, `_hz`, `_pct`) and whose `MetricInfo.unit` is empty is a WARN. Unit text is not compared with the suffix.
- If it does not exist, the code-side checks are skipped and the run says so on one line. Validation never requires a build.
- The export's age is not checked in v1 (see Open Q2).

### Suggestions

`schema_suggest` returns at most one suggestion, the closest valid key or id by `difflib` ratio, only when the ratio is at least 0.6. If nothing is close it returns none.

### Precommit

`dia docs precommit` runs `dia validate schema` on the changed `*.schema.json` files and the changed data files whose format has a schema, and reports its findings under a "schema" heading. FAILs block; WARNs do not.

### Logging and observation

This is a CLI tool: it prints a one-line summary (files checked, FAIL, WARN) and, with `--json`, the findings. No runtime pillars apply.

## Acceptance Criteria

1. A sidecar breaking each schema-file rule (missing key, wrong type, unknown `kind`/`type`, empty description, duplicate full key, enum with no values) produces a FAIL naming file and entry.
2. A data file key with no schema entry is a FAIL, with a suggestion when a close key exists and none otherwise.
3. A value outside `min`/`max`, not in an enum, or of the wrong type is a FAIL naming the key, the value and the allowed range.
4. In a defaults file, a `default` that differs from the file's value is a FAIL; in a pack it is not checked.
5. A `ref` pointing at an id not in the referenced format's records is a FAIL with a "did you mean" suggestion when one is close.
6. A duplicate in a `unique` column, and a record missing a `required` column, are FAILs.
7. Record paths (`species[].growth_rate`) apply to every record in the array.
8. An `affects`/`related` key that does not resolve, and an unknown `ui.ramp`, are FAILs.
9. A data file whose `format` has no schema is a WARN; a file in a schema'd folder with no `format` is a FAIL; unrelated JSON is ignored.
10. The envelope keys `format` and `version` need no schema entry; every other leaf key does.
11. With a schema export present, a metric with no `MetricInfo`, a command with no `ValueParam` and a ranged component field with no description are WARNs; with no export, the code-side checks are skipped with a one-line notice.
12. The exit code is 0 with only warnings, 1 with any FAIL, 2 on a usage error; `--json` output lists every finding with severity, rule, file, key and message.
13. The command checks Feature 1's shared conformance fixtures and reaches the same verdicts as the C++ loader tests on every fixture.
14. `dia docs precommit` runs it on changed schema/data files and fails the commit on a FAIL.
15. Code-side coverage warnings are grouped by owner with counts, the individual items appear only with `--verbose` (and always in `--json`), and a command that sets `noArgs` is never reported.
16. A metric named with a unit suffix (`_ms`, `_us`, `_bytes`, `_kb`, `_mb`, `_hz`, `_pct`) and an empty unit produces a WARN; a non-empty unit does not, whatever its text.

## Test Strategy

- **pytest:** one fixture per FAIL rule and the OK path; pure rule functions tested without files; suggestion thresholds (close, not close, empty); discovery and pairing; pack vs defaults; code-side coverage with and without an export; exit codes and `--json` shape.
- **Shared conformance fixtures:** a folder of good and bad schema files with an `expected.json` verdict per file, kept under `Dia/DiaValueSchema/Testing/Conformance/`. Feature 1's GoogleTests and this feature's pytest both load every file there and compare to `expected.json`. A new format rule must add a fixture.
- **Real data proof (when CoW content exists):** `dia validate schema` passes on every F-3 defaults file and pack in the repo.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| VS-002 | One schema per data-file format | Pairing by `format` == `for` |
| VS-004 | Strict coverage; fail on missing entry/description or out-of-range | The FAIL rules above |
| VS-003 | Metrics and command args are described in code | Checked only through the export; no sidecar fallback |
| PD-009 | Generated output under `Cluiche/out/` | The export is read from `Cluiche/out/<App>/`; nothing written outside it |
| DiaCLI rule | Never bypass the CLI; fix the CLI | Validation is a `dia validate` subcommand and runs in precommit |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Format rules and shared conformance fixtures | Approved |
| F-3 Feature 1 envelope (`format`, `version`) | Pairing | Specced (CoW); the validator works on any file that has an envelope |
| Feature 6 export | Code-side coverage | Draft, TBD |
| Feature 5 ramp names | `ui.ramp` check | Draft, TBD (fixed list until then) |
| DiaCLI `dia validate`, `dia docs precommit` | Hosting | Done |

## Open Design Questions

1. **Hidden bookkeeping keys.** The rule that every leaf key needs an entry (including `id`, `parent` and similar) is strict but adds many `ui.hidden` entries. If it proves noisy, allow a schema-level `ignore` list. Decide after F-3's first real packs.
2. **Stale exports (resolved 2026-10-20, Feature 6).** The export header records `git_commit` and `dirty`; the validator prints one WARN when they differ from HEAD and still uses the export. The code-side coverage lists come from the export's `undescribed` section.
3. **Pack detection by folder.** Treating a `Packs/` folder as the sign of a pack is a convention from the F-3 layout. If F-3 changes where packs live, the rule has to follow; check against F-3 Feature 1.

## Status

`Approved` (2026-10-20). Plan not yet created.
