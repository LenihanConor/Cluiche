# StageCatalogue fixtures

Shared by the C++ loader (CluicheGameBaseline) and the Python loader (`dia_cli/utils/stage_catalogue.py`).
Both must produce output equal to `expected_catalogue.json`.

- `basic/` - valid set. Boot transitions order: TestRig, NoImport, ShowTest, ToolBench, Misc, Plain, ShowcaseDemo
  (differs from import order). `NoImport` has no `.diastage` (listed untagged). `NotListed` is imported but not in
  Boot's transitions (omittedCount 1). `ShowTest` tags are `["test","showcase"]`; kinds are reported in canonical
  order showcase, tool, test. `Misc` has only a free-form tag and `Plain` has none, so both get an `other` row.
- `over_cap/` - 65 stages (Stage01..Stage65) in Boot's transitions; imports are listed in reverse so order must come
  from transitions. 64 kept, Stage65 dropped (droppedCount 1).
- `malformed/game.diagame` - invalid JSON, expects Malformed.
- `no_root_manifest/` - valid JSON, no `"type":"manifest"` import, expects NoRootManifest.
- `missing` - no files; tests pass a nonexistent path, expects MissingDiagame.

`expected_catalogue.json`: per fixture `result`; for `basic` the ordered `entries` (`name`, `tags` as written in the
file, `kinds`, `description` with "" when absent), `rows` (`group` in order showcase, tool, test, other; within a
group ascending `entry` index; `entry` indexes into `entries`), `omittedCount`, `droppedCount`. For `over_cap` only
`entryCount`, `droppedCount`, `omittedCount`.
- `presets/` - `WithPresets` (presets `alpine42` with a description and an override, `bare` with none) and `NoPresets`.
  Used by the launch-presets tests (catalogue carries preset names and descriptions, never the override JSON); not in `expected_catalogue.json`.
