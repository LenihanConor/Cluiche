# DiaCLI Reference

DiaCLI is the Python-based development CLI for the Cluiche platform. It provides environment provisioning, build pipeline execution, and test orchestration.

**Location:** `Dia/DiaCLI/`
**Entry point:** `python -m dia_cli` (from `Dia/DiaCLI/`)
**Python:** 3.11+ (managed via `.python-version` and `.venv/`)

---

## Quick Start

```bash
# From repo root, activate the venv:
Dia\DiaCLI\.venv\Scripts\activate

# From Dia/DiaCLI/:
python -m dia_cli --help
```

---

## Global Options

These flags apply to every command:

| Flag | Description |
|------|-------------|
| `--no-color` | Disable ANSI colour output |
| `--quiet` | Suppress terminal output (JSON log still written) |
| `--log-json PATH` | Override NDJSON log file path |

---

## Commands

### `dia env` -- Environment Management

Manages the Cluiche development environment: toolchain, SDK dependencies, git submodules, Docker, and AI context.

#### `dia env verify`

Read-only health check. Safe for CI.

```
dia env verify [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `--toolchain` | Check toolchain only (VS2022, Python, Git, Node, Docker) |
| `--deps` | Check deps.json dependencies only |
| `--submodules` | Check git submodules only |
| `--docker` | Check Docker only |
| `--claude` | Check Claude context only |
| `--json` | Machine-readable JSON output |
| `--quiet` | Print only WARNs and FAILs |

Exit codes: 0 = pass, 1 = fail, 2 = warn only.

JSON output schema:
```json
{
  "result": "pass|warn|fail",
  "checks": [
    {
      "name": "git",
      "category": "toolchain|deps|submodules",
      "status": "pass|warn|fail",
      "detail": "version or status info",
      "fix": "suggested remediation"
    }
  ]
}
```

#### `dia env setup`

Provision a fresh developer machine. Runs toolchain install, deps restore, submodule init, and Claude context wiring.

```
dia env setup [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `--toolchain` | Run toolchain install step only (requires admin) |
| `--deps` | Run deps restore step only |
| `--dep ID` | Restore a single named dependency |
| `--submodules` | Run submodule init step only |
| `--claude` | Run AI context wiring step only |
| `--force` | Re-run all steps even if sentinels are present |
| `--fail-fast` | Abort on first step failure |
| `--quiet` | Suppress progress output |

#### `dia env deps`

Restore binary SDK dependencies defined in `deps.json` at the repo root.

```
dia env deps [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `--dep ID` | Restore a single named dependency |
| `--force` | Re-download even if sentinel says up-to-date |
| `--quiet` | Suppress progress output |

Each dependency in `deps.json` has an `id`, `version`, `url`, `sha256`, and `unzip_to` field. Sentinels are written to `.diaenv/<id>/<version>.sentinel` to skip unnecessary re-downloads.

#### `dia env claude-setup`

Generate `.claude/settings.local.json` from the template and wire the memory symlink.

```
dia env claude-setup [--force]
```

#### `dia env docker`

Manage the Docker Windows Container build environment.

```
dia env docker image [--force]    # Build or pull the Docker image
dia env docker deps  [--force]    # Restore External/ deps inside the container
dia env docker paths [--force]    # Verify/configure PATH inside the container
```

---

### `dia pipeline` -- Build Pipeline

Runs the multi-stage build pipeline. Stages are defined in `pipeline.toml` at the repo root.

```
dia pipeline [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `--config CONFIG` | `Debug`, `Release`, or `Both` (default from `pipeline.toml`) |
| `--target TARGET` | Build target name (default from `pipeline.toml`) |
| `--stage STAGES` | Comma-separated stages to run (default: all for target) |
| `--force` | Re-run stages even if sentinels say up-to-date |
| `--docker` | Run all stages inside Docker container |

**Valid stages:** `proto-compile`, `compile-code`, `asset-build`, `package`

**Examples:**
```bash
# Full pipeline for default target
dia pipeline

# Debug build of googletest target only
dia pipeline --config Debug --target googletest

# Run only the proto-compile stage, forcing recompile
dia pipeline --stage proto-compile --force

# Build both Debug and Release
dia pipeline --config Both
```

**Pipeline stages:**

| Stage | What it does |
|-------|-------------|
| `proto-compile` | Compile `.proto` files via `protoc` |
| `compile-code` | Build C++ via MSBuild |
| `asset-build` | Build game assets (placeholder, exits 0) |
| `package` | Copy runtime files (DLLs, assets) to `$(OutDir)` |

---

### `dia test` -- Test Execution

Run the various Cluiche test suites.

#### `dia test cli`

Run the DiaCLI Python test suite (pytest).

```
dia test cli [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `--filter PATTERN` | Run only tests matching PATTERN (`-k` flag) |
| `--parallel` | Run in parallel via pytest-xdist (`-n auto`) |
| `--coverage-out PATH` | Write coverage XML report |
| `--docker` | Run tests inside Docker container |

Integration tests (marked `@pytest.mark.integration`) are excluded by default. Run them explicitly with:
```bash
python -m pytest tests/ -m integration
```

#### `dia test googletest`

Run the GoogleTests C++ test suite.

```
dia test googletest [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `--filter PATTERN` | Pass to `--gtest_filter` |
| `--config CONFIG` | `Debug` or `Release` (default: `Debug`) |
| `--verbose` | Enable `--gtest_print_time=1` |
| `--docker` | Run inside Docker container |

Expects the binary at `Cluiche/bin/<Config>/x64/GoogleTests.exe`.

#### `dia test ui`

Run the DiaApplicationEditor Vitest suite.

```
dia test ui [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `--filter PATTERN` | Pass to vitest `-t` flag |
| `--watch` | Run in watch mode (`npm run test:watch`) |
| `--docker` | Run inside Docker container |

Requires `node_modules/` in `Dia/DiaApplicationEditor/UI/`. Exit code 2 if missing.

#### `dia test env-integration`

Agentic end-to-end validation loop: provisions environment, runs build pipeline, executes tests, and optionally auto-fixes environment failures using Claude.

```
dia test env-integration [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `--skip-env` | Skip Stage 1 (assume container already provisioned) |
| `--max-auto-fixes N` | Max automatic fix attempts before prompting (default: 1) |
| `--no-fix` | Fail immediately on any error |
| `--docker` | Run all stages inside Docker container |
| `--inject-fault TYPE` | [test only] Inject a simulated fault |

**Stages:**
1. `dia env setup` -- Provision environment
2. `dia pipeline` -- Build all targets
3. `dia test cli` + `dia test googletest` -- Run test suites

Failures are classified as `env` (environment) or `logic` (code bug). Environment failures can be auto-fixed via the Anthropic API.

---

## Configuration Files

| File | Location | Purpose |
|------|----------|---------|
| `deps.json` | Repo root | Binary SDK dependency manifest |
| `pipeline.toml` | Repo root | Build pipeline stage and target definitions |
| `winget.json` | Repo root | Toolchain packages for `winget import` |
| `Directory.Build.targets` | Repo root | MSBuild auto-restore hook (calls `dia env deps`) |

---

## Log Output

All commands emit structured NDJSON events to `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson`.

Event types: `run_started`, `run_completed`, `run_failed`, `stage_started`, `stage_completed`, `stage_failed`, `log_line`.

Every event has a `ts` (ISO 8601 timestamp). Completed/failed events include `duration_ms`.

---

## Testing

The DiaCLI test suite lives in `Dia/DiaCLI/tests/`:

| File | What it tests | Count |
|------|--------------|-------|
| `test_dia_env.py` | DiaEnv system (deps, verify, setup, docker) | ~110 |
| `test_dia_pipeline.py` | DiaPipeline (config, stages, runner) | ~30 |
| `test_dia_test_cli.py` | `dia test cli` runner | ~11 |
| `test_dia_test_googletest.py` | `dia test googletest` runner | ~14 |
| `test_dia_test_ui.py` | `dia test ui` runner | ~14 |
| `test_dia_test_env_integration.py` | Agentic env-integration loop | ~16 |
| `test_dia_output.py` | OutputContext and NDJSON logging | ~20 |
| `test_integration.py` | Integration tests (real machine state) | 28 |

Run unit tests:
```bash
cd Dia/DiaCLI
.venv/Scripts/python -m pytest tests/ -m "not integration" -v
```

Run integration tests:
```bash
.venv/Scripts/python -m pytest tests/ -m integration -v
```

Run all:
```bash
.venv/Scripts/python -m pytest tests/ -v
```
