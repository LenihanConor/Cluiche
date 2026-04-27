# Feature Spec: docker-execution

## Parent System
@docs/specs/systems/dia/diapipeline.md

## Status
`Done`

## Summary

Implement `dia pipeline --docker` — re-invokes all requested pipeline stages inside the Docker container via `docker run`, with the repo volume-mounted. All stage logic runs identically inside the container; the host-side `--docker` handler is a thin re-invocation wrapper. Exit 3 if the Docker image is not found.

## Problem

`dia test env-integration` requires `dia pipeline` to run inside the Docker container (SD-TEST-001). Without `--docker` support, the pipeline is host-only and cannot be used for the integration test loop.

## Goals

- `dia pipeline --docker` re-runs the identical command inside the container via `docker run`
- Volume-mount the repo root so all paths resolve identically to the host
- Pass `ANTHROPIC_API_KEY` from the host environment into the container if present
- Exit 3 with a clear message if the Docker image does not exist
- Stream container stdout/stderr to the host terminal in real time
- All other pipeline flags (`--config`, `--target`, `--stage`, `--force`) are forwarded unchanged to the in-container invocation

## Non-Goals

- Building the Docker image — owned by `docker-build-env` (DiaEnv)
- Running with GPU or GUI — Docker scope is headless (SD-ENV-008)
- Persistent container state between invocations — each `--docker` run is a fresh `docker run`

## Docker Invocation

```python
def run_in_docker(cli_args: list[str], repo_root: Path, image_name: str) -> int:
    """Re-invoke dia pipeline inside Docker with the same args (minus --docker)."""
    docker_cmd = [
        "docker", "run", "--rm",
        "--volume", f"{repo_root}:C:/repo",
        "--workdir", "C:/repo",
    ]

    # Forward API key if present
    if os.environ.get("ANTHROPIC_API_KEY"):
        docker_cmd += ["--env", "ANTHROPIC_API_KEY"]

    docker_cmd += [image_name, "python", "-m", "dia_cli"]
    docker_cmd += ["pipeline"] + cli_args  # cli_args has --docker stripped

    result = subprocess.run(docker_cmd)
    return result.returncode
```

The `image_name` is resolved from `pipeline.toml [global] docker_image` (optional field; defaults to `cluiche-build-env:latest`).

## Image Existence Check

Before invoking `docker run`, check that the image exists:

```python
result = subprocess.run(["docker", "image", "inspect", image_name], capture_output=True)
if result.returncode != 0:
    logger.error(f"Docker image '{image_name}' not found. Run `dia env docker image` to build it.")
    return 3
```

## Args Forwarding

`--docker` is stripped from the forwarded args list. All other flags (`--config`, `--target`, `--stage`, `--force`) are passed through verbatim. This means:

```bash
# host invocation
dia pipeline --docker --stage compile-code --config Release --target cluichetest

# becomes inside container
python -m dia_cli pipeline --stage compile-code --config Release --target cluichetest
```

## Implementation

### Files modified

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── pipeline_cmd.py          # Add --docker flag handling; call run_in_docker before stage dispatch
    └── commands/
        └── pipeline/
            └── pipeline_runner.py   # Add docker_image resolution; no new file needed
```

No new file is needed — docker invocation is a pre-stage intercept in `pipeline_cmd.py`:

```python
@cli.command()
@click.option("--docker", is_flag=True, default=False)
@click.pass_context
def pipeline(ctx, docker, **kwargs):
    if docker:
        args = build_forwarded_args(ctx)  # rebuild CLI args without --docker
        sys.exit(run_in_docker(args, repo_root, image_name))
    # normal stage dispatch
    ...
```

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `pipeline-config` feature | Hard | `docker_image` config field |
| `docker-build-env` feature (DiaEnv) | Hard | Image must exist before `--docker` can be used |
| `docker` | System tool | Docker Desktop running on host |

## Acceptance Criteria

1. `dia pipeline --docker --stage compile-code --config Debug --target googletest` runs the compile-code stage inside the container and exits with the container's exit code
2. All pipeline stages run identically inside the container as on the host
3. If the Docker image does not exist, exits 3 with "Docker image 'cluiche-build-env:latest' not found. Run `dia env docker image` to build it."
4. Container stdout/stderr streams to the host terminal in real time
5. `ANTHROPIC_API_KEY` is forwarded from the host environment into the container if present
6. `--force` and all other flags are forwarded correctly to the in-container invocation
7. `docker run --rm` is used — no containers left running after the command exits

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
| PD-005 | Platform | x64 Windows only | Compliant — Docker container is Windows x64 |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files modified |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — 0 success, 1 stage failure, 3 image not found |
| SD-PIPE-001 | DiaPipeline | `pipeline.toml` is single source of truth | Compliant — `docker_image` optional field in `[global]` |
| SD-PIPE-004 | DiaPipeline | `--docker` runs all stages inside container via volume-mounted repo | This feature implements that decision |
| SD-ENV-008 | DiaEnv | Docker scope is headless build + test only | Compliant — `--docker` is headless; no GPU/GUI stages |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |
| SD-TEST-001 | DiaTest | All tests run inside Docker | Compliant — `dia test env-integration` calls `dia pipeline --docker` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Volume mount path | The container uses `C:/repo` as the workdir — does this work for all path resolution inside the container? | Yes — `pipeline.toml` uses repo-relative paths. As long as `repo_root` in the container resolves to `C:/repo` and all paths in `pipeline.toml` are relative to it, all stage logic works unchanged. `path_resolver.py` uses the workdir as `repo_root` inside the container. |
| 2 | Docker Desktop required | Does the developer need Docker Desktop running to use `--docker`? | Yes. If Docker is not running, `docker image inspect` fails with a connection error; the stage exits 1 with Docker's error message. No special handling needed — Docker's error is clear enough. |
| 3 | `--rm` cleanup | Does `--rm` cause any state to be lost between runs? | No — all state (source code, deps, built binaries) lives in the volume-mounted repo. Nothing is persisted in the container itself. `--rm` is correct. |
