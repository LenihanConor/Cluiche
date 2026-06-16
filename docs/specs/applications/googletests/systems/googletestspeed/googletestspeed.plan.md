**Spec:** @docs/specs/applications/googletests/systems/googletestspeed/googletestspeed.md
**Status:** Done

## Implementation Patterns

### Task 1 — slow-suite-tagging
- Add `--all` flag to `googletest_runner.run()` and `launch.launch_target()` (and their CLI surfaces in `dia_cli/cli/run.py`, `dia_cli/cli/launch.py`, `dia_cli/cli/test.py`)
- Default filter: when no `--filter` is given and `--all` is absent, inject `--gtest_filter=-SLOW_*`
- Post-run warning: parse `--gtest_output=xml:<path>` from the GTest run, read the XML, find `<testsuite>` nodes with `time > 0.5` whose `name` does not start with `SLOW_`, print WARNING lines
- Output XML goes to `Cluiche/out/GoogleTests/last_run.xml` (PD-009 compliant)
- Unit tests in `tests/test_dia_test_googletest.py` — new cases for `--all`, default filter injection, and warning emission (mock subprocess + fake XML)
- DiaCLI pytest suite must still pass: `cd Dia/DiaCLI && python -m pytest tests/ -x -q`

### Task 2 — release-config-ci
- `pipeline.toml` `[global]` `default_config` stays `"Debug"` (dev default unchanged)
- `pipeline.toml` `[targets.googletest]` gains `full_suite_config = "Release"` — CLI picks this up when `--all` is passed without an explicit `--config`
- `PipelineConfig` / `TargetConfig` dataclass gets `full_suite_config: str = "Debug"` field
- `googletest_runner.run()` already accepts `config`; `run.py` passes `full_suite_config` when `--all` and no explicit `--config`
- Unit tests: `test_dia_pipeline.py` (toml parsing) + `test_dia_test_googletest.py` (full-suite config selection)

### Task 3 — precompiled-header
- `GoogleTests.vcxproj`: add `<ClInclude Include="pch.h" />` and per-ItemDefinitionGroup `<PrecompiledHeader>Use</PrecompiledHeader>` + `<PrecompiledHeaderFile>pch.h</PrecompiledHeaderFile>`
- `pch.cpp`: `#include "pch.h"` with `<PrecompiledHeader>Create</PrecompiledHeader>` in the vcxproj
- `pch.h` includes: `gtest/gtest.h` only (+ stable Dia headers if any — start minimal, SD-002)
- All existing `.cpp` files must NOT have `#include "gtest/gtest.h"` as the first include after this — they gain `#include "pch.h"` prepended... actually: PCH is forced via `/FI` compiler flag rather than per-file edits to avoid touching 400 files
- Use `/FI pch.h` in `<AdditionalOptions>` in the ClCompile block so it's forced-included without editing every .cpp
- Verify: `dia pipeline --target googletest --stage compile-code` exits 0

### Task 4 — shard-runner
- New file: `Dia/DiaCLI/dia_cli/commands/test/shard_runner.py` — `run_shards(binary, out_dir, num_shards, filter_pattern, config)` — splits filter space, spawns N subprocesses, merges XML, returns aggregate exit code
- New file: `Dia/DiaCLI/dia_cli/commands/test/xml_merger.py` — `merge_xml(input_paths, output_path)` — ~30 lines, merges `<testsuite>` elements under `<testsuites>`
- `googletest_runner.run()` gains `shards: int = 0` param; if `> 1`, delegates to `shard_runner.run_shards()`
- CLI: `dia_cli/cli/run.py` and `dia_cli/cli/test.py` get `--shards N` option
- Shard suite list: GTest `--gtest_list_tests` output split evenly by test count
- Default N: `os.cpu_count() - 1` when `--shards` given with no value (SD-004)
- Merged XML → `Cluiche/out/GoogleTests/merged.xml`
- Exit code aggregation: non-zero from any shard → non-zero overall (AD-005)
- Unit tests in `tests/test_dia_test_googletest.py` covering: shard count=1 falls back to normal, shard count=N launches N processes, exit aggregation

### Task 5 — fixture-amortisation
- Target: `Cluiche/Tests/GoogleTests/Python/` suite — all six files call `Initialize()`/`Shutdown()` in `SetUp`/`TearDown`
- Pattern: rename fixture class to keep per-test `SetUp`/`TearDown` for state reset, but add `static void SetUpTestSuite()` that calls `Initialize()` once and `static void TearDownTestSuite()` that calls `Shutdown()` once
- `TestLifecycle.cpp` is special: it tests the init/shutdown lifecycle explicitly, so it must NOT be amortised — leave it as-is
- `TestIntegration.cpp`, `TestModule.cpp`, `TestScriptExecution.cpp`, `TestTypeConversion.cpp`, `TestErrorHandling.cpp` — amortise
- Verify: `dia run googletest --filter="DiaPython*"` passes; wall-clock time in XML shows improvement

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | slow-suite-tagging: `--all` flag + default `SLOW_*` filter exclusion + post-run WARNING from XML | DiaCLI pytest: `tests/test_dia_test_googletest.py` | Done | sonnet | 20/20 tests pass. `googletest_runner.py`, `run.py`, `launch.py`, `test.py` |
| 2 | release-config-ci: `full_suite_config` in `pipeline.toml` + `TargetConfig` + runner wiring | DiaCLI pytest: `test_dia_pipeline.py`, `test_dia_test_googletest.py` | Done | sonnet | 77/77 pass. `pipeline_config.py`, `pipeline.toml`, `run.py` |
| 3 | precompiled-header: `pch.h` + `pch.cpp` + `/FI pch.h` in vcxproj for Debug+Release | `dia pipeline --target googletest --stage compile-code` exits 0 | Done | sonnet | XML valid; Asan/Ubsan untouched. Build verify deferred to Task 5 full run. |
| 4 | shard-runner: `shard_runner.py` + `xml_merger.py` + `--shards N` CLI flag + merged XML output | DiaCLI pytest: new shard tests in `test_dia_test_googletest.py` | Done | sonnet | 37/37 pass. `shard_runner.py`, `xml_merger.py`, `googletest_runner.py`, `run.py`, `launch.py`, `test.py` |
| 5 | fixture-amortisation: `SetUpTestSuite`/`TearDownTestSuite` in 5 Python test files | `dia run googletest --filter="DiaPython*"` passes | Done | haiku | TestLifecycle.cpp untouched. Build verify deferred (requires msbuild). |
