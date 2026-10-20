# Feature Spec: preserve-subfolder-deploy

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diacli/diacli.md | **preserve-subfolder-deploy** |

**Origin:** CoW F-3 World Assets (@docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md) needs world-sim assets to deploy with their subfolders intact so bundle-relative paths resolve the same way in dev and deployed builds. Logged as Dia need #17 in `docs/specs/applications/cow/dia-needs.md`.  
**Status:** `Approved`

---

## Problem Statement

The asset pipeline flattens deployed files to `global/<category>/` at line 50 of `layout.py`. CoW world-sim assets live under `Global/WorldGen/Baked/`, `Global/Weather/Packs/`, `Global/Ecology/Packs/`, etc. Flattening breaks bundle-relative paths like `"terrain": { "baked": "WorldGen/Baked/archipelago_s42.wld.json" }` (line 115 of `f3-worldassets.md`) because the deployed path becomes `global/misc/archipelago_s42.wld.json`, not `global/world/WorldGen/Baked/archipelago_s42.wld.json`.

F-3 expects records tagged `world` to preserve their source subfolder structure under `global/world/<path below Global>/`. A file at `Cluiche/Assets/CoW/Global/Ecology/Packs/lush.ecology.json` deploys to `$(OutDir)assets/global/world/Ecology/Packs/lush.ecology.json` so the `world` alias resolves `Ecology/Packs/lush.ecology.json` the same way in both dev and deployed builds.

---

## Solution Overview

`resolve_deploy_path` (line 30 of `layout.py`) checks if `"world"` is in the record's `tags`. If present, it computes the subfolder path below `Global/` from the `source_path` and deploys to `<deploy_root>/global/world/<subfolder>/<filename>` instead of applying category tag resolution.

The `world` alias is registered from `.diagame` `path_aliases` as `"world": "./global/world"` (F-3 line 172). CoW's `cow.diagame` already has `path_aliases` for `root` and `ui_common`; this adds a third entry.

### Author protection rule

F-3 Resolved Decision 3 (line 286): every world-sim asset written by the game carries `"author": "game"` in its catalogue record. The pipeline must not delete `author: game` records during `build-assets` or `deploy`. This protection belongs in the catalogue update logic (not in `layout.py`), wherever records are removed from the catalogue when source files are deleted.

---

## Design

### Deploy path changes

**File:** `Dia/DiaCLI/dia_cli/commands/asset/layout.py`

New function after `resolve_category`:

```python
def compute_world_subfolder(source_path: str) -> str:
    """Extract subfolder path below 'Global/' for world-tagged assets.
    
    Example: 'Global/Ecology/Packs/lush.ecology.json' -> 'Ecology/Packs'
    Returns empty string if 'Global/' not found or source_path is just a filename.
    """
    parts = Path(source_path).parts
    try:
        global_idx = next(i for i, p in enumerate(parts) if p == "Global")
        # Everything between Global/ and the filename
        subfolder_parts = parts[global_idx + 1:-1]
        return str(Path(*subfolder_parts)) if subfolder_parts else ""
    except (StopIteration, ValueError):
        return ""
```

Amend `resolve_deploy_path` (line 30):

```python
def resolve_deploy_path(record: dict, context: "BuildContext") -> Path:
    """Resolve the full output path for an asset from its scope and tags.

    Scope rule:
        global -> <deploy_root>/global/<category>/
        stage  -> <deploy_root>/stages/<stage_name>/<category>/
        
    world-tag override:
        If 'world' in tags, deploy to <deploy_root>/global/world/<subfolder>/
        preserving the path below 'Global/' in source_path.

    Folder assets (type 'folder') get a trailing slash appended (SD-CAT-013).
    """
    scope = record.get("scope", "global")
    tags: list[str] = record.get("tags", [])
    filename = Path(record.get("source_path", "")).name

    if "world" in tags:
        # Preserve subfolder structure for world-sim assets
        subfolder = compute_world_subfolder(record.get("source_path", ""))
        if subfolder:
            base = context.deploy_root / "global" / "world" / subfolder
        else:
            base = context.deploy_root / "global" / "world"
    elif scope == "stage":
        category = resolve_category(tags)
        stage_name = record.get("stage_name", "")
        base = context.deploy_root / "stages" / stage_name / category
    else:
        category = resolve_category(tags)
        base = context.deploy_root / "global" / category

    return base / filename
```

### Catalogue protection rule

**Investigation needed:** Catalogue records are read/written by `BuildContext` from `assets.catalogue.json`. The pipeline does not have a deletion phase today; records for missing source files accumulate. If F-3's `author: game` protection needs implementation, identify where records are pruned (if anywhere) and add an `if record.get("author") == "game": continue` guard.

**Current finding:** The catalogue is append-only during pipeline runs. Records with missing source files are not removed. The `author: game` protection is therefore **automatic** — no code change needed. This should be verified in F-3's Feature 3 test (a pipeline run between two `save_as` calls must preserve game-authored records).

If a future cleanup command is added (e.g., `dia asset catalogue prune`), it must skip `author: game` records.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | A record with `tags: ["world"]` and `source_path: "Global/Ecology/Packs/lush.ecology.json"` deploys to `global/world/Ecology/Packs/lush.ecology.json` | pytest with mock record |
| AC2 | A record with `tags: ["world", "misc"]` ignores the `misc` tag and uses `world` deploy path | pytest |
| AC3 | A record with `tags: ["misc"]` (no `world`) deploys to `global/misc/` as before | pytest (regression) |
| AC4 | Stage-scoped records are unaffected by `world` tag; they still deploy to `stages/<name>/<category>/` | pytest |
| AC5 | A `world`-tagged record whose `source_path` doesn't contain `Global/` deploys to `global/world/<filename>`, and `dia validate manifest` / `build-assets` log a Warning naming the record and suggesting a path under `Global/` (amended 2026-10-20, approval) | pytest edge case asserts both the path and the warning |
| AC6 | CoW's `cow.diagame` adds `"world": "./global/world"` to `path_aliases` and `PathStore` resolves it correctly at runtime | Manual: `dia launch cow`, log alias resolution |
| AC7 | `author: game` records are not removed by `dia pipeline --target cow` even if their source file is missing | pytest: add a record with `author: game`, delete its source, run pipeline, verify record survives |

---

## Test Strategy

**pytest** (`Dia/DiaCLI/tests/commands/asset/test_layout_world_deploy.py`):
- `test_world_tag_preserves_subfolders`: AC1, AC2.
- `test_world_tag_no_subfolder`: AC5.
- `test_non_world_tag_unchanged`: AC3 (regression).
- `test_stage_scope_ignores_world_tag`: AC4.

**pytest** (`Dia/DiaCLI/tests/commands/asset/test_catalogue_protection.py`):
- `test_author_game_survives_missing_source`: AC7.

**Manual:**
- Amend CoW's `cow.diagame` to add `world` alias.
- Add a test world-sim file to CoW's catalogue with `tags: ["world"]` and `source_path: "Global/Weather/test.json"`.
- Run `dia pipeline --target cow` and verify it deploys to `global/world/Weather/test.json`.

---

## Binding Decisions

| Source | Constraint |
|--------|------------|
| PD-009 | Authored files are source assets; generated output goes to `out/`. World-sim assets are authored, so they live under `Assets/` and deploy. |
| AD-001 CoW sign-off | This change (Dia-tier, signed off 2026-10-19) enables CoW F-3 bundle-relative paths. |
| F-3 Resolved Decision 1 | The `world` path alias resolves to `./global/world` at runtime. |
| F-3 Resolved Decision 3 | Game-authored records carry `author: game`; pipeline does not delete them. |

## Resolved Decisions (approved 2026-10-20)

1. **Stage scope wins over the `world` tag.** CoW has no stage-scoped world assets yet.
2. **A future `prune` command must skip `author: game` records.** This is recorded as a requirement for whatever spec adds pruning, and noted in F-3 Feature 4.
3. **A misplaced `world` file gets a warning, not silent flattening.** A `world`-tagged record outside `Global/` still deploys to `global/world/<filename>`, but validate and build-assets warn, so the mistake is noticed (AC5).

---

## Status

`Approved` (2026-10-20). No plan yet. Build sign-off (AD-001) given by the user 2026-10-20; not started (implementation deferred by the user). dia-needs #17 signed off under CoW AD-001 on 2026-10-19.
