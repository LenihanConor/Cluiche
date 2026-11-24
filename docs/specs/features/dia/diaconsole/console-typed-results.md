# Feature Spec: DiaConsole — Console Typed Results

## Parent System
@docs/specs/applications/dia/systems/diaconsole/diaconsole.md

**Status:** `Approved`

---

## Summary

Wire the Results tab (currently a static "No structured results yet" placeholder from `console-native-shell`) to real data: a small adapter registry (`dia_console/results.py`) that turns a completed execution's output into typed `ResultRecord`s, a new `GET /api/executions/{id}/results` endpoint, and `app.js` rendering severity-grouped cards + an artifacts line matching the mockup's Results-tab state. v1 ships exactly two adapters — a `test.googletest` adapter parsing the gtest XML report, and a generic fallback (exit code + tail of merged log output) that every other command falls back to — so no execution is ever left with only raw log text in the Results tab.

---

## Goals

1. `dia_console/results.py` exposes `build_results(command_id: str, handle: ExecutionHandle) -> list[ResultRecord]`, computed fresh on every call (no caching, consistent with the registry's own no-cache rule) — re-parsing a small XML file per request is negligible cost.
2. A `test.googletest` adapter parses `Cluiche/out/GoogleTests/last_run.xml` (the standard gtest XML report format: `<testsuites>` → `<testsuite>` → `<testcase>`, a `<failure>` child marking a failed case) into: one summary `TestResult` record (pass/fail totals, `severity=None` if all passed else `"error"`), one `TestResult` record per failing testcase (`title="{classname}.{name}"`, `summary=`the failure message, `severity="error"`), and one `Artifact` record pointing at the XML file if it exists.
3. **Known v1 limitation, not a bug to fix here:** the gtest XML path is fixed (`Cluiche/out/GoogleTests/last_run.xml`), not per-execution like the NDJSON log — two concurrent `test.googletest` executions would race on it. The adapter reads the file immediately when the execution's terminal event fires; this is correct for the common single-run case and is an accepted limitation, not a defect. No change is made to `dia_cli`'s `googletest_runner.py` to add a per-execution override — that would be scope creep into a different command's behavior for a race that requires deliberately overlapping two test runs from the console to hit.
4. A generic fallback adapter (`GenericResult`, used for every `command_id` without a specific adapter) reports `handle.returncode` and the tail of the merged log buffer (stderr is already merged into stdout at the execution layer — see `console-command-model`'s `execution.py` — so there is no separate stderr to report) plus an `Artifact` record for the NDJSON log path, if it exists on disk (per the coverage caveat, most commands never create one).
5. `GET /api/executions/{id}/results` (new route in `dia_console/web/app.py`) calls `build_results` and returns the list as JSON; 404 for an unknown `execution_id`, matching the existing pattern for `/events`/`/logs`/`/cancel`.
6. `app.js`'s Results tab renders real severity-grouped cards (matching `mockups/console.html`'s State B: a success/failure summary card, per-failure detail cards, an artifacts line with clickable paths) once the tab is selected for a completed execution, replacing the static empty state from `console-native-shell` only when real data exists — an in-progress or never-run execution still shows the empty state.
7. Every command produces *something* structured in the Results tab — never just raw log text — because the fallback adapter always applies when no specific adapter matches `command_id`.

---

## Data Contracts

**New HTTP route:**
```
GET /api/executions/{execution_id}/results  →  JSON array of ResultRecord
```
Reuses the existing `_handle_or_404` pattern already in `dia_console/web/app.py`. `ResultRecord` (from `dia_console/model.py`, already defined, unchanged by this feature):
```python
@dataclass(frozen=True)
class ResultRecord:
    kind: str             # "TestResult" | "Artifact" | "GenericResult" (only these three used by this feature's adapters)
    severity: str | None  # "error" | None (v1 adapters don't use "warning" — see Open Design Questions)
    title: str
    summary: str | None
    payload: Mapping[str, Any]
```

**Adapter registry shape (`dia_console/results.py`):**
```python
ResultAdapter = Callable[[ExecutionHandle], list[ResultRecord]]

_ADAPTERS: dict[str, ResultAdapter] = {
    "test.googletest": _googletest_adapter,
}

def build_results(command_id: str, handle: ExecutionHandle) -> list[ResultRecord]:
    adapter = _ADAPTERS.get(command_id, _generic_fallback_adapter)
    return adapter(handle)
```

**`test.googletest` adapter payload shapes:**
- Summary record: `payload={"passCount": int, "failCount": int, "totalCount": int}`.
- Per-failure record: `payload={"suite": str, "testcase": str, "durationSeconds": float, "file": str | None, "line": int | None}` (file/line parsed from the `<failure>` message text if present in the standard `file:line:` prefix gtest emits — `None` if not parseable, never a crash).
- Artifact record: `title="gtest-results.xml"`, `payload={"path": str}` (repo-relative if under the repo root, else absolute).

**Generic fallback adapter payload shape:** `payload={"returncode": int | None, "logTail": list[str]}` (last ~20 lines of the merged log buffer — `handle`'s already-drained `_log_buffer`, or an equivalent public accessor if one needs to be added — see Constraints).

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | `dia_console/results.py` — `build_results()`, `_ADAPTERS` registry, `_generic_fallback_adapter` | Fallback must never raise for any `command_id`/handle state — a command with no output at all still gets a `GenericResult` with `returncode` and an empty `logTail`. |
| 2 | `_googletest_adapter` — parse `Cluiche/out/GoogleTests/last_run.xml` via `xml.etree.ElementTree` | If the file doesn't exist or fails to parse, fall through to the generic fallback's behavior for this execution rather than raising — a `test.googletest` run that crashed before writing XML still needs *some* result. |
| 3 | `GET /api/executions/{execution_id}/results` in `dia_console/web/app.py` | Same 404-on-unknown-id pattern as the three existing execution routes. |
| 4 | `app.js` — Results tab rendering: fetch `/api/executions/{id}/results` when the Results tab is clicked (or automatically once `events()`'s terminal event fires, mirroring how the Log tab already auto-populates), render cards grouped by `severity` (error cards styled `.card.err`, everything else styled `.card` per the mockup), artifacts line from any `kind == "Artifact"` records | Keep the existing static empty-state markup as the fallback shown before any execution has run. |
| 5 | Test: `test.googletest` adapter against a real gtest XML fixture (both an all-pass and a with-failures fixture, written as test fixtures — do not require a real `GoogleTests.exe` build) | Fixture-based, not a real subprocess run — this feature is about parsing, not re-proving `console-command-model`'s subprocess mechanics. |
| 6 | Test: generic fallback adapter for an arbitrary `command_id` with no specific adapter, using a fake `ExecutionHandle` | Confirms it never raises and always returns at least one `GenericResult`. |
| 7 | Test: `GET /api/executions/{id}/results` end-to-end via FastAPI's `TestClient`, including the 404 case | Unit-level, using the app's existing fake-service test pattern from `console-native-shell`'s test suite — no real subprocess needed for this route test. |
| 8 | Test (`@pytest.mark.integration`): a real `dia test googletest` execution through the full stack, confirming `/results` returns real `TestResult` records matching the actual XML the run produced | The one test in this feature that touches a real subprocess and a real build artifact. |

---

## Binding Decisions

| Decision | Implication |
|----------|--------------|
| System spec's `console-typed-results` responsibility — "classify command output into structured ResultRecords... instead of leaving the user to read raw log text" | The generic fallback adapter is what guarantees this for every command without a specific adapter (Goal 7). |
| SD-CONSOLE-005 — UI depends only on the model's interfaces | `app.js` only calls `GET /api/executions/{id}/results`; it never parses XML or touches `dia_console.results` directly — only the FastAPI layer does. |
| SD-CONSOLE-010 — no persistent cache (originally scoped to the command registry) | Extended here by choice, not by binding force: `build_results` recomputes on every call rather than caching on the handle, for the same reason (avoids an invalidation question that doesn't need to exist yet). |

---

## Open Design Questions

1. **No "warning" severity used in v1.** `googletest_runner.py`'s own `_warn_untagged_slow_suites` prints "WARNING: N suite(s) exceeded 500ms..." to stdout, which is visible in the Log tab already, but this feature's adapter doesn't turn it into a `ResultRecord` with `severity="warning"` — doing so would mean grep-parsing free-text log lines rather than structured XML, which is a different (and lower-confidence) parsing strategy than the rest of this adapter. Left as a natural future enhancement rather than built now, to keep this adapter's parsing strategy uniform (XML-only).
2. **`ExecutionHandle`'s log buffer may need a public accessor.** The generic fallback adapter's `logTail` needs the last ~20 lines of the process's merged stdout/stderr — `ExecutionHandle` currently exposes this only via the internal `_log_buffer` list (built for `log_lines()`'s own iteration). If there's no already-public way to read a snapshot of it, task 1 needs to add a small public accessor (e.g. `handle.log_tail(n: int) -> list[str]`) to `dia_console/execution.py` — this is the one place this feature may need to touch the execution layer, and should be a minimal additive accessor, not a behavior change, if needed.
