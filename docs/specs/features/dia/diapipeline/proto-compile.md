# Feature Spec: proto-compile

## Parent System
@docs/specs/systems/dia/diapipeline.md

## Status
`Superseded` — protobuf codegen is now a `build_deps.protobuf` sub-step within `compile-code`. See [compile-code.md](compile-code.md). This spec is retained for historical reference.

## Summary

Implement `dia pipeline --stage proto-compile` — an automated stage that runs `protoc` on `Dia/DiaDebugProtocol/proto/debug_protocol.proto` and generates C++ headers into `Dia/DiaDebugProtocol/proto/generated/`. Currently this is a manual step; the `generated/` directory exists but is empty. This feature automates it and guards against unnecessary re-runs with a sentinel file.

## Problem

The `generated/` directory under `Dia/DiaDebugProtocol/proto/` exists but is empty. Generating the C++ protobuf headers from `debug_protocol.proto` is currently a manual, undocumented step. Any developer setting up a fresh clone or adding a new proto message must know to run `protoc` themselves, with the exact flags, pointing at the right include directories. This feature automates the step and makes it reproducible.

## Goals

- Run `protoc` on `debug_protocol.proto` and write C++ headers to `proto/generated/`
- Use `External/protobuf/protobuf-25.6/install-x64/bin/protoc.exe` — no PATH requirement
- Pass the correct `--proto_path` so that `import "google/protobuf/struct.proto"` resolves (well-known types are built into the `protoc` binary; no separate include dir needed for them)
- Guard against re-runs with a sentinel file: `.diaenv/proto/debug_protocol.sentinel`
- `--force` flag bypasses the sentinel and always re-runs
- Clear error message if `protoc.exe` is not found at the expected path

## Non-Goals

- Generating Python or JavaScript bindings — C++ only (spec can be extended later)
- Generating protos for files other than `debug_protocol.proto`
- Adding a new `.proto` file — out of scope for this feature
- Making `protoc.exe` available inside the Docker container — that is the `docker-build-env` feature's responsibility

## Protoc Invocation

```python
protoc_exe = repo_root / "External/protobuf/protobuf-25.6/install-x64/bin/protoc.exe"
proto_file = repo_root / "Dia/DiaDebugProtocol/proto/debug_protocol.proto"
proto_dir  = repo_root / "Dia/DiaDebugProtocol/proto"
output_dir = repo_root / "Dia/DiaDebugProtocol/proto/generated"

cmd = [
    str(protoc_exe),
    f"--proto_path={proto_dir}",
    f"--cpp_out={output_dir}",
    str(proto_file),
]
```

`google/protobuf/struct.proto` is a well-known type built into the `protoc` binary. No additional `--proto_path` is needed to resolve it.

## Sentinel Logic

Sentinel file: `.diaenv/proto/debug_protocol.sentinel`

- Before running: if sentinel exists and `--force` is not set, skip with log "proto-compile: up to date (sentinel present)"
- After successful `protoc` run: write sentinel (create `.diaenv/proto/` if needed)
- On failure: do not write sentinel; log the full `protoc` stderr output

The sentinel does not hash the `.proto` file — it only tracks whether the last run succeeded. This matches the simpler behaviour of the existing deps sentinel pattern (SD-ENV-005 parallel).

If the `.proto` file changes, the developer must run `dia pipeline --stage proto-compile --force` to regenerate. A future enhancement could hash the `.proto` file into the sentinel for automatic invalidation.

## Generated Output

`protoc --cpp_out` generates two files:
- `Dia/DiaDebugProtocol/proto/generated/debug_protocol.pb.h`
- `Dia/DiaDebugProtocol/proto/generated/debug_protocol.pb.cc`

Both files are gitignored (generated artefacts). The `generated/` directory itself is committed (with a `.gitkeep`).

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    └── commands/
        └── pipeline/
            └── stages/
                └── proto_compile_stage.py   # Stage handler
```

### `proto_compile_stage.py` responsibilities

```python
def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path) -> int:
    """Returns 0 on success/skip, 1 on failure."""
```

1. Check sentinel — skip if present and not `--force`
2. Verify `protoc.exe` exists at expected path — exit 1 with clear message if not
3. Ensure `output_dir` exists (`mkdir -p`)
4. Run `protoc` via `subprocess.run`, capture stdout/stderr
5. On non-zero exit: log stderr, return 1
6. On success: write sentinel, log generated file names, return 0

### Integration with `pipeline_runner.py`

`pipeline_runner.py` calls `proto_compile_stage.run(...)` when `"proto-compile"` is in the active stage list. The runner handles event emission (`OnStageStarted`, `OnStageCompleted`, `OnStageFailed`) around the call.

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `pipeline-config` feature | Hard | `PipelineConfig` provides `proto_dir` and `output_dir` |
| `protoc.exe` | System tool | `External/protobuf/protobuf-25.6/install-x64/bin/protoc.exe` |
| `subprocess` (stdlib) | Python | |

## Acceptance Criteria

1. `dia pipeline --stage proto-compile` generates `debug_protocol.pb.h` and `debug_protocol.pb.cc` in `Dia/DiaDebugProtocol/proto/generated/`
2. Running again without `--force` skips execution and prints "proto-compile: up to date (sentinel present)"
3. `dia pipeline --stage proto-compile --force` re-runs `protoc` even if sentinel exists
4. After a successful run, the sentinel file `.diaenv/proto/debug_protocol.sentinel` exists
5. If `protoc.exe` is not found at `External/protobuf/protobuf-25.6/install-x64/bin/protoc.exe`, exits 1 with "protoc not found at <path>"
6. `protoc` failure (non-zero exit) logs the full stderr output and exits 1 without writing sentinel
7. Generated headers compile cleanly when included in a C++ project (verified by compile-code stage succeeding after proto-compile)
8. `debug_protocol.proto` imports `google/protobuf/struct.proto` — this resolves correctly without any additional `--proto_path` flag

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaPipeline | @docs/specs/systems/dia/diapipeline.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — tooling |
| PD-003 | Platform | Component-based entities | Compliant — tooling |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only |
| PD-005 | Platform | x64 Windows only | Compliant — `protoc.exe` path is `install-x64`; no cross-platform support |
| PD-006 | Platform | VS project files are source of truth | Compliant — this feature does not modify any `.vcxproj` files |
| PD-007 | Platform | C++20 required | Compliant — generated `.pb.h` / `.pb.cc` files are consumed by C++20 projects; protoc output is C++20-compatible |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — proto output goes to `proto/generated/`, not `OutDir` |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — stage handler called by pipeline runner |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — 0 success/skip, 1 failure |
| SD-PIPE-001 | DiaPipeline | `pipeline.toml` is single source of truth | Compliant — `proto_dir` and `output_dir` read from `pipeline.toml [proto]` section |
| SD-PIPE-002 | DiaPipeline | Stage ordering fixed | Compliant — proto-compile runs first; this is stage 1 |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Well-known types | Does `protoc` need a `--proto_path` pointing at protobuf's own include dir for `google/protobuf/struct.proto`? | No — well-known types (`google/protobuf/*.proto`) are built into the `protoc` binary since protobuf 3.x. The only `--proto_path` needed is the directory containing `debug_protocol.proto`. |
| 2 | Generated files in git | Should `debug_protocol.pb.h` / `.pb.cc` be committed? | No — they are generated artefacts. They are gitignored. The `generated/` directory is committed with a `.gitkeep` so the path exists on a fresh clone. |
| 3 | Sentinel invalidation | What if someone edits `debug_protocol.proto` without running `--force`? | The sentinel does not detect source changes. The developer must run `--force` after editing a `.proto` file. A future enhancement (hash-based sentinel) can automate this. Documented as a known limitation. |
| 4 | protoc path | Should the protoc path be configurable in `pipeline.toml`? | Yes — the system spec's AI Review Q1 answer confirms: `pipeline.toml` can override with a `protoc_path` field. The `[proto]` section in `pipeline.toml` should accept an optional `protoc_path` key; if absent, the default path is used. |
| 5 | Output dir creation | What if `proto/generated/` does not exist? | `proto_compile_stage.py` creates it with `mkdir -p` before invoking `protoc`. The `.gitkeep` ensures the directory exists in the repo, but `mkdir -p` is a safety net for any edge case. |
