# Feature Spec: DiaConsole — Console Project Context

## Parent System
@docs/specs/applications/dia/systems/diaconsole/diaconsole.md

**Status:** `Approved`

---

## Summary

**Scope corrected from the system spec's original framing (see the system spec's Responsibilities/Out-of-Scope/SD-CONSOLE-007, amended 2026-09-29).** This feature builds target/config/platform context — reading `pipeline.toml`'s real targets and supplying them as defaults into command forms — not project-level (cross-repo/sibling) switching. Verified against the real codebase: there is no second project in this repo (`CoW` doesn't exist as a directory anywhere; only `CluicheEditor`, `CluicheGameBaseline`, `CluicheTest` are real sibling apps under `Cluiche/`), and no discovery mechanism for finding other project roots exists in DiaCLI. Building either a registry or a scanner for a project that doesn't exist would be unverifiable beyond "finds itself." Cross-project switching is parked, not built in any form, until both a real second project and a real discovery mechanism exist.

What's real and gets built: a top-bar target/config selector (replacing `console-native-shell`'s hardcoded placeholder text) backed by `pipeline.toml`, and a mechanism that fills matching form fields with the current target/config when the user switches, without overwriting a field the user has already edited by hand.

---

## Goals

1. `dia_console/context.py` exposes `list_targets(repo_root) -> list[TargetInfo]` via `pipeline_config.load_pipeline_config(repo_root)` — reused directly, not re-parsed independently (per the plan's existing Implementation Pattern, which this part of correctly holds up).
2. **One small additive change outside `dia_console/`, mirroring `console-typed-results`' `ExecutionHandle.log_tail()` precedent:** `pipeline_config.py`'s `TargetConfig` gains a `hidden: bool = False` field, parsed from `traw.get("hidden", False)`. Two targets in the real `pipeline.toml` (`diasfml`, `diauiultralight`) already set `hidden = true`, but the current loader silently drops that flag — it's dead TOML metadata today. This is a backward-compatible default-preserving addition (every existing caller of `load_pipeline_config` is unaffected), needed so DiaConsole's target dropdown doesn't surface two internal build-dependency targets that no other DiaCLI tooling exposes as user-facing choices.
3. `list_targets()` returns only non-hidden targets: `[{"name": "googletest", "appName": ...}, {"name": "cluichetest", ...}, {"name": "cluicheeditor", ...}]` for the current real `pipeline.toml` (excludes `diasfml`/`diauiultralight`).
4. `GET /api/context` (new route) returns `{"targets": [...], "configs": ["Debug", "Release"], "platform": "x64", "defaults": {"target": <global.default_target>, "config": <global.default_config>}}`. `configs` is a fixed two-item list (CLAUDE.md's own Configurations section — not derived from `pipeline.toml`, which has no such list). `platform` is a fixed single string, not a list — per PD-005 (x64 is the only supported platform) and confirmed by grepping every `dia_cli/cli/*.py`: no command has a `--platform` option at all, so there is nothing to select. It renders as a static label, never a dropdown.
5. The top bar's target/config segments become real `<select>`s (replacing `console-native-shell`'s hardcoded `cluichetest / Debug / x64` text), populated from `/api/context`, defaulting to `defaults.target`/`defaults.config`. The platform segment stays a static label.
6. On target/config change, `app.js` re-applies the new value into any currently-rendered form field whose descriptor `name` is exactly `"target"` or `"config"` (confirmed by grepping every `dia_cli/cli/*.py`: `run`/`launch`/`fix` all name their target argument `target`; `pipeline`/`run`/`launch`/`test`/`fix`/`check sanitizer` all name their config option `config` — a consistent naming convention across the real CLI, not a heuristic guess) — **but only if that field hasn't been manually edited by the user** (see Goal 7).
7. `app.js` tracks a per-field `touched` flag, set the first time a user directly interacts with an input (not set by a programmatic default-fill). Context-driven default refreshes only overwrite untouched fields; a field the user has typed into keeps its value across a target/config switch.
8. No project-level switching UI anywhere — no dropdown, no placeholder, no disabled control hinting at a feature that isn't real.

---

## Data Contracts

**New HTTP route:**
```
GET /api/context  →  {"targets": [{"name": str, "appName": str}, ...], "configs": ["Debug", "Release"], "platform": "x64", "defaults": {"target": str, "config": str}}
```

**`pipeline_config.py` addition (the one permitted change outside `dia_console/`):**
```python
@dataclass
class TargetConfig:
    project: str
    app_name: str = ""
    stages: list[str] = field(default_factory=list)
    deploy: DeployConfig = field(default_factory=DeployConfig)
    build_deps: BuildDepsConfig = field(default_factory=BuildDepsConfig)
    full_suite_config: str = "Debug"
    hidden: bool = False   # new — parsed from traw.get("hidden", False)
```

**`dia_console/context.py`:**
```python
@dataclass(frozen=True)
class TargetInfo:
    name: str
    app_name: str

def list_targets(repo_root: Path) -> list[TargetInfo]:
    """Non-hidden targets from pipeline.toml, via the existing loader — never re-parses the TOML itself."""
```

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | `pipeline_config.py` — add `TargetConfig.hidden: bool = False`, parse `traw.get("hidden", False)` in `load_pipeline_config` | The only change to this file. Existing tests for `load_pipeline_config` must still pass unmodified — this is additive with a safe default. |
| 2 | `dia_console/context.py` — `TargetInfo`, `list_targets(repo_root)` filtering `hidden` targets | Reuses `pipeline_config.load_pipeline_config` — does not reimplement TOML parsing. |
| 3 | `GET /api/context` in `dia_console/web/app.py` | Fixed `configs`/`platform` values per Goal 4; `targets`/`defaults` from `list_targets` + the loaded `PipelineConfig.global_cfg`. |
| 4 | `app.js` — target/config `<select>`s in the top bar, populated from `/api/context`, replacing the hardcoded breadcrumb text | Platform stays a static label — not a `<select>`, per Goal 4. |
| 5 | `app.js` — `touched` tracking on form fields (set on first real user `input`/`change`, not on a programmatic fill) | Extends the existing field-rendering code from `console-native-shell` (`renderNonFlagField`, etc.) — add the flag there, don't duplicate field-creation logic. |
| 6 | `app.js` — on target/config change, re-fill untouched fields named exactly `target`/`config` in the currently-rendered form (if any) | No-op if no command is selected, or if the selected command has neither field — most commands don't. |
| 7 | Test: `pipeline_config.py`'s `hidden` parsing — a target with `hidden = true`, a target without the key (defaults `False`), confirms existing (unaffected) targets still parse identically | Regression-safe addition. |
| 8 | Test: `list_targets()` against the real `pipeline.toml` — asserts `diasfml`/`diauiultralight` are excluded, `googletest`/`cluichetest`/`cluicheeditor` are present | Golden-list style, like `console-command-model`'s registry test — not a hardcoded count that breaks when a target is added. |
| 9 | Test: `GET /api/context` shape, via `TestClient` | Unit-level, no real subprocess. |
| 10 | Test: touched-field tracking — programmatic fill doesn't set `touched`; a real `input` event does; a context switch after a real edit does not overwrite that field, but does overwrite an untouched one | The core behavioral guarantee of this feature (Goal 7) — this is the one test that must not be faked. |

---

## Binding Decisions

| Decision | Implication |
|----------|--------------|
| SD-CONSOLE-007 (revised) — target/config context via `pipeline.toml` only, no sibling-project discovery in v1 | This feature builds exactly that; no project-switching UI exists anywhere in its scope. |
| PD-005 — x64 is the only supported platform | `platform` in `/api/context` is a fixed string, never a list; the UI renders it as a label, not a control. |
| SD-CONSOLE-005 — UI depends only on the model's interfaces | `app.js` only calls `/api/context`; it never reads `pipeline.toml` or touches `pipeline_config.py` directly. |

---

## Open Design Questions

1. **`hidden` semantics beyond DiaConsole.** Adding `TargetConfig.hidden` makes the field available to any future DiaCLI code (e.g. `dia pipeline --help`'s target list, if that's ever built to be TOML-driven) — this feature only *consumes* it for the console's dropdown, it doesn't retrofit any existing command to respect it. Worth a follow-up check whether `dia run`/`dia pipeline` should also stop suggesting `diasfml`/`diauiultralight` as valid `--target` values anywhere they currently do, but that's out of scope here.
