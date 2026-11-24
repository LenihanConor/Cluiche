# Feature Spec: DiaConsole — Console Presets

## Parent System
@docs/specs/applications/dia/systems/diaconsole/diaconsole.md

**Status:** `Approved`

---

## Summary

Developers re-run the same command+argument combinations over and over (e.g. `test e2e --suite smoke`). Presets let them save a combination once, under a name, and re-run it by name from either the CLI (`dia preset run <id>`) or the console's nav tree — instead of re-filling the same form or re-typing the same flags every time. Presets are core v1 scope per SD-CONSOLE-006 and SD-CONSOLE-011, not deferred polish.

Two preset files exist per repo: a committed, team-shared `.dia/console-presets.yaml` and a gitignored, personal `.dia/console-presets.local.yaml`. Both are loaded and merged by `id`; the local file wins on collision.

---

## Goals

1. `dia_console/presets.py` defines `PresetDescriptor` (frozen dataclass): `id`, `name`, `command_id`, `values` (a flat `Mapping[str, Any]`, matching the system spec's YAML shape), `source` (`"shared"` or `"local"` — which file this preset currently lives in, needed so edit/delete write back to the right file).
2. `load_presets(repo_root) -> list[PresetDescriptor]` reads `.dia/console-presets.yaml` (create if absent — an empty `presets: []` is valid and normal on a fresh clone) and `.dia/console-presets.local.yaml` (absent is normal — most clones won't have one yet), merges by `id`, local overlay wins on collision (SD-CONSOLE-011). New dependency: `pyyaml` (not currently a DiaCLI dependency; `toml` is used for `pyproject.toml`/`pipeline.toml` only, not for this format — the system spec's own preset examples are YAML).
3. `save_preset(repo_root, preset: PresetDescriptor, *, scope: Literal["shared", "local"]) -> None` and `delete_preset(repo_root, preset_id: str, *, scope: Literal["shared", "local"]) -> None` read-modify-write the target file only (never touch the other file), replacing an existing entry with the same `id` in that file or appending a new one. Both are idempotent re-saves/deletes (re-running either produces the same end state).
4. `build_execute_request(descriptor: CommandDescriptor, values: Mapping[str, Any], project_id: str | None) -> ExecuteCommandRequest` splits a preset's flat `values` into `arguments`/`options` by checking each key against `descriptor.arguments`/`descriptor.options` names — the registry (not the preset file) is what knows which of a command's parameters are positional vs named, so the on-disk YAML never needs to encode that distinction.
5. `dia preset list` (new `dia_cli/cli/preset.py`) prints every merged preset's `id`, `name`, `command_id`, and `source`.
6. `dia preset run <id>` resolves the preset, looks up its `command_id` in a freshly-built `CommandRegistry.from_click_app(dia_cli_app)`, builds argv via `ExecutionService.build_argv` (reused, not reimplemented) against a synthetic `ExecuteCommandRequest` from `build_execute_request`, then runs it as a plain inherited-stdio subprocess (`subprocess.run(argv)`) — the same experience as if the user had typed the underlying command by hand. No `--log-json` override and no NDJSON tailing here: this is a CLI-only convenience wrapper, not a console-UI execution (SD-CONSOLE-009's subprocess constraint is about the console's execution engine; a bare CLI passthrough has no in-process UI to protect).
7. `GET /api/presets` (new route on the existing FastAPI app) returns the merged preset list as JSON, each with its `source`.
8. `POST /api/presets` ("save current") creates a new preset from the request body (`id`, `name`, `command_id`, `values` — the full effective argument/option snapshot from the currently-rendered form, defaults included, not just touched fields) and always writes to the **local** file (personal quick-save; sharing a preset with the team is a deliberate act of hand-editing or committing the shared YAML, not a UI default).
9. `PUT /api/presets/{id}` (rename/edit) and `DELETE /api/presets/{id}` operate on whichever file that preset's `source` says it currently lives in — editing a shared preset directly edits the committed file; editing a local one edits the local file.
10. Nav tree gets a dedicated "Presets" section, listed above the reflected command tree (per the earlier interview decision), populated from `GET /api/presets`. Clicking a preset selects its `command_id` and pre-fills the run form with `values` (marking every pre-filled field `touched = true`, since it's an explicit user choice, not a context default per `console-project-context`'s touched-field rule) — it does **not** auto-execute; the user still clicks Run.
11. `.dia/console-presets.local.yaml` is added to `.gitignore`. `.dia/console-presets.yaml` is committed (starts as `presets: []` if this feature ships before any real preset is saved).

---

## Data Contracts

**Preset file shape (already specified in the system spec's Public Interfaces — reproduced here as the contract this feature implements):**
```yaml
# .dia/console-presets.yaml — committed, team-shared
presets:
  - id: smoke-e2e
    name: E2E Smoke
    command: test.e2e
    values:
      suite: smoke

# .dia/console-presets.local.yaml — gitignored, personal
presets:
  - id: my-quick-run
    name: My Quick Run
    command: run
    values:
      target: cluichetest
      config: Debug
```

**`dia_console/presets.py`:**
```python
@dataclass(frozen=True)
class PresetDescriptor:
    id: str
    name: str
    command_id: str
    values: Mapping[str, Any]
    source: Literal["shared", "local"]

def load_presets(repo_root: Path) -> list[PresetDescriptor]: ...
def save_preset(repo_root: Path, preset: PresetDescriptor, *, scope: Literal["shared", "local"]) -> None: ...
def delete_preset(repo_root: Path, preset_id: str, *, scope: Literal["shared", "local"]) -> None: ...
def build_execute_request(
    descriptor: CommandDescriptor, values: Mapping[str, Any], project_id: str | None = None,
) -> ExecuteCommandRequest: ...
```

**New HTTP routes (`dia_console/web/app.py`):**
```
GET    /api/presets            -> [{"id", "name", "commandId", "values", "source"}, ...]
POST   /api/presets             {"id", "name", "commandId", "values"}      -> the created PresetDescriptor (source="local")
PUT    /api/presets/{id}        {"name"?, "values"?}                       -> the updated PresetDescriptor
DELETE /api/presets/{id}       -> 204 No Content
```

**New CLI (`dia_cli/cli/preset.py`):**
```bash
dia preset list
dia preset run <id>
```

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | `dia_console/presets.py` — `PresetDescriptor`, `load_presets`, merge-by-id with local overlay winning | Add `pyyaml` to `pyproject.toml`. Missing files degrade to an empty list for that source, never an error. |
| 2 | `dia_console/presets.py` — `save_preset`, `delete_preset` | Read-modify-write only the target file; preserves other entries in that file untouched. |
| 3 | `dia_console/presets.py` — `build_execute_request` | Splits flat `values` into `arguments`/`options` via the given `CommandDescriptor`'s param names; unknown keys (typo'd values) are dropped, not passed through as bogus argv. |
| 4 | `dia_cli/cli/preset.py` — `dia preset list` / `dia preset run <id>` | `run` reuses `ExecutionService.build_argv` for argv construction — no second argv-serialization implementation. |
| 5 | `GET/POST/PUT/DELETE /api/presets*` in `dia_console/web/app.py` | `POST` always writes `scope="local"`; `PUT`/`DELETE` use the target preset's own `source`. |
| 6 | `.gitignore` — add `.dia/console-presets.local.yaml`; commit an initial `.dia/console-presets.yaml` with `presets: []` | |
| 7 | `app.js` — "Presets" nav section above the command tree, populated from `/api/presets` | Clicking a preset selects its command and pre-fills the form via the existing field-rendering path from `console-native-shell`, marking each pre-filled field `touched` per Goal 10. |
| 8 | `app.js` — "Save current" action in the run form, "Edit"/"Delete" actions per preset in the nav | Save posts the form's full current value snapshot (all fields, not just touched ones) per Goal 8. |
| 9 | Test: `load_presets` merge-by-id, local overlay wins, missing-file tolerance | |
| 10 | Test: `save_preset`/`delete_preset` only mutate their target file | |
| 11 | Test: `build_execute_request` argument/option split, unknown-key drop | |
| 12 | Test: `dia preset run smoke-e2e` against a real repo-shared preset entry (integration-style, like `console-command-model`'s one real-subprocess test) | |
| 13 | Test: a user-local preset with a colliding `id` overrides the shared one end-to-end (`load_presets` → `dia preset run`) | This is the plan's stated acceptance test for this feature — must not be faked. |
| 14 | Test: `/api/presets` CRUD routes via `TestClient` | |

---

## Binding Decisions

| Decision | Implication |
|----------|--------------|
| SD-CONSOLE-006 — presets are a core v1 feature, not deferred polish | This feature ships list/run/save/edit/delete, not a stub. |
| SD-CONSOLE-011 — repo-shared + gitignored user-local overlay, local wins on collision | `load_presets`'s merge order and `PresetDescriptor.source` both exist specifically to implement this. |
| SD-CONSOLE-005 — UI depends only on the model's interfaces | `app.js` only calls `/api/presets*`; it never reads the YAML files or constructs argv itself. |
| SD-CONSOLE-009 — console execution is subprocess-only, never in-process | `dia preset run`'s plain `subprocess.run(argv)` and the web UI's "pre-fill, don't auto-run" behavior (Goal 10) both keep every actual command invocation on the existing subprocess path — this feature adds no new in-process call into a DiaCLI command. |

---

## Open Design Questions

1. **Should `dia preset run` stream through the same NDJSON/`ExecutionHandle` machinery as the console UI, for consistency, instead of a bare inherited-stdio `subprocess.run`?** Decided against for v1 (Goal 6) because there's no UI consumer for the CLI path to serve, but if a future feature wants `dia preset run --json` for scripting, revisit then rather than building it speculatively now.
2. **Collision between a preset's saved `values` and a command whose parameters changed since the preset was saved** (e.g. a renamed option). `build_execute_request` silently drops unknown keys (Task 3) rather than erroring — worth confirming that's the right failure mode versus a warning, once a real stale preset is hit in practice.
