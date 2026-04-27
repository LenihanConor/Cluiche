# Feature Spec: asset-build

## Parent System
@docs/specs/systems/dia/diapipeline.md

## Status
`Done`

## Summary

Implement `dia pipeline --stage asset-build` as a no-op stub. The stage reserves its position in the fixed stage order (proto-compile → compile-code → **asset-build** → package) and logs "asset-build: skipped (not yet implemented)". It exists so that: the stage surface is complete, the stage name can appear in `pipeline.toml` target configs, and no breaking change to stage ordering is required when the real asset pipeline is specced.

## Problem

The stage ordering decision (SD-PIPE-002) fixes four stages. Without a placeholder for `asset-build`, the stage slot would be absent from the current implementation, requiring a breaking change to the stage list (and every `pipeline.toml` config that references it) when the real asset pipeline arrives.

## Goals

- Register `asset-build` as a valid stage name accepted by the CLI
- Log a single line: "asset-build: skipped (not yet implemented)"
- Return exit 0 always
- `pipeline.toml` targets can include `"asset-build"` in their `stages` list without error

## Non-Goals

- Any real asset processing — that belongs to a future asset pipeline system
- Asset file watching, incremental builds, or dependency tracking

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    └── commands/
        └── pipeline/
            └── stages/
                └── asset_build_stage.py   # No-op stub
```

### `asset_build_stage.py`

```python
from loguru import logger
from pathlib import Path
from ..pipeline_config import PipelineConfig

def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path) -> int:
    logger.info("asset-build: skipped (not yet implemented)")
    return 0
```

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `pipeline-config` feature | Hard | Stage registered in runner's dispatch table |

## Acceptance Criteria

1. `dia pipeline --stage asset-build` exits 0 and prints "asset-build: skipped (not yet implemented)"
2. `dia pipeline` (all stages) runs `asset-build` in the correct position (after `compile-code`, before `package`) and does not fail
3. A `pipeline.toml` target that includes `"asset-build"` in its `stages` list does not produce a validation error

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
| PD-005 | Platform | x64 Windows only | Compliant — no platform-specific code; no-op |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files touched |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — always exits 0 |
| SD-PIPE-001 | DiaPipeline | `pipeline.toml` is single source of truth | Compliant — stage name accepted from `stages` list |
| SD-PIPE-002 | DiaPipeline | Stage ordering fixed | Compliant — stub reserves stage 3 position |
| SD-PIPE-005 | DiaPipeline | asset-build is no-op stub until asset pipeline is specced | This feature implements that decision |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Future replacement | When the real asset pipeline is specced, does this stub need to be removed? | Yes — the stub will be replaced in-place by the real implementation. The stage name, position, and handler signature remain the same; only the body changes. No breaking changes to `pipeline.toml` or the CLI surface. |
